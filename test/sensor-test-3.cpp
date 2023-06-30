#include <iostream>
#include <string>

extern "C" {
  #include <pigpiod_if2.h>
}

int main(int argc, char *argv[]) {

  const int pi_handle{pigpio_start(0, 0)};

  if (pi_handle < 0) {
    std::cerr << "Could not connect to pigpio daemon" << std::endl;
    return 1;
  }

  const int i2c_handle{i2c_open(pi_handle, 0x1u, 0x17u, 0x0u)};

  if (i2c_handle < 0) {
    using namespace std::literals;

    const std::string flag_name{
      (i2c_handle == PI_BAD_I2C_BUS) ? "PI_BAD_I2C_BUS"s :
      (i2c_handle == PI_BAD_I2C_ADDR) ? "PI_BAD_I2C_ADDR"s :
      (i2c_handle == PI_BAD_FLAGS) ? "PI_BAD_FLAGS"s :
      (i2c_handle == PI_NO_HANDLE) ? "PI_NO_HANDLE"s :
      (i2c_handle == PI_I2C_OPEN_FAILED) ? "PI_I2C_OPEN_FAILED"s :
      "unknown error"s
    };

    std::cerr
      << "Could not open I2C device: "
      << i2c_handle
      << " ("
      << flag_name
      << ")"
      << std::endl;
    pigpio_stop(pi_handle);
    return 1;
  }

  int temp{i2c_read_byte_data(pi_handle, i2c_handle, 0x1u)};

  std::cout << temp << std::endl;

  i2c_close(pi_handle, i2c_handle);

  //int GPIO{4};
  //int level{gpio_read(pi_handle, GPIO)};
  //printf("GPIO %d is %d\n", GPIO, level);

  pigpio_stop(pi_handle);

  return 0;
}
