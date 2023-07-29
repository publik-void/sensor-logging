#include <filesystem>
#include <iostream>
#include <fstream>
#include <ios>
#include <iomanip>
#include <string>
#include <string_view>
#include <utility>
#include <optional>
#include <variant>
#include <tuple>
#include <array>
#include <vector>
#include <ranges>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cinttypes>
#include <csignal>
#include <cstdlib>
#include <limits>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <ctime>
#include <chrono>
#include <ratio>
#include <type_traits>

// TODO: Replace `#include` directives with `import` statements. On Debian
// Bullseye, which is still the basis for the current Raspberry Pi OS at the
// time of writing this comment, the GCC (and standard Clang) version is too old
// to support the use of modules. I could perhaps rely on backports or clang,
// but I think I won't mess around with those options for now. but maybe I'll
// update to Debian Bookworm when it is possible, which surely comes with a
// newer GCC version.

extern "C" {
  #include <pigpiod_if2.h>
  #include "DHTXXD.h"
  #include "_433D.h"
}

#include "machine.cpp"

namespace cc { // `cc` stands for compile-time constants
  // NOTE: I splitted this namespace into several blocks for organizational
  // purposes (folding and so on)

  #ifdef NDEBUG
    bool constexpr ndebug{true};
  #else
    bool constexpr ndebug{false};
  #endif

  enum struct Host { lasse_raspberrypi_0, lasse_raspberrypi_1, other };

  auto constexpr host{
    hostname == "lasse-raspberrypi-0" ? Host::lasse_raspberrypi_0 :
    hostname == "lasse-raspberrypi-1" ? Host::lasse_raspberrypi_1 :
    Host::other};
}

namespace cc {
  // General config

  bool constexpr log_errors{not ndebug};
  std::string_view constexpr log_info_string{"INFO"};
  std::string_view constexpr log_error_string{"ERROR"};

  std::chrono::milliseconds constexpr sampling_interval{3000};
  unsigned constexpr samples_per_aggregate{5u};
  unsigned constexpr aggregates_per_run{60u};

  using timestamp_duration_t = std::chrono::milliseconds;
  int constexpr timestamp_width{10};
  int constexpr timestamp_decimals{2};

  int constexpr field_decimals_default{6};

  std::string_view constexpr csv_delimiter_string{", "};
  std::string_view constexpr csv_false_string{"0"};
  std::string_view constexpr csv_true_string{"1"};

  int constexpr exit_code_success{0};
  int constexpr exit_code_error{1};
  int constexpr exit_code_interrupt{130};

  std::chrono::milliseconds constexpr wait_interval_min{100};
} // namespace cc

std::string log_info_prefix;
std::string log_error_prefix;

#include "util.cpp"
#include "csv.cpp"
#include "toml.cpp"

namespace io {

template <typename T>
bool errored(T const &handle_container) { return handle_container.handle < 0; }

struct Pi {
  int handle;
  operator int() const { return this->handle; }

  Pi(Pi const &) = delete;
  Pi & operator=(Pi const &) = delete;

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
    //std::cout << "Pi constructed at " << this << std::endl;
  }

  Pi(Pi&& that) : handle{that.handle} {
    that.handle = PI_BAD_HANDLE;
    //std::cout << "Pi moved from " << &that << " to " << this << std::endl;
  }

  ~Pi() {
    if (this->handle >= 0) pigpio_stop(this->handle);
    //std::cout << "Pi destructed at " << this << std::endl;
  }
};

struct I2C {
  int handle;
  int const pi_handle;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  I2C(I2C const &) = delete;
  I2C & operator=(I2C const &) = delete;

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
    //std::cout << "I2C constructed at " << this << std::endl;
  }

  I2C(I2C&& that) : handle{that.handle}, pi_handle{that.pi_handle} {
    that.handle = PI_BAD_HANDLE;
    //std::cout << "I2C moved from " << &that << " to " << this << std::endl;
  }

  ~I2C() {
    if (this->handle >= 0) {
      int const response{i2c_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix << "closing I2C device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "I2C destructed at " << this << std::endl;
  }
};

struct Serial {
  int handle;
  int const pi_handle;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  Serial(Serial const &) = delete;
  Serial & operator=(Serial const &) = delete;

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
    //std::cout << "Serial constructed at " << this << std::endl;
  }

  Serial(Serial&& that) : handle{that.handle}, pi_handle{that.pi_handle} {
    that.handle = PI_BAD_HANDLE;
    //std::cout << "Serial moved from " << &that << " to " << this << std::endl;
  }

  ~Serial() {
    if (this->handle >= 0) {
      int const response{serial_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix << "closing serial device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "Serial destructed at " << this << std::endl;
  }
};

struct DHT {
  DHTXXD_t * dht;
  operator DHTXXD_t * () const { return this->dht; }

  DHT(DHT const &) = delete;
  DHT & operator=(DHT const &) = delete;

  DHT(int const pi_handle, int const gpio_index, int const dht_model = DHTAUTO,
      DHTXXD_CB_t callback = nullptr) :
      dht{DHTXXD(pi_handle, gpio_index, dht_model, callback)} {
    //std::cout << "DHT constructed at " << this << std::endl;
  }

  DHT(DHT&& that) : dht{that.dht} {
    that.dht = nullptr;
    //std::cout << "DHT moved from " << &that << " to " << this << std::endl;
  }

  ~DHT() {
    if (this->dht) DHTXXD_cancel(this->dht);
    //std::cout << "DHT destructed at " << this << std::endl;
  }
};

bool errored(DHT const &) { return false; }

struct LDP433Receiver {
  _433D_rx_t * ldp433_receiver;
  operator _433D_rx_t * () const { return this->ldp433_receiver; }

  LDP433Receiver(LDP433Receiver const &) = delete;
  LDP433Receiver & operator=(LDP433Receiver const &) = delete;

  LDP433Receiver(int const pi_handle, int const gpio_index,
      _433D_rx_CB_t callback = nullptr) :
      ldp433_receiver{_433D_rx(pi_handle, gpio_index, callback)} {
    //std::cout << "LDP433Receiver constructed at " << this << std::endl;
  }

  LDP433Receiver(LDP433Receiver&& that) :
      ldp433_receiver{that.ldp433_receiver} {
    that.ldp433_receiver = nullptr;
    //std::cout << "LDP433Receiver moved from " << &that << " to " << this
    //  << std::endl;
  }

  ~LDP433Receiver() {
    if (this->ldp433_receiver) _433D_rx_cancel(this->ldp433_receiver);
    //std::cout << "LDP433Receiver destructed at " << this << std::endl;
  }
};

bool errored(LDP433Receiver const &) { return false; }

struct LDP433Transmitter {
  _433D_tx_t * ldp433_transmitter;
  operator _433D_tx_t * () const { return this->ldp433_transmitter; }

  LDP433Transmitter(LDP433Transmitter const &) = delete;
  LDP433Transmitter & operator=(LDP433Transmitter const &) = delete;

  LDP433Transmitter(int const pi_handle, int const gpio_index) :
      ldp433_transmitter{_433D_tx(pi_handle, gpio_index)} {
    //std::cout << "LDP433Transmitter constructed at " << this << std::endl;
  }

  LDP433Transmitter(LDP433Transmitter&& that) :
      ldp433_transmitter{that.ldp433_transmitter} {
    that.ldp433_transmitter = nullptr;
    //std::cout << "LDP433Transmitter moved from " << &that << " to " << this
    //  << std::endl;
  }

  ~LDP433Transmitter() {
    if (this->ldp433_transmitter) _433D_tx_cancel(this->ldp433_transmitter);
    //std::cout << "LDP433Transmitter destructed at " << this << std::endl;
  }
};

bool errored(LDP433Transmitter const &) { return false; }

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

template <typename T0, typename T1 = std::uint8_t>
bool get_flag(T0 const &status, T1 const &flag = 0xffu){
  return (status & flag) != 0u;
}
template <typename T0, typename T1 = std::uint8_t>
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
template <class Rep0, class Period0, class Rep1, class Period1>
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

template <typename T>
char mhz19_checksum(T const packet) {
  char checksum{0x00};
  for(std::size_t i{1}; i < 8; ++i) checksum += packet[i];
  return (0xff - checksum) + 0x01;
}

template <typename T>
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

} // namespace io

#include "sensors.cpp"

namespace sensors {
  // IO setup and sampling functions, written here manually, because
  // machine-generating them would involve a lot of complexity for little gain
  // at my scale of operation.  However, I am generating their declarations, as
  // those are useful in some of the other generated code.

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

  auto setup_sensor_io(auto const &) { return nullptr; }

  sensor sample_sensor(auto const &clock, auto const &) {
    // NOTE: A design choice I made back when I first started the sensor-logging
    // repository was to use Unix time (as in seconds since epoch) to timestamp
    // each measurement. This function assumes that the `clock` measures Unix
    // time. In C++20, the `std::chrono::system_clock` is guaranteed to be Unix
    // time.
    auto const timestamp{std::chrono::duration_cast<cc::timestamp_duration_t>(
      clock.now().time_since_epoch())};
    return sensor{std::optional{timestamp}};
  }

  auto sample_sensor(auto const &clock) { return sample_sensor(clock, nullptr); }

  auto setup_sensorhub_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::I2C(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::I2C(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  sensorhub sample_sensorhub(auto const &clock, auto const &i2c) {
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
      io::get_flag(status, flag_ntc_overrange)};
    std::optional<bool> const ntc_error{
      io::get_flag(status, flag_ntc_error)};
    std::optional<float> const ntc_temperature{[&](){
      std::optional<float> const ntc_temperature{read(reg_ntc_temperature)};
      return (not ntc_overrange.value_or(true) and not ntc_error.value_or(true))
        ? ntc_temperature : std::optional<float>{};
    }()};

    std::optional<bool> const dht11_error{io::get_flag(read(reg_dht11_error))};
    std::optional<float> const dht11_temperature{[&](){
      std::optional<float> const dht11_temperature{read(reg_dht11_temperature)};
      return (not dht11_error.value_or(true)) ? dht11_temperature
                                              : std::optional<float>{};
    }()};
    std::optional<float> const dht11_humidity{[&](){
      std::optional<float> const dht11_humidity{read(reg_dht11_humidity)};
      return (not dht11_error.value_or(true)) ? dht11_humidity
                                              : std::optional<float>{};
    }()};

    std::optional<bool> const bmp280_error{io::get_flag(read(reg_bmp280_error))};
    std::optional<float> const bmp280_temperature{[&](){
      std::optional<float> const bmp280_temperature{
        read(reg_bmp280_temperature)};
      return (not bmp280_error.value_or(true)) ? bmp280_temperature
                                               : std::optional<float>{};
    }()};
    std::optional<float> const bmp280_pressure{[&](){
      auto const bmp280_pressure_0{read(reg_bmp280_pressure_0)};
      auto const bmp280_pressure_1{read(reg_bmp280_pressure_1)};
      auto const bmp280_pressure_2{read(reg_bmp280_pressure_2)};
      if (bmp280_pressure_0.has_value() and
          bmp280_pressure_1.has_value() and
          bmp280_pressure_2.has_value() and
          not bmp280_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(bmp280_pressure_0.value()) <<  0u |
          static_cast<std::uint32_t>(bmp280_pressure_1.value()) <<  8u |
          static_cast<std::uint32_t>(bmp280_pressure_2.value()) << 16u};
      else return std::optional<float>{};
    }()};

    std::optional<bool> const brightness_overrange{
      io::get_flag(status, flag_brightness_overrange)};
    std::optional<bool> const brightness_error{
      io::get_flag(status, flag_brightness_error)};
    std::optional<float> const brightness{[&](){
      auto const brightness_0{read(reg_brightness_0)};
      auto const brightness_1{read(reg_brightness_1)};
      if (brightness_0.has_value() and
          brightness_1.has_value() and
          not brightness_overrange.value_or(true) and
          not brightness_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(brightness_0.value()) << 0u |
          static_cast<std::uint32_t>(brightness_1.value()) << 8u};
      else return std::optional<float>{};
    }()};

    std::optional<float> const motion{util::optional_apply([](auto const x){
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

  auto setup_dht22_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::DHT(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::DHT(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  dht22 sample_dht22(auto const &clock, auto const &dht) {
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

  auto setup_mhz19_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::Serial(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::Serial(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  mhz19 sample_mhz19(auto const &clock, auto const &serial) {
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
} // namespace sensors

namespace cc {
  // Configuration of setup of physical sensors depending on machine

  using sensors_tuple_t_lasse_raspberrypi_0 = std::tuple<
    //sensors::mhz19,
    sensors::sensorhub,
    sensors::dht22>;
  auto constexpr sensors_physical_instance_names_lasse_raspberrypi_0{
    std::to_array({"sensorhub_0", "dht22_0"})};
  auto constexpr sensors_io_setup_args_lasse_raspberrypi_0{std::make_tuple(
    //std::make_tuple(const_cast<char *>("/dev/serial0"), 9600u),
    std::make_tuple(0x1u, 0x17u),
    std::make_tuple(4, DHTXX))};

  using sensors_tuple_t_lasse_raspberrypi_1 = std::tuple<
    >;
  auto constexpr sensors_physical_instance_names_lasse_raspberrypi_1{
    std::to_array({"dht22_2"})};
  auto constexpr sensors_io_setup_args_lasse_raspberrypi_1{std::make_tuple(
    )};

  //

  using sensors_tuple_t =
    std::conditional<host == Host::lasse_raspberrypi_0,
      sensors_tuple_t_lasse_raspberrypi_0,
    std::conditional<host == Host::lasse_raspberrypi_1,
      sensors_tuple_t_lasse_raspberrypi_1,
      std::tuple<>>::type>::type;
  auto constexpr n_sensors{std::tuple_size_v<cc::sensors_tuple_t>};
  sensors_tuple_t constexpr blueprint{};

  auto constexpr get_names(
      std::integral_constant<Host, Host::lasse_raspberrypi_0>) {
    return sensors_physical_instance_names_lasse_raspberrypi_0 ; }
  auto constexpr get_names(
      std::integral_constant<Host, Host::lasse_raspberrypi_1>) {
    return sensors_physical_instance_names_lasse_raspberrypi_1 ; }

  auto constexpr get_names() {
    return get_names(std::integral_constant<Host, host>{}); }

  auto constexpr sensors_physical_instance_names{get_names()};

  auto constexpr get_args(
      std::integral_constant<Host, Host::lasse_raspberrypi_0>) {
    return sensors_io_setup_args_lasse_raspberrypi_0; }
  auto constexpr get_args(
      std::integral_constant<Host, Host::lasse_raspberrypi_1>) {
    return sensors_io_setup_args_lasse_raspberrypi_1; }

  auto constexpr get_args() {
    return get_args(std::integral_constant<Host, host>{}); }

  auto constexpr sensors_io_setup_args{get_args()};

}

namespace {
  // Unnamed namespace for internal linkage
  // As far as I can tell, this basically makes definitions private to the
  // translation unit and is preferred over static variables.
  std::atomic_bool quit_early{false};
  std::condition_variable cv;
  std::mutex cv_mutex;
  void graceful_exit(int const = 0) { quit_early = true; cv.notify_all(); }

  bool interruptable_wait_until(auto const &clock, auto time_point) {
    while (true) {
      auto const now{clock.now()};

      using clock_duration_t = typename decltype(now)::duration;
      using time_point_duration_t = typename decltype(time_point)::duration;

      clock_duration_t constexpr wait_interval{std::max(clock_duration_t{1},
        std::chrono::duration_cast<clock_duration_t>(cc::wait_interval_min))};

      auto const toc{std::chrono::time_point_cast<time_point_duration_t>(
        now + wait_interval)};
      bool const finish{toc >= time_point};
      std::unique_lock<std::mutex> lk(cv_mutex);
      cv.wait_until(lk, finish ? time_point : toc);
      if (quit_early) return true;
      if (finish) return false;
    }
  }
}

int main(int const argc, char const * const argv[]) {
  std::signal(SIGINT , graceful_exit);
  std::signal(SIGTERM, graceful_exit);

  std::vector<std::string> const args{argv, argv + argc};

  if constexpr (cc::log_errors) {
    log_info_prefix = args.front() + ": " + cc::log_info_string.data() + ": ";
    log_error_prefix = args.front() + ": " + cc::log_error_string.data() + ": ";
    std::cerr
      << log_info_prefix
      << "Error logging to stderr enabled.\n"
      << log_info_prefix
      << "This probably means that this is not a release build.\n"
      << std::flush;
  }

  using key_t = std::string;
  using flag_t = bool;
  using opt_t = std::optional<std::string>;
  using flags_t = std::unordered_map<key_t, flag_t>;
  using opts_t = std::unordered_map<key_t, opt_t>;

  flags_t main_flags{};
  opts_t main_opts{{"base-path", {}}};
  auto arg_itr{
    util::get_cmd_args(main_flags, main_opts, ++(args.begin()), args.end())};

  enum struct MainMode { help, error, print_config, ldp433_listen, shortly };
  MainMode main_mode{MainMode::error};

  if (arg_itr >= args.end()) { if constexpr (cc::log_errors) std::cerr
    << log_error_prefix << "evaluating `mode` argument: not enough arguments."
    << std::endl; }
  else if (*arg_itr == "help") main_mode = MainMode::help;
  else if (*arg_itr == "print-config") main_mode = MainMode::print_config;
  else if (*arg_itr == "ldp433-listen") main_mode = MainMode::ldp433_listen;
  else if (*arg_itr == "shortly") main_mode = MainMode::shortly;
  else { if constexpr (cc::log_errors) std::cerr << log_error_prefix
    << "evaluating `mode` argument: got \"" << *arg_itr << "\"." << std::endl; }
  ++arg_itr;

  auto constexpr duration_shortly_run{cc::sampling_interval *
    cc::samples_per_aggregate * cc::aggregates_per_run};
  // TODO: If this clock can be const, I can probably make clock defintions and
  // clocks passed to functions everywhere const
  std::chrono::system_clock const clock{};
  auto const time_point_startup{clock.now()};
  auto const time_point_last_midnight{
    std::chrono::floor<std::chrono::days>(time_point_startup)};
  auto const time_point_next_shortly_run{(util::lfloor((time_point_startup -
    time_point_last_midnight) / duration_shortly_run) + 1) *
    duration_shortly_run + time_point_last_midnight};

  if (main_mode == MainMode::help or main_mode == MainMode::error) {
    std::cout << "Usage:\n"
      << "  " << args.front()
      << " [--base-path=<base path>] [--] mode [opts...] [--]\n"
        "\n"
        "  Each mode can be safely interrupted by pressing Ctrl+C or sending a "
          "SIGTERM.\n"
        "\n"
        "  The binary may have been built with or without error logging. If it "
          "has, a\n"
        "  notification will be printed to stderr at startup. If not, the "
          "process may\n"
        "  quit silently when an error has occurred.\n"
        "\n"
        "  `--base-path` sets the `sensor-logging` repository root path and is "
          "required\n"
        "  for any file IO, as it deliberately has no default value.\n"
        "\n"
        "Modes:\n"
        "  help\n"
        "    Print this usage message.\n"
        "\n"
        "  print-config\n"
        "    Print a non-exhaustive configuration, including compile-time "
            "constants and\n"
        "    some other parameters.\n"
        "\n"
        "  ldp433-listen\n"
        "    Listen for 433MHz RF transmissions and log to stdout.\n"
        "\n"
        "  shortly [--now]\n"
        "    The main mode which samples sensors at periodic time points and "
             "writes the data\n"
        "    into CSV files.\n"
        "\n"
        "    Writes to stdout if `--base-path` is not passed.\n"
        "\n"
        "    `--now` disables the default behaviour of waiting for the next "
            "full sampling\n"
        "    duration (counting from previous midnight) to finish before "
            "starting.\n"
      << std::flush;
    if (main_mode == MainMode::error) return cc::exit_code_error;
  } else if (main_mode == MainMode::print_config) {
    auto &out{std::cout};
    auto constexpr sampling_interval_in_seconds{(
      static_cast<double>(decltype(cc::sampling_interval)::period::num) /
      static_cast<double>(decltype(cc::sampling_interval)::period::den)) *
      cc::sampling_interval.count()};
    auto constexpr run_duration_in_minutes{sampling_interval_in_seconds *
      cc::samples_per_aggregate * cc::aggregates_per_run / 60};
    out
      << "# Non-exhaustive config in TOML format\n"
      << "\n"
      #ifdef __DATE__
      #ifdef __TIME__
      // NOTE: It would of course be nice to have this be a proper TOML
      // datetime, plus not depend on these macros being defined, but it's not
      // that simple to get that working, and I don't want to spend more time
      // on trying, because it's really not that important.
      << io::toml::TOMLWrapper{
          std::make_pair("compilation_local_datetime_macros",
          std::make_tuple(__DATE__, __TIME__))}
      #endif
      #endif
      << io::toml::TOMLWrapper{std::make_pair("hostname", cc::hostname)}
      << io::toml::TOMLWrapper{std::make_pair("ndebug", cc::ndebug)}
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("process", args.front())};
    if (main_opts["base-path"].has_value()) out
      << io::toml::TOMLWrapper{std::make_pair("base_path",
          main_opts["base-path"].value())};
    out
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("time_point_startup",
          time_point_startup)}
      << io::toml::TOMLWrapper{std::make_pair("time_point_last_midnight",
          time_point_last_midnight)}
      << io::toml::TOMLWrapper{std::make_pair("time_point_next_shortly_run",
          time_point_next_shortly_run)}
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("sampling_interval_in_seconds",
          sampling_interval_in_seconds)}
      << io::toml::TOMLWrapper{std::make_pair("samples_per_aggregate",
          static_cast<signed>(cc::samples_per_aggregate))}
      << io::toml::TOMLWrapper{std::make_pair("aggregates_per_run",
          static_cast<signed>(cc::aggregates_per_run))}
      << io::toml::TOMLWrapper{std::make_pair("run_duration_in_minutes",
          static_cast<signed>(run_duration_in_minutes)),
          "from the above 3 parameters"}
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("sensors", util::map_constexpr(
          [](auto const &s){ return sensors::name(s); }, cc::blueprint))}
      << io::toml::TOMLWrapper{std::make_pair("sensors_physical_instance_names",
          cc::sensors_physical_instance_names)}
      << io::toml::TOMLWrapper{std::make_pair("sensors_io_setup_args",
          cc::sensors_io_setup_args)}
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("period_system_clock",
          static_cast<double>(std::chrono::system_clock::period::num) /
          static_cast<double>(std::chrono::system_clock::period::den))}
      << io::toml::TOMLWrapper{std::make_pair("period_high_resolution_clock",
          static_cast<double>(std::chrono::high_resolution_clock::period::num) /
          static_cast<double>(std::chrono::high_resolution_clock::period::den))}
      << "\n"
      << io::toml::TOMLWrapper{std::make_pair("digits_float",
        std::numeric_limits<float>::digits)}
      << io::toml::TOMLWrapper{std::make_pair("digits_double",
        std::numeric_limits<double>::digits)}
      << io::toml::TOMLWrapper{std::make_pair("digits_long_double",
        std::numeric_limits<long double>::digits)}
      << std::flush;
  } else if (main_mode == MainMode::ldp433_listen) {
    // TODO
  } else if (main_mode == MainMode::shortly) {
    flags_t flags{{"now", false}};
    opts_t opts{};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());

    if (not flags["now"])
      if (interruptable_wait_until(clock, time_point_next_shortly_run))
        return cc::exit_code_interrupt;

    std::chrono::high_resolution_clock const sampling_clock{};
    auto const time_point_filename{clock.now()};
    auto const time_point_reference{sampling_clock.now()};

    std::array<std::ofstream, cc::n_sensors> file_streams;
    auto const print_newlines{[&](){
        std::array<bool, cc::n_sensors> a;
        for (auto &x : a) x = main_opts["base-path"].has_value();
        a[cc::n_sensors - 1] = true;
        return a;
      }()};
    auto const outs{[&](){
        std::array<std::ostream *, cc::n_sensors> a;
        std::transform(file_streams.begin(), file_streams.end(), a.begin(),
          [&](auto &fs){
            return main_opts["base-path"].has_value() ? &fs : &(std::cout) ; });
        return a;
      }()};

    auto const close_files{[&](){
        if (main_opts["base-path"].has_value()) for (auto &fs : file_streams)
          if (fs.is_open()) fs.close();
      }};

    bool error_during_resource_allocation;

    // Open files for writing
    if (main_opts["base-path"].has_value()) {
      error_during_resource_allocation = false;

      auto const filename_prefix{[&](){
          auto const ctime_filename{
            std::chrono::system_clock::to_time_t(time_point_filename)};
          char filename_prefix[20];
          std::strftime(filename_prefix, sizeof(filename_prefix),
            "%Y-%m-%d-%H-%M-%SZ", std::gmtime(&ctime_filename));
          return std::string{filename_prefix};
        }()};
      auto const shortly_path{std::filesystem::path{
        main_opts["base-path"].value()} / "data" / "shortly"};

      try {
        if (not std::filesystem::is_directory(shortly_path)) {
          error_during_resource_allocation = true;
          if constexpr (cc::log_errors) std::cerr << log_error_prefix
            << shortly_path << " is not a directory." << std::endl;
        }
      } catch (std::filesystem::filesystem_error const &e) {
        error_during_resource_allocation = true;
        if constexpr (cc::log_errors) std::cerr << log_error_prefix << e.what()
          << std::endl;
      }

      if (not error_during_resource_allocation)
        util::for_constexpr([&](auto const &s, auto const &name, auto &fs){
            if (error_during_resource_allocation) return;

            auto const file_dirname{shortly_path / cc::hostname};
            try {
              if (not std::filesystem::is_directory(file_dirname)) {
                if (std::filesystem::exists(file_dirname)) {
                  error_during_resource_allocation = true;
                  if constexpr (cc::log_errors) std::cerr << log_error_prefix
                    << file_dirname << " exists, but is not a directory."
                    << std::endl;
                } else {
                  if (not std::filesystem::create_directory(file_dirname)) {
                    error_during_resource_allocation = true;
                    if constexpr (cc::log_errors) std::cerr << log_error_prefix
                      << file_dirname << " could not be created." << std::endl;
                  }
                }
              }
            } catch (std::filesystem::filesystem_error const &e) {
              error_during_resource_allocation = true;
              if constexpr (cc::log_errors) std::cerr << log_error_prefix <<
                e.what() << std::endl;
            }
            if (error_during_resource_allocation) return;

            std::filesystem::path const file_basename{
              filename_prefix + "-" + name + ".csv"};
            auto const file_path{file_dirname / file_basename};
            try {
              if (std::filesystem::exists(file_path)) {
                error_during_resource_allocation = true;
                if constexpr (cc::log_errors) std::cerr << log_error_prefix
                  << file_path << " already exists." << std::endl;
              }
            } catch (std::filesystem::filesystem_error const &e) {
              error_during_resource_allocation = true;
              if constexpr (cc::log_errors) std::cerr << log_error_prefix <<
                e.what() << std::endl;
            }
            if (error_during_resource_allocation) return;

            fs.open(file_path, std::ios::out);
            if (not fs.good()) {
              error_during_resource_allocation = true;
              auto const state{fs.rdstate()};
              auto const error_description{
                (state & std::ios_base::badbit) ? "irrecoverable stream error" :
                (state & std::ios_base::badbit) ? "input/output operation "
                  "failed (formatting or extraction error)" :
                (state & std::ios_base::badbit) ? "irrecoverable stream error" :
                "unknown error"};
              if constexpr (cc::log_errors) std::cerr << log_error_prefix
                << "opening file at " << file_path << ": "
                << error_description << "." << std::endl;
            }
          }, cc::blueprint, cc::sensors_physical_instance_names, file_streams);
      if (error_during_resource_allocation)
        { close_files(); return cc::exit_code_error; }
    }

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    error_during_resource_allocation = false;
    auto const sensor_ios{util::map_constexpr(
      [&](auto const &s, auto const &args){
        auto sensor_io{setup_io(s, pi, args)};
        if (io::errored(sensor_io)) error_during_resource_allocation = true;
        return sensor_io;
      }, cc::blueprint, cc::sensors_io_setup_args)};
    if (error_during_resource_allocation) return cc::exit_code_error;

    util::for_constexpr([](auto const &s, auto const &name,
          std::ostream * const &out, bool const &print_newline){
        sensors::write_csv_field_names((*out), s, name, not print_newline); },
      cc::blueprint, cc::sensors_physical_instance_names, outs, print_newlines);

    for (unsigned aggregate_index{0u};
        aggregate_index < cc::aggregates_per_run;
        ++aggregate_index) {

      auto aggregate{cc::blueprint};
      auto state{util::map_constexpr([](auto const &s){
        return sensors::init_state(s); }, cc::blueprint)};

      for (unsigned sample_index{0u};
          sample_index < cc::samples_per_aggregate;
          ++sample_index) {

        if (quit_early) { close_files(); return cc::exit_code_interrupt; }

        {
          auto const xs{util::map_constexpr(
            [&](auto const &s, auto const &sensor_io){
              return sample(s, clock, sensor_io);
            }, cc::blueprint, sensor_ios)};

          std::tie(aggregate, state) = util::tr(util::map_constexpr([](
                auto const& a, auto const &s, auto const &x){
              return aggregation_step(a, s, x);
            }, aggregate, state, xs));
        }

        if ((sample_index + 1u) == cc::samples_per_aggregate) {
          aggregate = util::map_constexpr([](auto const &a, auto const &s){
              return aggregation_finish(a, s);
            }, aggregate, state);

          util::for_constexpr([](auto const &a, std::ostream * const &out,
          bool const &print_newline){
              sensors::write_csv_fields((*out), a, not print_newline);
            }, aggregate, outs, print_newlines);
        }

        auto const time_point_next_sample{time_point_reference +
          cc::sampling_interval * (aggregate_index * cc::samples_per_aggregate +
          sample_index + 1u)};
        if (interruptable_wait_until(sampling_clock, time_point_next_sample))
          { close_files(); return cc::exit_code_interrupt; }
      }
    }

    close_files();
  }

  if constexpr (cc::log_errors) if (arg_itr < args.end()) std::cerr <<
    log_error_prefix << "evaluating trailing arguments starting from `" <<
    *arg_itr << "`." << std::endl;

  return cc::exit_code_success;
} // main()
