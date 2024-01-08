#include <filesystem>
#include <fstream>
#include <iostream>
#include <ios>
#include <iomanip>
#include <string>
#include <string_view>
#include <sstream>
#include <regex>
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
#include <future>
#include <ctime>
#include <chrono>
#include <ratio>
#include <type_traits>
#include <stdexcept>

// NOTE: Since this code is supposed to run on a Raspberry Pi Zero, it must
// support the GCC or Clang version of Raspberry Pi OS Bullseye, unless I want
// to introduce an annoying dependency on a special installation of newer
// compiler versions. Thus, I won't be able to use modules instead of `#include`
// directives here.

extern "C" {
  #include <pigpiod_if2.h>
  #include "DHTXXD.h"
  #include "_433D.h"
}
