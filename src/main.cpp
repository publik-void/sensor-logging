#include <iostream>
#include <chrono>
#include <thread>
#include <optional>
#include <string_view>

extern "C" {
  #include <pigpiod_if2.h>
}

#include "sensors.cpp"

namespace cc { // compile-time constants
  #include "machine.cpp"

  bool constexpr use_gpio{true};
  bool constexpr use_i2c{hostname == std::string_view{"lasse-raspberrypi-0"}};

  std::chrono::milliseconds constexpr sampling_interval{250};
  std::chrono::milliseconds constexpr sleeping_{10};
  unsigned constexpr samples_per_aggregate{6u};
  unsigned constexpr aggregates_per_run{2u};
}

// Returns a tick count since the epoch for the given time point
template<class TickPeriod = std::chrono::seconds, class Clock, class Duration>
auto const integral(std::chrono::time_point<Clock, Duration> const &time_point){
  return std::chrono::duration_cast<TickPeriod>(
    time_point.time_since_epoch()).count();
}

//int main(int argc, char *argv[]) {
int main() {
  int const pi_handle{cc::use_gpio ? pigpio_start(0, 0) : 0};

  if (pi_handle < 0) {
    std::cerr
      << "Could not connect to pigpio daemon: "
      << pigpio_error(pi_handle)
      << std::endl;
    return 1;
  }

  int const i2c_handle{
    cc::use_i2c ? i2c_open(pi_handle, 0x1u, 0x17u, 0x0u) : 0};

  if (i2c_handle < 0) {
    pigpio_stop(pi_handle);
    std::cerr
      << "Could not open I2C device: "
      << pigpio_error(i2c_handle)
      << std::endl;
    return 1;
  }

  std::chrono::high_resolution_clock const clock{};

  auto const time_reference{clock.now()};

  //std::cout << integral(time_reference) << std::endl;

  //unsigned constexpr samples_per_run{
  //  cc::samples_per_aggregate * cc::aggregates_per_run};

  for (unsigned aggregate_index{0u};
      aggregate_index < cc::aggregates_per_run;
      ++aggregate_index) {
    for (unsigned sample_index{0u};
        sample_index < cc::samples_per_aggregate;
        ++sample_index) {

      std::cout
        << "Aggregate index " << aggregate_index
        << ", sample index " << sample_index
        << std::endl;

      int temp{i2c_read_byte_data(pi_handle, i2c_handle, 0x1u)};
      std::cout << temp << std::endl;

      //int GPIO{4};
      //int level{gpio_read(pi_handle, GPIO)};
      //printf("GPIO %d is %d\n", GPIO, level);

      if ((sample_index + 1u) == cc::samples_per_aggregate) {
        // TODO: Do aggregation…
      }

      std::this_thread::sleep_until(time_reference + (
          (aggregate_index * cc::samples_per_aggregate + sample_index + 1u) *
        cc::sampling_interval));
    }
  }

  i2c_close(pi_handle, i2c_handle);
  pigpio_stop(pi_handle);

  return 0;
}
