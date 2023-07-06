#include <iostream>
#include <ios>
#include <optional>
#include <tuple>
#include <string_view>
#include <cstdint>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

// TODO: Replace `#include` directives with `import` statements. On Debian
// Bullseye, which is still the basis for the current Raspberry Pi OS at the
// time of writing this comment, the GCC version is too old to support the use
// of modules. I could perhaps rely on backports or clang, but I think I won't
// mess around with those options for now. but maybe I'll update to Debian
// Bookworm when it is possible, which surely comes with a newer GCC version.

extern "C" {
  #include <pigpiod_if2.h>
  #include "DHTXXD.h"
}

#include "machine.cpp"

namespace cc { // compile-time constants

  bool constexpr log_errors{true};

  std::chrono::milliseconds constexpr sampling_interval{300};
  unsigned constexpr samples_per_aggregate{5u};
  unsigned constexpr aggregates_per_run{600u};

  std::string_view constexpr csv_delimiter_string{", "};
  std::string_view constexpr csv_false_string{"0"};
  std::string_view constexpr csv_true_string{"1"};

  int constexpr exit_code_success{0};
  int constexpr exit_code_error{1};
  int constexpr exit_code_interrupt{130};
}

template<typename T>
struct CSVWrapper {
  T const &x;
};

template<typename T>
std::ostream& operator<<(std::ostream& os, CSVWrapper<T> const &x) {
  return os << x.x;
}
template<typename T>
std::ostream& operator<<(std::ostream& os,
    CSVWrapper<std::optional<T>> const &x) {
  return x.x.has_value() ? (os << CSVWrapper{x.x.value()})
                         : os << ""; // Still need to call `<<` e.g. for `setw`
}
std::ostream& operator<<(std::ostream& os, CSVWrapper<bool> const &x) {
  return os << (x.x ? cc::csv_true_string : cc::csv_false_string);
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
    if (this->handle < 0) {
      if constexpr (cc::log_errors) {
        char const *port_env{std::getenv("PIGPIO_PORT")};
        std::cerr << "Error connecting to pigpio daemon at "
          << "address " << (addr ? addr : "localhost")
          << ", port " << (port ? port : (port_env ? port_env : "8888"))
          << ": " << pigpio_error(this->handle) << std::endl;
      }
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
  operator int() const { return this->handle; }

  I2C(I2C const &) = delete;

  I2C(int const pi_handle, unsigned const bus,
      unsigned const addr, unsigned const flags = 0u) : 
      handle{i2c_open(pi_handle, bus, addr, flags)}, pi_handle{pi_handle} {
    if (this->handle < 0) if constexpr (cc::log_errors) {
      auto const original_flags{std::cerr.flags()};
      std::cerr
        << "Error opening I2C device on "
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
      if (response < 0) if constexpr (cc::log_errors) std::cerr
        << "Error closing I2C device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "I2C destructed" << std::endl;
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

template<typename T>
bool errored(T const &handle_container) { return handle_container.handle < 0; }

auto create_i2c_reader(int const pi_handle, int const i2c_handle) {
  return [=](unsigned const reg){
    int const response{i2c_read_byte_data(pi_handle, i2c_handle, reg)};
    if (response < 0) {
      if constexpr (cc::log_errors) std::cerr << "Error reading from "
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

#include "sensors.cpp"

namespace sensors {
  // Sampling functions, written here manually, because machine-generating them
  // would involve a lot of complexity for little gain at my scale of operation.

  sensorhub sample_sensorhub(int const pi_handle, int const i2c_handle) {
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

    auto const read{create_i2c_reader(pi_handle, i2c_handle)};

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

    return sensorhub{{},
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
}

// Returns a tick count since the epoch for the given time point
template<class TickPeriod = std::chrono::seconds, class Clock, class Duration>
auto const integral(std::chrono::time_point<Clock, Duration> const &time_point){
  return std::chrono::duration_cast<TickPeriod>(
    time_point.time_since_epoch()).count();
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

//int main(int argc, char *argv[]) {
int main() {
  std::signal(SIGINT , graceful_exit);
  std::signal(SIGTERM, graceful_exit);

  Pi const pi{};
  if (errored(pi)) return cc::exit_code_error;

  I2C const i2c{pi, 0x1u, 0x17u};
  if (errored(i2c)) return cc::exit_code_error;

  DHT const dht{pi, 4, DHTXX};

  std::chrono::high_resolution_clock const clock{};

  auto const time_reference{clock.now()};

  //std::cout << integral(time_reference) << std::endl;

  //unsigned constexpr samples_per_run{
  //  cc::samples_per_aggregate * cc::aggregates_per_run};

  for (unsigned aggregate_index{0u};
      aggregate_index < cc::aggregates_per_run;
      ++aggregate_index) {

    sensors::sensorhub a0{};
    sensors::sensorhub_state s0{};

    //sensors::write_csv_field_names(std::cout, a0);

    for (unsigned sample_index{0u};
        sample_index < cc::samples_per_aggregate;
        ++sample_index) {

      if (quit_early) return cc::exit_code_interrupt;

      auto ready{DHTXXD_ready(dht)};
      auto dht_data{DHTXXD_data(dht)};
      std::cout
        << "ready: " << ready
        << ", temperature: " << dht_data.temperature
        << ", humidity: " << dht_data.humidity
        << ", timestamp: " << dht_data.timestamp
        << ", status: " << dht_data.status
        << std::endl;;
      DHTXXD_manual_read(dht);

      //std::cout
      //  << "Aggregate index " << aggregate_index
      //  << ", sample index " << sample_index
      //  << std::endl;

      auto const x0{sensors::sample_sensorhub(pi, i2c)};
      std::tie(a0, s0) = aggregation_step(a0, s0, x0);
      //sensors::write_csv_fields(std::cout, x0);

      //int GPIO{4};
      //int level{gpio_read(pi, GPIO)};
      //printf("GPIO %d is %d\n", GPIO, level);

      if ((sample_index + 1u) == cc::samples_per_aggregate) {
        a0 = aggregation_finish(a0, s0);
        sensors::write_csv_fields(std::cout, a0);
      }

      auto const next_time_point{time_reference + cc::sampling_interval *
        (aggregate_index * cc::samples_per_aggregate + sample_index + 1u)};
      std::unique_lock<std::mutex> lk(cv_mutex);
      cv.wait_until(lk, next_time_point);
    }
  }

  return cc::exit_code_success;
}
