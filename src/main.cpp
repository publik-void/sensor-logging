#include <iostream>
#include <ios>
#include <chrono>
#include <thread>
#include <optional>
#include <tuple>
#include <string_view>
#include <cstdint>
#include <cstdlib>

extern "C" {
  #include <pigpiod_if2.h>
}

#include "machine.cpp"

namespace cc { // compile-time constants

  bool constexpr log_errors{true};

  std::chrono::milliseconds constexpr sampling_interval{300};
  std::chrono::milliseconds constexpr sleeping_{10};
  unsigned constexpr samples_per_aggregate{5u};
  unsigned constexpr aggregates_per_run{4u};
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
  }

  ~Pi() {
    if (this->handle >= 0) pigpio_stop(this->handle);
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
    if (this->handle < 0) if constexpr (cc::log_errors) std::cerr
      << "Error opening I2C device on "
      << "Pi " << pi_handle
      << ", bus 0x" << std::hex << bus << std::dec
      << ", address 0x" << std::hex << addr << std::dec
      << ", flags 0x" << std::hex << flags << std::dec
      << ": " << pigpio_error(this->handle) << std::endl;
  }

  ~I2C() {
    if (this->handle >= 0) {
      int const response{i2c_close(this->pi_handle, this->handle)};
      if (response < 0) if constexpr (cc::log_errors) std::cerr
        << "Error closing I2C device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
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

//int main(int argc, char *argv[]) {
int main() {
  Pi const pi{};
  if (errored(pi)) return 1;

  I2C const i2c{pi, 0x1u, 0x17u};
  if (errored(i2c)) return 1;

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

    //sensors::write_field_names(std::cout, a0);

    for (unsigned sample_index{0u};
        sample_index < cc::samples_per_aggregate;
        ++sample_index) {

      //std::cout
      //  << "Aggregate index " << aggregate_index
      //  << ", sample index " << sample_index
      //  << std::endl;

      auto const x0{sensors::sample_sensorhub(pi, i2c)};
      std::tie(a0, s0) = aggregation_step(a0, s0, x0);
      //sensors::write_fields(std::cout, x0);

      //int GPIO{4};
      //int level{gpio_read(pi, GPIO)};
      //printf("GPIO %d is %d\n", GPIO, level);

      if ((sample_index + 1u) == cc::samples_per_aggregate) {
        a0 = aggregation_finish(a0, s0);
        sensors::write_fields(std::cout, a0);
      }

      std::this_thread::sleep_until(time_reference + (
          (aggregate_index * cc::samples_per_aggregate + sample_index + 1u) *
        cc::sampling_interval));
    }
  }

  return 0;
}
