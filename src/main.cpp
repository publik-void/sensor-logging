#include <iostream>
#include <ios>
#include <iomanip>
#include <optional>
#include <tuple>
#include <array>
#include <cstdint>
#include <cinttypes>
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <ratio>

// TODO: Replace `#include` directives with `import` statements. On Debian
// Bullseye, which is still the basis for the current Raspberry Pi OS at the
// time of writing this comment, the GCC version is too old to support the use
// of modules. I could perhaps rely on backports or clang, but I think I won't
// mess around with those options for now. but maybe I'll update to Debian
// Bookworm when it is possible, which surely comes with a newer GCC version.

extern "C" {
  #include <pigpiod_if2.h>
  #include "DHTXXD.h"
  #include "_433D.h"
}

#include "machine.cpp"

namespace cc { // compile-time constants
  #ifdef NDEBUG
    bool constexpr ndebug{true};
  #else
    bool constexpr ndebug{false};
  #endif

  bool constexpr log_errors{!ndebug};
  char constexpr const * const log_info_string{"INFO"};
  char constexpr const * const log_error_string{"ERROR"};

  std::chrono::milliseconds constexpr sampling_interval{300};
  unsigned constexpr samples_per_aggregate{5u};
  unsigned constexpr aggregates_per_run{600u};

  using timestamp_duration_t = std::chrono::milliseconds;
  int constexpr timestamp_width{10};
  int constexpr timestamp_decimals{2};

  int constexpr field_decimals_default{6};

  char constexpr const * const csv_delimiter_string{", "};
  char constexpr const * const csv_false_string{"0"};
  char constexpr const * const csv_true_string{"1"};

  int constexpr exit_code_success{0};
  int constexpr exit_code_error{1};
  int constexpr exit_code_interrupt{130};
}

std::string log_info_prefix;
std::string log_error_prefix;

template<typename T>
struct CSVWrapper {
  T const &x;
};

template<typename T>
std::ostream& operator<<(std::ostream& out, CSVWrapper<T> const &x) {
  return out << x.x;
}
template<typename T>
std::ostream& operator<<(std::ostream& out,
    CSVWrapper<std::optional<T>> const &x) {
  return x.x.has_value() ? (out << CSVWrapper{x.x.value()})
                         : out << ""; // Still need to call `<<` e.g. for `setw`
}
std::ostream& operator<<(std::ostream& out, CSVWrapper<bool> const &x) {
  return out << (x.x ? cc::csv_true_string : cc::csv_false_string);
}
template<class Rep, std::intmax_t Num, std::intmax_t Denom>
std::ostream& operator<<(std::ostream& out,
    CSVWrapper<std::chrono::duration<Rep, std::ratio<Num, Denom>>> const &x) {
  auto const original_width{out.width()};
  auto const original_flags{out.flags()};
  auto seconds{std::imaxdiv(x.x.count() * Num, Denom)};
  out << std::dec << std::setw(cc::timestamp_width) << seconds.quot;
  if constexpr (cc::timestamp_decimals > 0) {
    auto const original_fill{out.fill()};
    auto constexpr timestamp_den{static_cast<std::intmax_t>(
      std::pow(10, cc::timestamp_decimals))};
    auto const fractional{
      std::imaxdiv(seconds.rem * timestamp_den, Denom).quot};
    out << "." << std::setw(cc::timestamp_decimals) << std::setfill('0')
      << fractional;
    out.fill(original_fill);
  }
  out.width(original_width);
  out.flags(original_flags);
  return out;
}


// NOTE: `optional_apply` could perhaps be defined as a variadic template
// and function, but let's keep it simple for now. C++23 also has something like
// this, but I'm on C++20 for now.
template<class F, class T>
std::optional<T> optional_apply(F f, std::optional<T> const &x0) {
  if (x0.has_value()) return std::optional<T>{f(x0.value())};
  else return std::optional<T>{};
}
template<class F, class T>
std::optional<T> optional_apply(F f,
    std::optional<T> const &x0, std::optional<T> const &x1) {
  if (x0.has_value()) {
    if (x1.has_value()) return std::optional<T>{f(x0.value(), x1.value())};
    else return std::optional<T>{x0};
  } else {
    if (x1.has_value()) return std::optional<T>{x1};
    else return std::optional<T>{};
  }
}

struct Pi {
  int const handle;
  operator int() const { return this->handle; }

  Pi(Pi const &) = delete;

  Pi(char const *addr = nullptr, char const *port = nullptr) :
      handle{pigpio_start(addr, port)} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
        char const *port_env{std::getenv("PIGPIO_PORT")};
        std::cerr << log_error_prefix
          << "connecting to pigpio daemon at "
          << "address " << (addr ? addr : "localhost")
          << ", port " << (port ? port : (port_env ? port_env : "8888"))
          << ": " << pigpio_error(this->handle) << std::endl;
    }
    //std::cout << "Pi constructed" << std::endl;
  }

  ~Pi() {
    if (this->handle >= 0) pigpio_stop(this->handle);
    //std::cout << "Pi destructed" << std::endl;
  }
};

struct I2C {
  int const handle;
  int const pi_handle;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  I2C(I2C const &) = delete;

  I2C(int const pi_handle, unsigned const bus,
      unsigned const addr, unsigned const flags = 0u) : 
      handle{i2c_open(pi_handle, bus, addr, flags)}, pi_handle{pi_handle} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
      auto const original_flags{std::cerr.flags()};
      std::cerr << log_error_prefix
        << "opening I2C device on "
        << "Pi " << pi_handle
        << std::hex << std::showbase
        << ", bus " << bus
        << ", address " << addr
        << ", flags " << flags
        << ": " << pigpio_error(this->handle) << std::endl;
      std::cerr.flags(original_flags);
    }
    //std::cout << "I2C constructed" << std::endl;
  }

  ~I2C() {
    if (this->handle >= 0) {
      int const response{i2c_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix << "closing I2C device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "I2C destructed" << std::endl;
  }
};

struct Serial {
  int const handle;
  int const pi_handle;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  Serial(Serial const &) = delete;

  Serial(int const pi_handle, char * const tty, unsigned const baud_rate,
      unsigned const flags = 0u) : 
      handle{serial_open(pi_handle, tty, baud_rate, flags)},
      pi_handle{pi_handle} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
      std::cerr << log_error_prefix
        << "opening serial device on "
        << "Pi " << pi_handle
        << ", file " << tty
        << ", baud rate " << baud_rate
        << ", flags " << flags
        << ": " << pigpio_error(this->handle) << std::endl;
    }
    //std::cout << "Serial constructed" << std::endl;
  }

  ~Serial() {
    if (this->handle >= 0) {
      int const response{serial_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix << "closing serial device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "Serial destructed" << std::endl;
  }
};

struct DHT {
  DHTXXD_t * const dht;
  operator DHTXXD_t * () const { return this->dht; }

  DHT(DHT const &) = delete;

  DHT(int const pi_handle, int const gpio_index, int const dht_model = DHTAUTO,
      DHTXXD_CB_t callback = nullptr) :
      dht{DHTXXD(pi_handle, gpio_index, dht_model, callback)} {
    //std::cout << "DHT constructed" << std::endl;
  }

  ~DHT() {
    DHTXXD_cancel(this->dht);
    //std::cout << "DHT destructed" << std::endl;
  }
};

struct LDP433Receiver {
  _433D_rx_t * const ldp433_receiver;
  operator _433D_rx_t * () const { return this->ldp433_receiver; }

  LDP433Receiver(LDP433Receiver const &) = delete;

  LDP433Receiver(int const pi_handle, int const gpio_index,
      _433D_rx_CB_t callback = nullptr) :
      ldp433_receiver{_433D_rx(pi_handle, gpio_index, callback)} {
    //std::cout << "LDP433Receiver constructed" << std::endl;
  }

  ~LDP433Receiver() {
    _433D_rx_cancel(this->ldp433_receiver);
    //std::cout << "LDP433Receiver destructed" << std::endl;
  }
};

struct LDP433Transmitter {
  _433D_tx_t * const ldp433_transmitter;
  operator _433D_tx_t * () const { return this->ldp433_transmitter; }

  LDP433Transmitter(LDP433Transmitter const &) = delete;

  LDP433Transmitter(int const pi_handle, int const gpio_index) :
      ldp433_transmitter{_433D_tx(pi_handle, gpio_index)} {
    //std::cout << "LDP433Transmitter constructed" << std::endl;
  }

  ~LDP433Transmitter() {
    _433D_tx_cancel(this->ldp433_transmitter);
    //std::cout << "LDP433Transmitter destructed" << std::endl;
  }
};

template<typename T>
bool errored(T const &handle_container) { return handle_container.handle < 0; }

auto create_i2c_reader(int const pi_handle, int const i2c_handle) {
  return [=](unsigned const reg){
    int const response{i2c_read_byte_data(pi_handle, i2c_handle, reg)};
    if (response < 0) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "reading from "
        << "Pi " << pi_handle
        << ", I2C " << i2c_handle
        << ", register " << reg
        << ": " << pigpio_error(response) << std::endl;
      return std::optional<std::uint8_t>{};
    } else {
      return std::optional<std::uint8_t>{response & std::uint8_t{0xffu}};
    }
  };
}

template<typename T0, typename T1 = std::uint8_t>
bool get_flag(T0 const &status, T1 const &flag = 0xffu){
  return (status & flag) != 0u;
}
template<typename T0, typename T1 = std::uint8_t>
std::optional<bool> get_flag(
    std::optional<T0> const &status, T1 const &flag = 0xffu){
  return (status.has_value())
    ? std::optional<bool>{get_flag(status.value(), flag)}
    : std::optional<bool>{};
};

// Empties all bytes buffered for reading from the given serial port
int serial_flush(Serial const &serial) {
  int const response{serial_data_available(serial.pi_handle, serial)};
  if (response < 0) return response; // TODO: Error reporting to stderr
  for (int i{0}; i < response; ++i) {
    int const response{serial_read_byte(serial.pi_handle, serial)};
    if (response < 0) return response; // TODO: Error reporting to stderr
  }
  return response;
}

// Checks periodically until requested byte count is available and then reads
template<class Rep0, class Period0, class Rep1, class Period1>
int serial_wait_read(Serial const &serial, char * const buf,
    unsigned const count,
    std::chrono::duration<Rep0, Period0> const timeout =
      std::chrono::milliseconds{100},
    std::chrono::duration<Rep1, Period1> const interval =
      std::chrono::milliseconds{10}) {
  std::size_t const
    max_intervals_to_wait{static_cast<std::size_t>(timeout / interval)};
  for (std::size_t i{0}; i < max_intervals_to_wait; ++i) {
    int const response{serial_data_available(serial.pi_handle, serial)};
    if (response < 0) return response;
    if (response >= static_cast<int>(count))
      return serial_read(serial.pi_handle, serial, buf, count);
    std::this_thread::sleep_for(interval);
  }
  return 0;
}

template<typename T>
char mhz19_checksum(T const packet) {
  char checksum{0x00};
  for(std::size_t i{1}; i < 8; ++i) checksum += packet[i];
  return (0xff - checksum) + 0x01;
}

template<typename T>
int mhz19_send(Serial const &serial, T const packet) {
  std::array<char, 9> buf{{packet[0], packet[1], packet[2], packet[3],
    packet[4], packet[5], packet[6], packet[7], mhz19_checksum(packet)}};
  int const response{serial_write(serial.pi_handle, serial, buf.data(), 9u)};
  if constexpr (cc::log_errors) if (response < 0) std::cerr
    << log_error_prefix << "sending packet to MH-Z19 on "
    << "Pi " << serial.pi_handle
    << ", serial " << serial.handle
    << ": " << pigpio_error(response) << std::endl;
  return response;
}

std::optional<std::array<std::uint8_t, 8>> mhz19_receive(
    Serial const &serial) {
  std::chrono::milliseconds constexpr timeout{100};
  std::chrono::milliseconds constexpr interval{10};

  std::array<char, 9> buf{};
  int const response{
    serial_wait_read(serial, buf.data(), 9u, timeout, interval)};
  if (response < 9) {
    if constexpr (cc::log_errors) {
      std::cerr << log_error_prefix
        << "receiving packet from MH-Z19 on "
        << "Pi " << serial.pi_handle
        << ", serial " << serial.handle
        << ": ";
      if (response < 0) std::cerr << pigpio_error(response);
      else std::cerr << "expected to read 9 bytes, got " << response;
      std::cerr << std::endl;
    }
    return {};
  }

  // If the checksum doesn't match, don't output an error
  if (buf[8] != mhz19_checksum(buf)) return {};

  std::array<std::uint8_t, 8> packet{{buf[0], buf[1], buf[2], buf[3], buf[4],
    buf[5], buf[6], buf[7]}};
  return {packet};
}

#include "sensors.cpp"

namespace sensors {
  // Sampling functions, written here manually, because machine-generating them
  // would involve a lot of complexity for little gain at my scale of operation.

  // One design choice to be made here is how to handle the aggregation of time
  // values if the dependent variables are missing. For a sensor like the DHT22,
  // which has two dependent variables total which always fail together, it
  // would make sense to not include the time points of failed readings, as
  // taking the mean of the time points that are left would provide a better
  // precision for the actual time of the aggregated reading. Meanwhile, for
  // something like the SensorHub, where it's common to have most readings not
  // fail and only some occasionally fail, I would have to provide an
  // aggregated time point for each individual reading to achieve the same
  // precision. So I think I'll make this choice for each sampling function
  // individually, depending on whether all readings always fail in sync or not.

  template<typename Clock>
  sensor sample_sensor(Clock &clock) {
    // NOTE: The clock determines the epoch start. I have to make sure to use
    // this with a clock that uses the unix time epoch.
    auto const timestamp{std::chrono::duration_cast<cc::timestamp_duration_t>(
      clock.now().time_since_epoch())};
    return sensor{std::optional{timestamp}};
  }

  template<typename Clock>
  sensorhub sample_sensorhub(Clock &clock, I2C const &i2c) {
    unsigned constexpr reg_ntc_temperature   {0x01u};
    unsigned constexpr reg_brightness_0      {0x02u};
    unsigned constexpr reg_brightness_1      {0x03u};
    unsigned constexpr reg_status            {0x04u};
    unsigned constexpr reg_dht11_temperature {0x05u};
    unsigned constexpr reg_dht11_humidity    {0x06u};
    unsigned constexpr reg_dht11_error       {0x07u};
    unsigned constexpr reg_bmp280_temperature{0x08u};
    unsigned constexpr reg_bmp280_pressure_0 {0x09u};
    unsigned constexpr reg_bmp280_pressure_1 {0x0au};
    unsigned constexpr reg_bmp280_pressure_2 {0x0bu};
    unsigned constexpr reg_bmp280_error      {0x0cu};
    unsigned constexpr reg_motion            {0x0du};

    unsigned constexpr flag_ntc_overrange       {0x01u};
    unsigned constexpr flag_ntc_error           {0x02u};
    unsigned constexpr flag_brightness_overrange{0x04u};
    unsigned constexpr flag_brightness_error    {0x08u};

    auto const read{create_i2c_reader(i2c.pi_handle, i2c)};

    auto const status{read(reg_status)};

    std::optional<bool> const ntc_overrange{
      get_flag(status, flag_ntc_overrange)};
    std::optional<bool> const ntc_error{
      get_flag(status, flag_ntc_error)};
    std::optional<float> const ntc_temperature{[&](){
      std::optional<float> const ntc_temperature{read(reg_ntc_temperature)};
      return (!ntc_overrange.value_or(true) and !ntc_error.value_or(true))
        ? ntc_temperature : std::optional<float>{};
    }()};

    std::optional<bool> const dht11_error{get_flag(read(reg_dht11_error))};
    std::optional<float> const dht11_temperature{[&](){
      std::optional<float> const dht11_temperature{read(reg_dht11_temperature)};
      return (!dht11_error.value_or(true)) ? dht11_temperature
                                            : std::optional<float>{};
    }()};
    std::optional<float> const dht11_humidity{[&](){
      std::optional<float> const dht11_humidity{read(reg_dht11_humidity)};
      return (!dht11_error.value_or(true)) ? dht11_humidity
                                            : std::optional<float>{};
    }()};

    std::optional<bool> const bmp280_error{get_flag(read(reg_bmp280_error))};
    std::optional<float> const bmp280_temperature{[&](){
      std::optional<float> const bmp280_temperature{
        read(reg_bmp280_temperature)};
      return (!bmp280_error.value_or(true)) ? bmp280_temperature
                                            : std::optional<float>{};
    }()};
    std::optional<float> const bmp280_pressure{[&](){
      auto const bmp280_pressure_0{read(reg_bmp280_pressure_0)};
      auto const bmp280_pressure_1{read(reg_bmp280_pressure_1)};
      auto const bmp280_pressure_2{read(reg_bmp280_pressure_2)};
      if (bmp280_pressure_0.has_value() and
          bmp280_pressure_1.has_value() and
          bmp280_pressure_2.has_value() and
          !bmp280_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(bmp280_pressure_0.value()) <<  0u |
          static_cast<std::uint32_t>(bmp280_pressure_1.value()) <<  8u |
          static_cast<std::uint32_t>(bmp280_pressure_2.value()) << 16u};
      else return std::optional<float>{};
    }()};

    std::optional<bool> const brightness_overrange{
      get_flag(status, flag_brightness_overrange)};
    std::optional<bool> const brightness_error{
      get_flag(status, flag_brightness_error)};
    std::optional<float> const brightness{[&](){
      auto const brightness_0{read(reg_brightness_0)};
      auto const brightness_1{read(reg_brightness_1)};
      if (brightness_0.has_value() and
          brightness_1.has_value() and
          !brightness_overrange.value_or(true) and
          !brightness_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(brightness_0.value()) << 0u |
          static_cast<std::uint32_t>(brightness_1.value()) << 8u};
      else return std::optional<float>{};
    }()};

    std::optional<float> const motion{optional_apply([](auto const x){
        return (x == 1) ? 1.f : 0.f;
      }, read(reg_motion))};

    return sensorhub{sample_sensor(clock),
      ntc_temperature,
      ntc_overrange,
      ntc_error,
      dht11_temperature,
      dht11_humidity,
      dht11_error,
      bmp280_temperature,
      bmp280_pressure,
      bmp280_error,
      brightness,
      brightness_overrange,
      brightness_error,
      motion};
  }

  template<typename Clock>
  dht22 sample_dht22(Clock &clock, DHT const &dht) {
    // The DHTXXD library provides two ways of reading data:
    // * Read periodically in separate thread and set a flag to indicate that
    //   new data are available. This flag is then reset when the data are
    //   retrieved.
    // * Read manually. In this case the flag (accessed through `DHTXXD_ready`)
    //   can be ignored. The comments in the library source files suggest not
    //   reading a DHT22 more often than every 3 seconds and a DHT11 not more
    //   often than every second. If a manual read is attempted too early, the
    //   status field of the data will indicate a timeout, and otherwise nothing
    //   bad should happen, except that the current thread may block for a short
    //   amount of time.
    // I am choosing to go with the second option here. This means I need to
    // manage the frequency with which this function is called myself.

    DHTXXD_manual_read(dht);
    auto const data{DHTXXD_data(dht)};

    return (data.status == DHT_GOOD)
      ? dht22{sample_sensor(clock), {data.temperature}, {data.humidity}}
      : dht22{};
  }

  template<typename Clock>
  mhz19 sample_mhz19(Clock &clock, Serial const &serial) {
    // Resources on this sensor (MH-Z19, MH-Z19B, MH-Z19C):
    // https://revspace.nl/MHZ19
    // https://habr.com/ru/articles/401363/
    //
    // Regarding the calibration of this sensor: As the documentation is so
    // spread out among bad data sheets and experiments recorded in blog posts
    // and comments, for several different models, it's kinda hard to get a
    // solid idea of how I best do this.
    // First of all, it seems that the sensor does an automatic calibration
    // every 24h, which relies on an internal value that tracks something like
    // the minimum CO2 concentration and can be found in the 6th and maybe 7th
    // byte of the response to the standard read command (0x86).
    // There is a "zero point" and a "span" which can be calibrated. Calibration
    // can be triggered manually and the automatic calibration (also in some
    // places referred to as ABC, automatic baseline correction) can be turned
    // off.
    // There are critiques of the automatic calibration, which may lead to hard
    // jumps in the readings, but it seems this is especially the case in the
    // first couple days of operation and perhaps even more so if the sensor is
    // operated at 3.3V instead of 5V. Otherwise, maybe it's not that bad,
    // especially if the sensor is regularly exposed to 400ppm.
    // I don't fully understand the manual calibration, especially in regards to
    // what conditions need to be present when performing it. It looks like
    // there is a possibility of ruining the readings more or less forever if
    // doing this wrong.
    // So I guess I'll go with the "ABC" for now, as my sensors should be
    // exposed to atmospheric conditions at least semi-regularly. And then I'll
    // record the special value "U" in case it helps with correcting some issues
    // during data analysis.
    //
    // The detection range can be set to 1000, 2000, 3000, and 5000ppm. I reckon
    // a higher range comes with lower accuracy? As I don't know whether setting
    // this triggers any other kind of resets, I should probably do it
    // infrequently.

    char constexpr byte_start{0xff};
    char constexpr byte_sensor_number{0x01};

    std::array<char, 8> constexpr cmd_read{{byte_start, byte_sensor_number,
      0x86, 0x00, 0x00, 0x00, 0x00, 0x00}};

    serial_flush(serial);
    int const response{mhz19_send(serial, cmd_read)};
    if (response >= 0) {
      auto const maybe_packet{mhz19_receive(serial)};
      if (maybe_packet.has_value()) {
        auto const packet{maybe_packet.value()};
        float const co2_concentration{static_cast<float>(
          static_cast<std::uint32_t>(packet[2]) << 8u |
          static_cast<std::uint32_t>(packet[3]) << 0u)};
        float const temperature{packet[4] - 40.f};
        int const status{packet[5]};
        int const u0{packet[6]}, u1{packet[7]};
        return mhz19{sample_sensor(clock), {co2_concentration}, {temperature},
          status == 0 ? std::optional<int>{} : std::optional<int>{status},
          {u0}, {u1}};
      }
    }
    return mhz19{};
  }
}

// Unnamed namespace for internal linkage
// As far as I can tell, this basically makes definitions private to the
// translation unit and is preferred over static variables.
namespace {
  std::atomic_bool quit_early{false};
  std::condition_variable cv;
  std::mutex cv_mutex;
  void graceful_exit(int const = 0) { quit_early = true; cv.notify_all(); }
}

int main(int const, char const * const argv[]) {
  std::signal(SIGINT , graceful_exit);
  std::signal(SIGTERM, graceful_exit);

  log_info_prefix =
    std::string(argv[0]) + ": " + cc::log_info_string + ": ";
  log_error_prefix =
    std::string(argv[0]) + ": " + cc::log_error_string + ": ";
  if constexpr (cc::log_errors) std::cerr << log_info_prefix
    << "Error logging to stderr enabled."
      " This probably means that this is not a release build." << std::endl;

  Pi const pi{};
  if (errored(pi)) return cc::exit_code_error;

  I2C const i2c{pi, 0x1u, 0x17u};
  if (errored(i2c)) return cc::exit_code_error;

  DHT const dht{pi, 4, DHTXX};

  Serial const serial{pi, const_cast<char *>("/dev/serial0"), 9600u};
  if (errored(serial)) return cc::exit_code_error;

  std::chrono::high_resolution_clock const clock{};

  auto const time_reference{clock.now()};

  sensors::write_csv_field_names(std::cout, sensors::sensorhub{});
  sensors::write_csv_field_names(std::cout, sensors::dht22{});
  sensors::write_csv_field_names(std::cout, sensors::mhz19{});

  for (unsigned aggregate_index{0u};
      aggregate_index < cc::aggregates_per_run;
      ++aggregate_index) {

    sensors::sensorhub a0{};
    sensors::sensorhub_state s0{};

    sensors::dht22 a1{};
    sensors::dht22_state s1{};

    sensors::mhz19 a2{};
    sensors::mhz19_state s2{};

    for (unsigned sample_index{0u};
        sample_index < cc::samples_per_aggregate;
        ++sample_index) {

      if (quit_early) return cc::exit_code_interrupt;

      //std::cout
      //  << "Aggregate index " << aggregate_index
      //  << ", sample index " << sample_index
      //  << std::endl;

      auto const x0{sensors::sample_sensorhub(clock, i2c)};
      std::tie(a0, s0) = aggregation_step(a0, s0, x0);

      auto const x1{sensors::sample_dht22(clock, dht)};
      std::tie(a1, s1) = aggregation_step(a1, s1, x1);

      auto const x2{sensors::sample_mhz19(clock, serial)};
      std::tie(a2, s2) = aggregation_step(a2, s2, x2);

      if ((sample_index + 1u) == cc::samples_per_aggregate) {
        a0 = aggregation_finish(a0, s0);
        sensors::write_csv_fields(std::cout, a0);

        a1 = aggregation_finish(a1, s1);
        sensors::write_csv_fields(std::cout, a1);

        a2 = aggregation_finish(a2, s2);
        sensors::write_csv_fields(std::cout, a2);
      }

      auto const next_time_point{time_reference + cc::sampling_interval *
        (aggregate_index * cc::samples_per_aggregate + sample_index + 1u)};
      std::unique_lock<std::mutex> lk(cv_mutex);
      cv.wait_until(lk, next_time_point);
    }
  }

  return cc::exit_code_success;
}
