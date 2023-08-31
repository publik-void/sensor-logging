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

#include "machine.generated.cpp"

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

  std::string_view constexpr basename_dir_data{"data"};
  std::string_view constexpr basename_dir_shortly{"shortly"};
  std::string_view constexpr basename_file_control_state{".control_state"};
  std::string_view constexpr basename_file_control_params{".control_params"};
} // namespace cc

std::string log_info_prefix;
std::string log_error_prefix;

#include "util.cpp"
#include "csv.cpp"
#include "toml.cpp"
#include "io.cpp"
#include "sensors.cpp"

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
  std::optional<int> constexpr
    lpd433_receiver_gpio_index_lasse_raspberrypi_0{27},
    lpd433_transmitter_gpio_index_lasse_raspberrypi_0{17};

  using sensors_tuple_t_lasse_raspberrypi_1 = std::tuple<
    >;
  auto constexpr sensors_physical_instance_names_lasse_raspberrypi_1{
    std::to_array({"dht22_2"})};
  auto constexpr sensors_io_setup_args_lasse_raspberrypi_1{std::make_tuple(
    )};
  std::optional<int> constexpr
    lpd433_receiver_gpio_index_lasse_raspberrypi_1{},
    lpd433_transmitter_gpio_index_lasse_raspberrypi_1{};

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

  std::optional<int> constexpr
    lpd433_receiver_gpio_index{host == Host::lasse_raspberrypi_0
      ? lpd433_receiver_gpio_index_lasse_raspberrypi_0
      : lpd433_receiver_gpio_index_lasse_raspberrypi_1},
    lpd433_transmitter_gpio_index{host == Host::lasse_raspberrypi_0
      ? lpd433_transmitter_gpio_index_lasse_raspberrypi_0
      : lpd433_transmitter_gpio_index_lasse_raspberrypi_1};

}

#include "control.cpp"

namespace {
  // Unnamed namespace for internal linkage
  // As far as I can tell, this basically makes definitions private to the
  // translation unit and is preferred over static variables.
  std::atomic_bool quit_early{false};
  std::condition_variable cv;
  std::mutex cv_mutex;
  void graceful_exit(int const = 0) { quit_early = true; cv.notify_all(); }

  bool interruptible_wait() {
    std::unique_lock<std::mutex> lk(cv_mutex);
    cv.wait(lk);
    return true;
  }

  bool interruptible_wait_until(auto const &clock, auto time_point) {
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
    std::cerr << log_info_prefix
      << "Error logging to stderr enabled." << std::endl;
    if constexpr (not cc::ndebug) std::cerr << log_info_prefix
      << "`NDEBUG` not defined, meaning this is probably not a release build."
      << std::endl;
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

  enum struct MainMode { help, error, print_config, lpd433_listen,
    lpd433_oneshot, shortly, daily};
  MainMode main_mode{MainMode::error};

  if (arg_itr >= args.end()) { if constexpr (cc::log_errors) std::cerr
    << log_error_prefix << "evaluating `mode` argument: not enough arguments."
    << std::endl; }
  else if (*arg_itr == "help") main_mode = MainMode::help;
  else if (*arg_itr == "print-config") main_mode = MainMode::print_config;
  else if (*arg_itr == "lpd433-listen") main_mode = MainMode::lpd433_listen;
  else if (*arg_itr == "lpd433-oneshot") main_mode = MainMode::lpd433_oneshot;
  else if (*arg_itr == "shortly") main_mode = MainMode::shortly;
  else if (*arg_itr == "daily") main_mode = MainMode::daily;
  else { if constexpr (cc::log_errors) std::cerr << log_error_prefix
    << "evaluating `mode` argument: got \"" << *arg_itr << "\"." << std::endl; }
  ++arg_itr;

  auto constexpr duration_shortly_run{cc::sampling_interval *
    cc::samples_per_aggregate * cc::aggregates_per_run};
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
        "  lpd433-listen [--n-bits-min=<n> --n-bits-max=<m> "
          "--glitch=<t>]\n"
        "    Listen for 433MHz RF transmissions and log to stdout in CSV "
            "format.\n"
        "\n"
        "    Codes with less than `n` (default 8) or more than `m` (default "
            "32) bits as\n"
        "    well as bit steps shorter than `t` (default 150) µs are ignored.\n"
        "\n"
        "  lpd433-oneshot [--n-bits=<n> --n-repeats=<m> --intercode-gap=<t>\n"
        "  --pulse-length-short=<u> --pulse-length-long=<v>] codes...\n"
        "    Send `codes` as 433MHz RF transmissions.\n"
        "\n"
        "    Transmitted codes will have a length of `n` (default 24) bits, be "
            "repeated\n"
        "    `m` (default 6) times, with an intercode gap of `t` (default "
            "9000) µs, a\n"
        "    short pulse length of `u` (default 300) µs, and a long pulse "
            "length of `v`\n"
        "    (default 900) µs.\n"
        "\n"
        "  shortly [--now]\n"
        "    The main mode which samples sensors at periodic time points and "
             "writes the\n"
        "    data into CSV files.\n"
        "\n"
        "    Writes to stdout if `--base-path` is not passed.\n"
        "\n"
        "    `--now` disables the default behaviour of waiting for the next "
            "full sampling\n"
        "    duration (counting from previous midnight) to finish before "
            "starting.\n"
        "\n"
        "  daily [opts...]\n"
        "    Calls a Python interpreter running the `daily.py` script with "
            "the\n"
        "    `--hostname`, `--file-extension`, `base_path`, and `name...` "
            "arguments set\n"
        "    according to the configuration of the present binary (as output "
            "by the\n"
        "    `print-config` subcommand).\n"
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
  } else if (main_mode == MainMode::lpd433_listen) {
    if (not cc::lpd433_receiver_gpio_index.has_value()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "No LPD433 receiver configured in the present binary." << std::endl;
      return cc::exit_code_error;
    }

    flags_t flags{};
    opts_t opts{{"n-bits-min", {}}, {"n-bits-max", {}}, {"glitch", {}}};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());
    auto const parser{+[](std::string const &s){
      return std::stoi(s, nullptr, 0); }};
    auto const n_bits_min{util::parse_arg_value(parser, opts, "n-bits-min", 8)};
    auto const n_bits_max{
      util::parse_arg_value(parser, opts, "n-bits-max", 32)};
    auto const glitch{util::parse_arg_value(parser, opts, "glitch", 150)};

    _433D_rx_CB_t const callback{+[](_433D_rx_data_t x){
      std::chrono::system_clock const clock{};
      sensors::write_csv_fields(std::cout, sensors::lpd433_receiver{
        sensors::sample_sensor(clock), {x.code}, {x.bits}, {x.gap}, {x.t0},
        {x.t1}});
    }};

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    io::LPD433Receiver const
      lpd433_receiver{pi, cc::lpd433_receiver_gpio_index.value(), callback};
    _433D_rx_set_bits(lpd433_receiver, n_bits_min, n_bits_max);
    _433D_rx_set_glitch(lpd433_receiver, glitch);

    sensors::write_csv_field_names(std::cout, sensors::lpd433_receiver{}, "");

    interruptible_wait();
  } else if (main_mode == MainMode::lpd433_oneshot) {
    if (not cc::lpd433_transmitter_gpio_index.has_value()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "No LPD433 transmitter configured in the present binary."
        << std::endl;
      return cc::exit_code_error;
    }

    flags_t flags{};
    opts_t opts{{"n-bits", {}}, {"n-repeats", {}}, {"intercode-gap", {}},
      {"pulse-length-short", {}}, {"pulse-length-long", {}}};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());
    auto const parser{+[](std::string const &s){
      return std::stoi(s, nullptr, 0); }};
    auto const n_bits{util::parse_arg_value(parser, opts, "n-bits", 24)};
    auto const n_repeats{util::parse_arg_value(parser, opts, "n-repeats", 6)};
    auto const intercode_gap{
      util::parse_arg_value(parser, opts, "intercode-gap", 9000)};
    auto const pulse_length_short{
      util::parse_arg_value(parser, opts, "pulse-length-short", 300)};
    auto const pulse_length_long{
      util::parse_arg_value(parser, opts, "pulse-length-long", 900)};

    std::vector<std::uint64_t> codes{};
    while (arg_itr < args.end()) {
      try {
        auto const code_ull{std::stoull(*arg_itr, nullptr, 0)};
        codes.push_back(code_ull);
      } catch (std::exception const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "parsing code `" << *arg_itr << "` (" << e.what() << "). Ignoring."
        << std::endl;
      }
      ++arg_itr;
    }

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    io::lpd433_oneshot_send(pi, cc::lpd433_transmitter_gpio_index.value(),
      codes, n_bits, n_repeats, intercode_gap, pulse_length_short,
      pulse_length_long);
  } else if (main_mode == MainMode::shortly) {
    flags_t flags{{"now", false}};
    opts_t opts{};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());

    if (not flags["now"])
      if (interruptible_wait_until(clock, time_point_next_shortly_run))
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

    // NOTE: I am not sure if this is even needed. I think all file streams will
    // be closed anyway when they go out of scope.
    auto const close_files{[&](){
        if (main_opts["base-path"].has_value()) for (auto &fs : file_streams)
          if (fs.is_open()) fs.close();
      }};

    // TODO: Construct from previous serialization if available
    control::control_state control_state{};

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
      auto const path_dir_shortly{std::filesystem::path{
        main_opts["base-path"].value()} / cc::basename_dir_data /
        cc::basename_dir_shortly};

      try {
        if (not std::filesystem::is_directory(path_dir_shortly)) {
          error_during_resource_allocation = true;
          if constexpr (cc::log_errors) std::cerr << log_error_prefix
            << path_dir_shortly << " is not a directory." << std::endl;
        }
      } catch (std::filesystem::filesystem_error const &e) {
        error_during_resource_allocation = true;
        if constexpr (cc::log_errors) std::cerr << log_error_prefix << e.what()
          << std::endl;
      }

      if (not error_during_resource_allocation)
        util::for_constexpr([&](auto const &name, auto &fs){
            if (error_during_resource_allocation) return;

            auto const dirname_file{path_dir_shortly / cc::hostname};
            try {
              if (not std::filesystem::is_directory(dirname_file)) {
                if (std::filesystem::exists(dirname_file)) {
                  error_during_resource_allocation = true;
                  if constexpr (cc::log_errors) std::cerr << log_error_prefix
                    << dirname_file << " exists, but is not a directory."
                    << std::endl;
                } else {
                  if (not std::filesystem::create_directory(dirname_file)) {
                    error_during_resource_allocation = true;
                    if constexpr (cc::log_errors) std::cerr << log_error_prefix
                      << dirname_file << " could not be created." << std::endl;
                  }
                }
              }
            } catch (std::filesystem::filesystem_error const &e) {
              error_during_resource_allocation = true;
              if constexpr (cc::log_errors) std::cerr << log_error_prefix <<
                e.what() << std::endl;
            }
            if (error_during_resource_allocation) return;

            std::filesystem::path const basename_file{
              filename_prefix + "-" + name + ".csv"};
            auto const path_file{dirname_file / basename_file};
            try {
              if (std::filesystem::exists(path_file)) {
                error_during_resource_allocation = true;
                if constexpr (cc::log_errors) std::cerr << log_error_prefix
                  << path_file << " already exists." << std::endl;
              }
            } catch (std::filesystem::filesystem_error const &e) {
              error_during_resource_allocation = true;
              if constexpr (cc::log_errors) std::cerr << log_error_prefix <<
                e.what() << std::endl;
            }
            if (error_during_resource_allocation) return;

            fs.open(path_file, std::ios::out);
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
                << "opening file at " << path_file << ": "
                << error_description << "." << std::endl;
            }
          }, cc::sensors_physical_instance_names, file_streams);
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

        auto const xs{util::map_constexpr(
          [&](auto const &s, auto const &sensor_io){
            return sample(s, clock, sensor_io);
          }, cc::blueprint, sensor_ios)};

        std::tie(aggregate, state) = util::tr(util::map_constexpr([](
              auto const& a, auto const &s, auto const &x){
            return aggregation_step(a, s, x);
          }, aggregate, state, xs));

        if ((sample_index + 1u) == cc::samples_per_aggregate) {
          aggregate = util::map_constexpr([](auto const &a, auto const &s){
              return aggregation_finish(a, s);
            }, aggregate, state);

          util::for_constexpr([](auto const &a, std::ostream * const &out,
          bool const &print_newline){
              sensors::write_csv_fields((*out), a, not print_newline);
            }, aggregate, outs, print_newlines);
        }

        control_state = control::control_tick(pi, control_state, xs);

        auto const time_point_next_sample{time_point_reference +
          cc::sampling_interval * (aggregate_index * cc::samples_per_aggregate +
          sample_index + 1u)};
        if (interruptible_wait_until(sampling_clock, time_point_next_sample))
          { close_files(); return cc::exit_code_interrupt; }
      }
    }

    close_files();
  } else if (main_mode == MainMode::daily) {
    if (not main_opts["base-path"].has_value()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "`--base-path` option must be set in `daily` mode." << std::endl;
      return cc::exit_code_error;
    }

    // NOTE: Calling subprocesses in general and especially with the standard
    // libraries and GNU C libraries is a safety hazard and non-portable.
    // However, as this is code that is only meant to be run at home on
    // Raspberry Pis it's probably not necessary to overcomplicate things by
    // worrying about that. In that same spirit, let's just use `std::system`
    // here instead of `setenv`, `fork`, `exec`, `execle`, etc. Yes, it's an
    // extra shell indirection, but if I'd care that much about performance
    // here, I shouldn't use Python in the first place. Yes, it creates some
    // complexity in terms of escaping arguments, but it should be doable for
    // practical purposes. The upside is I can stay within the C++ standard
    // library and things should at least be relatively portable on Unix-like
    // systems with a `python3` command on the `PATH`.  `script/daily.py` is
    // probably kind of unsafe on its own already anyway…

    auto const quote{[](auto const &s){
        return "\"" + std::regex_replace(std::string{s}, std::regex{"\\\"|\\\\"},
          "\\$&") + "\"";
      }};

    auto const export_string{[&](auto const &k, auto const &v){
      return std::string{"export SENSOR_LOGGING_DAILY_PY_"} +
        k + "=" + quote(v) + " &&\n";}};

    auto const sensors_physical_instance_names_comma_separated{[&](){
        std::string result{};
        bool is_first_entry{true};
        util::for_constexpr([&](auto const &name){
            if (is_first_entry) is_first_entry = false;
            else result += ",";
            result += name;
          }, cc::sensors_physical_instance_names);
        return result;
      }()};

    std::filesystem::path const path_base{main_opts["base-path"].value()};
    std::filesystem::path const
      daily_py_path{path_base / "script" / "daily.py"};

    std::string const args_quoted{[&](){
      std::string s{""};
      while (arg_itr < args.end()) s += " " + quote(*(arg_itr++));
      return s; }()};

    std::string const command{
      export_string("COMMAND", args.front() + " --base-path=<base_path> daily")
        +
      export_string("FILE_EXTENSION", "csv") +
      export_string("HOSTNAME", cc::hostname) +
      export_string("BASE_PATH", path_base.native()) +
      export_string("NAMES", sensors_physical_instance_names_comma_separated) +
      "{ type pypy3 > /dev/null && python_interpreter=pypy3 ||\n"
      "  { type python3 > /dev/null && python_interpreter=python3; }; } &&\n"
      "\"$python_interpreter\" " + quote(daily_py_path.native()) + args_quoted};

    if constexpr (cc::log_errors) std::cerr << log_info_prefix
      << "Running the following shell command:\n  "
      << std::regex_replace(command, std::regex{"\\n"}, "\n  ") << std::endl;

    std::cout << std::flush; std::cerr << std::flush;
    auto const status_value{std::system(command.c_str())};
    if (WIFEXITED(status_value)) return WEXITSTATUS(status_value);
    return cc::exit_code_error;
  }

  if constexpr (cc::log_errors) if (arg_itr < args.end()) std::cerr <<
    log_error_prefix << "evaluating trailing arguments starting from `" <<
    *arg_itr << "`." << std::endl;

  return cc::exit_code_success;
} // main()
