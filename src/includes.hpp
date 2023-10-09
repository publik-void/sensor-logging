#include <filesystem>
#include <fstream>
#include <iostream>
#include <ios>
#include <iomanip>
#include <string>
#include <string_view>
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

// TODO: Replace `#include` directives with `import` statements. On Debian
// Bullseye, which is still the basis for the current Raspberry Pi OS at the
// time of writing this comment, the GCC (and standard Clang) version is too old
// to support the use of modules. I could perhaps rely on backports or clang,
// but I think I won't mess around with those options for now. but maybe I'll
// update to Debian Bookworm when it is possible, which surely comes with a
// newer GCC version.
// NOTE: I will have to look at how precompiling headers works with modules.

extern "C" {
  #include <pigpiod_if2.h>
  #include "DHTXXD.h"
  #include "_433D.h"
}
