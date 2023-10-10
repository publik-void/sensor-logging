// #include "includes.h" // Commented out, because CMake handles this

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

  bool constexpr log_errors{true};
  bool constexpr log_info{not ndebug};
  std::string_view constexpr log_info_string{"INFO"};
  std::string_view constexpr log_error_string{"ERROR"};

  std::chrono::milliseconds constexpr sampling_interval{3000};
  unsigned constexpr samples_per_aggregate{5u};
  unsigned constexpr aggregates_per_run{3u/*60u*/};

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

  int constexpr lpd433_receive_n_bits_min_default{8};
  int constexpr lpd433_receive_n_bits_max_default{32};
  int constexpr lpd433_receive_glitch_default{150};

  int constexpr lpd433_send_n_bits_default{24};
  int constexpr lpd433_send_n_repeats_default{6};
  int constexpr lpd433_send_intercode_gap_default{9000};
  int constexpr lpd433_send_pulse_length_short_default{300};
  int constexpr lpd433_send_pulse_length_long_default{900};

  float constexpr buzz_t_seconds_default{.08f};
  float constexpr buzz_f_hertz_default{1000.f};
  float constexpr buzz_pulse_width_default{.1f};

  std::chrono::milliseconds constexpr mhz19_receive_timeout_default{1000};
  std::chrono::milliseconds constexpr mhz19_receive_interval_default{50};

  std::string_view constexpr basename_dir_data{"data"};
  std::string_view constexpr basename_dir_shortly{"shortly"};
  std::string_view constexpr basename_file_control_state{".control-state"};
  std::string_view constexpr basename_file_control_params{".control-params"};
}

std::string log_info_prefix;
std::string log_error_prefix;

#include "util.cpp"
#include "csv.cpp"
#include "toml.cpp"
#include "io.cpp"
#include "sensors.cpp"

enum struct MainMode {
  help,
  error,
  print_config,
  lpd433_listen,
  lpd433_oneshot,
  buzz_oneshot,
  control,
  shortly,
  daily};
std::string main_mode_name(MainMode const &mode) {
  if (mode == MainMode::help          ) return "help";
  if (mode == MainMode::error         ) return "error";
  if (mode == MainMode::print_config  ) return "print-config";
  if (mode == MainMode::lpd433_listen ) return "lpd433-listen";
  if (mode == MainMode::lpd433_oneshot) return "lpd433-oneshot";
  if (mode == MainMode::buzz_oneshot  ) return "buzz-oneshot";
  if (mode == MainMode::control       ) return "control";
  if (mode == MainMode::shortly       ) return "shortly";
  if (mode == MainMode::daily         ) return "daily";
  return "";
}

namespace cc {

  // NOTE: `std::unordered_map`'s implementation is not `constexpr`-friendly,
  // so that's why this is only makred `const`. I believe it doesn't matter.
  std::unordered_map<MainMode, sensors::WriteFormat> const
    write_format_defaults{
      {MainMode::help          , sensors::WriteFormat::csv },
      {MainMode::error         , sensors::WriteFormat::csv },
      {MainMode::print_config  , sensors::WriteFormat::toml},
      {MainMode::lpd433_listen , sensors::WriteFormat::csv },
      {MainMode::lpd433_oneshot, sensors::WriteFormat::csv },
      {MainMode::buzz_oneshot  , sensors::WriteFormat::csv },
      {MainMode::control       , sensors::WriteFormat::csv },
      {MainMode::shortly       , sensors::WriteFormat::csv },
      {MainMode::daily         , sensors::WriteFormat::csv }};

  // Configuration of setup of physical sensors depending on machine

  using sensors_tuple_t_lasse_raspberrypi_0 = std::tuple<
    sensors::sensorhub,
    sensors::dht22,
    sensors::mhz19>;
  auto constexpr sensors_physical_instance_names_lasse_raspberrypi_0{
    std::to_array({"sensorhub_0",
                   "dht22_0",
                   "mhz19_1"})};
  auto constexpr sensors_io_setup_args_lasse_raspberrypi_0{std::make_tuple(
    std::make_tuple(0x1u, 0x17u),
    std::make_tuple(17, DHTXX),
    std::make_tuple(const_cast<char *>("/dev/serial0"), 9600u))};
  std::optional<int> constexpr
    lpd433_receiver_gpio_index_lasse_raspberrypi_0{},
    lpd433_transmitter_gpio_index_lasse_raspberrypi_0{},
    buzzer_gpio_index_lasse_raspberrypi_0{};

  using sensors_tuple_t_lasse_raspberrypi_1 = std::tuple<
    sensors::dht22,
    sensors::dht22,
    sensors::mhz19>;
  auto constexpr sensors_physical_instance_names_lasse_raspberrypi_1{
    std::to_array({"dht22_1",
                   "dht22_2",
                   "mhz19_0"})};
  auto constexpr sensors_io_setup_args_lasse_raspberrypi_1{std::make_tuple(
    std::make_tuple(5, DHTXX),
    std::make_tuple(6, DHTXX),
    std::make_tuple(const_cast<char *>("/dev/serial0"), 9600u))};
  std::optional<int> constexpr
    lpd433_receiver_gpio_index_lasse_raspberrypi_1{24},
    lpd433_transmitter_gpio_index_lasse_raspberrypi_1{23},
    buzzer_gpio_index_lasse_raspberrypi_1{16};

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
      : lpd433_transmitter_gpio_index_lasse_raspberrypi_1},
    buzzer_gpio_index{host == Host::lasse_raspberrypi_0
      ? buzzer_gpio_index_lasse_raspberrypi_0
      : buzzer_gpio_index_lasse_raspberrypi_1};

  template<size_t n>
  struct template_string_literal_helper {
    char value[n];
    constexpr template_string_literal_helper(const char (&str)[n]) {
      std::copy_n(str, n, value);
    }
  };

  // NOTE: On Raspberry Pi OS Bullseye, where I was developing this, Clang was
  // only version 11.0. To use a string literal as template parameter in the way
  // these functions do required GCC or a newer version of Clang.
  template<template_string_literal_helper name>
  std::size_t constexpr get_sensor_physical_instance_index(){
    auto constexpr it{std::find(cc::sensors_physical_instance_names.cbegin(),
      cc::sensors_physical_instance_names.cend(),
      std::string_view{name.value})};
    return it - cc::sensors_physical_instance_names.cbegin();
  }
  template<template_string_literal_helper name>
  auto constexpr get_sensor(auto const &sensors) {
    auto constexpr i{get_sensor_physical_instance_index<name>()};
    return std::get<i>(sensors);
  }

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

  if constexpr (cc::log_info) {
    log_info_prefix =
      "# " + args.front() + ": " + cc::log_info_string.data() + ": ";
    std::cerr << log_info_prefix
      << "Info logging to stderr enabled.\n" << log_info_prefix
      << "Error logging to stderr " << (cc::log_errors ? "en" : "dis")
      << "abled." << std::endl;
    std::string_view constexpr not_str{cc::ndebug ? "" : "not "};
    std::cerr << log_info_prefix
      << "`NDEBUG` " << not_str << "defined, meaning this is probably "
      << not_str << "a release build." << std::endl;
  }
  if constexpr (cc::log_errors) log_error_prefix =
    "# " + args.front() + ": " + cc::log_error_string.data() + ": ";

  using key_t = std::string;
  using flag_t = bool;
  using opt_t = std::optional<std::string>;
  using flags_t = std::unordered_map<key_t, flag_t>;
  using opts_t = std::unordered_map<key_t, opt_t>;

  flags_t main_flags{};
  opts_t main_opts{{"base-path", {}}, {"format", {}}};
  auto arg_itr{
    util::get_cmd_args(main_flags, main_opts, ++(args.begin()), args.end())};

  MainMode main_mode{MainMode::error};

  if (arg_itr >= args.end()) { if constexpr (cc::log_errors) std::cerr
    << log_error_prefix << "evaluating `mode` argument: not enough arguments."
    << std::endl; }
  else {
    using U = std::underlying_type_t<MainMode>;
    for (MainMode mode{MainMode::help};
        static_cast<U>(mode) <= static_cast<U>(MainMode::daily);
        mode = static_cast<MainMode>(static_cast<U>(mode) + U{1}))
      if (*arg_itr == main_mode_name(mode)) main_mode = mode;
    if constexpr (cc::log_errors) if (main_mode == MainMode::error) std::cerr
      << log_error_prefix << "evaluating `mode` argument: got \"" << *arg_itr
      << "\"." << std::endl;
    ++arg_itr;
  }

  sensors::WriteFormat const write_format{[&](){
      auto const &opt_format{main_opts["format"]};
      if (opt_format.has_value()) {
        auto const opt_format_value{opt_format.value()};
        using U = std::underlying_type_t<sensors::WriteFormat>;
        for (auto wf{sensors::WriteFormat::csv};
            static_cast<U>(wf) <= static_cast<U>(sensors::WriteFormat::toml);
            wf = static_cast<sensors::WriteFormat>(static_cast<U>(wf) + U{1}))
          if (opt_format_value == sensors::write_format_ext(wf)) return wf;
        if constexpr (cc::log_errors) std::cerr << log_error_prefix
          << "unrecognized format \"" << opt_format_value << "\"."
          << std::endl;
        main_mode = MainMode::error;
      }
      return cc::write_format_defaults.at(main_mode);
    }()};

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
      << "  " << args.front() << " \\\n"
        "  [--base-path=<base path>] [--format=<format>] [--] \\\n"
        "  mode [opts...] [--] [args...]\n"
        "\n"
        "  Each mode can be safely interrupted by pressing Ctrl+C or sending a "
          "SIGTERM.\n"
        "\n"
        "  Logging of messages to stderr is separated into info and errors and "
          "enabled or\n"
        "  disabled at compile time. If this binary was built without error "
          "logging, the\n"
        "  process may quit silently when an error has occured. It will still "
          "return a\n"
        "  nonzero exit status, however.\n"
        "\n"
        "  `--base-path` sets the `sensor-logging` repository root path and is "
          "required\n"
        "  for any file IO, as it deliberately has no default value.\n"
        "\n"
        "  `--format` sets the output format. Possible values are `csv` and "
          "`toml`. The\n"
        "  default format depends on the `mode`.\n"
        "\n"
        "Modes:\n"
        "  help\n"
        "    Print this usage message.\n"
        "\n"
        "  print-config\n"
        "    Print a non-exhaustive configuration report, including "
            "compile-time\n"
        "    constants, defaults for the command-line options where "
            "applicable, as well\n"
        "    as some other parameters.\n"
        "\n"
        "  lpd433-listen [--n-bits-min=<n>] [--n-bits-max=<m>] "
          "[--glitch=<t>]\n"
        "    Listen for 433MHz RF transmissions and log to stdout in CSV "
            "format.\n"
        "\n"
        "    Codes with less than <n> or more than <m> bits as well as bit "
            "steps shorter\n"
        "    than <t> µs are ignored.\n"
        "\n"
        "  lpd433-oneshot [--n-bits=<n>] [--n-repeats=<m>] "
          "[--intercode-gap=<t>] \\\n"
        "  [--pulse-length-short=<u>] [--pulse-length-long=<v>] codes...\n"
        "    Send `codes` as 433MHz RF transmissions.\n"
        "\n"
        "    Transmitted codes will have a length of <n> bits, be repeated <m> "
            "times,\n"
        "    with an intercode gap of <t> µs, a short pulse length of <u> µs, "
            "and a long\n"
        "    pulse length of <v> µs.\n"
        "\n"
        "  buzz-oneshot [--time=<t in seconds>] [--frequency=<f in hertz>] \\\n"
        "  [--pulse-width=<pulse width>]\n"
        "    Play a single beep on the buzzer.\n"
        "\n"
        "  control <variable> <setting>\n"
        "    Set an environment control variable manually.\n"
        "\n"
        "  shortly [--now] [--write-control[=<file path>]]\n"
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
        "    If `--write-control` is passed, the parameters and state of the "
            "environment\n"
        "    control circuit are written to stdout, or <file path>, if given.\n"
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
    if (write_format == sensors::WriteFormat::csv) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "`csv` output not supported (implemented) in mode `print-config`."
        << std::endl;
      return cc::exit_code_error;
    } else {
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
        << io::toml::TOMLWrapper{std::make_pair("log_info", cc::log_info)}
        << io::toml::TOMLWrapper{std::make_pair("log_errors", cc::log_errors)}
        << "\n"
        << io::toml::TOMLWrapper{std::make_pair("process", args.front())};
      if (main_opts["base-path"].has_value()) out
        << io::toml::TOMLWrapper{std::make_pair("base_path",
            main_opts["base-path"].value())};
      if (main_opts["format"].has_value()) out
        << io::toml::TOMLWrapper{std::make_pair("format",
            sensors::write_format_ext(write_format))};
      out
        << "\n"
        << io::toml::TOMLWrapper{std::make_pair("time_point_startup",
            time_point_startup)}
        << io::toml::TOMLWrapper{std::make_pair("time_point_last_midnight",
            time_point_last_midnight)}
        << io::toml::TOMLWrapper{std::make_pair("time_point_next_shortly_run",
            time_point_next_shortly_run)}
        << "\n"
        << io::toml::TOMLWrapper{std::make_pair("sampling_interval",
            sampling_interval_in_seconds), "s"}
        << io::toml::TOMLWrapper{std::make_pair("samples_per_aggregate",
            static_cast<signed>(cc::samples_per_aggregate))}
        << io::toml::TOMLWrapper{std::make_pair("aggregates_per_run",
            static_cast<signed>(cc::aggregates_per_run))}
        << io::toml::TOMLWrapper{std::make_pair("run_duration",
            static_cast<signed>(run_duration_in_minutes)),
            "min (calculated from the above 3 parameters)"}
        << "\n"
        << io::toml::TOMLWrapper{std::make_pair("period_system_clock",
          static_cast<double>(std::chrono::system_clock::period::num) /
          static_cast<double>(std::chrono::system_clock::period::den)), "s"}
        << io::toml::TOMLWrapper{std::make_pair("period_high_resolution_clock",
          static_cast<double>(std::chrono::high_resolution_clock::period::num) /
          static_cast<double>(std::chrono::high_resolution_clock::period::den)),
          "s"}
        << "\n"
        << io::toml::TOMLWrapper{std::make_pair("digits_float",
          std::numeric_limits<float>::digits)}
        << io::toml::TOMLWrapper{std::make_pair("digits_double",
          std::numeric_limits<double>::digits)}
        << io::toml::TOMLWrapper{std::make_pair("digits_long_double",
          std::numeric_limits<long double>::digits)}
        << "\n"
        << "[defaults.lpd433.receive]\n"
        << io::toml::TOMLWrapper{std::make_pair("n_bits_min",
          cc::lpd433_receive_n_bits_min_default)}
        << io::toml::TOMLWrapper{std::make_pair("n_bits_max",
          cc::lpd433_receive_n_bits_max_default)}
        << io::toml::TOMLWrapper{std::make_pair("glitch",
          cc::lpd433_receive_glitch_default), "µs"}
        << "\n"
        << "[defaults.lpd433.send]\n"
        << io::toml::TOMLWrapper{std::make_pair("n_bits",
          cc::lpd433_send_n_bits_default)}
        << io::toml::TOMLWrapper{std::make_pair("n_repeats",
          cc::lpd433_send_n_repeats_default)}
        << io::toml::TOMLWrapper{std::make_pair("intercode_gap",
          cc::lpd433_send_intercode_gap_default), "µs"}
        << io::toml::TOMLWrapper{std::make_pair("pulse_length_short",
          cc::lpd433_send_pulse_length_short_default), "µs"}
        << io::toml::TOMLWrapper{std::make_pair("pulse_length_long",
          cc::lpd433_send_pulse_length_long_default), "µs"}
        << "\n"
        << "[defaults.buzzer]\n"
        << io::toml::TOMLWrapper{std::make_pair("time",
          cc::buzz_t_seconds_default), "s"}
        << io::toml::TOMLWrapper{std::make_pair("frequency",
          cc::buzz_f_hertz_default), "Hz"}
        << io::toml::TOMLWrapper{std::make_pair("pulse_width",
          cc::buzz_pulse_width_default)}
        << "\n"
        << "[defaults.format]\n";
        for (auto const &[k, v] : cc::write_format_defaults)
          out << io::toml::TOMLWrapper{std::make_pair(main_mode_name(k),
            sensors::write_format_ext(v))};
        util::for_constexpr([](auto const &sensor, auto const &name,
            auto const &args){
          out << "\n[sensors." << name << "]\n"
            << io::toml::TOMLWrapper{std::make_pair("type",
              sensors::name(sensor))}
            << io::toml::TOMLWrapper{std::make_pair("io_setup_args",
              args)};
          }, cc::blueprint, cc::sensors_physical_instance_names,
          cc::sensors_io_setup_args);
        util::for_constexpr([](auto const &gpio_index_opt, auto const &type){
            if (gpio_index_opt.has_value()) {
              out << "\n[aux_devices." << type << "]\n"
                << io::toml::TOMLWrapper{std::make_pair("gpio_index",
                  gpio_index_opt.value())};
            }
          }, std::make_tuple(
            cc::lpd433_receiver_gpio_index,
            cc::lpd433_transmitter_gpio_index,
            cc::buzzer_gpio_index),
          std::make_tuple("lpd433_receiver", "lpd433_transmitter", "buzzer"));
        out << std::flush;
      }
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
    auto const n_bits_min{util::parse_arg_value(parser, opts, "n-bits-min",
      cc::lpd433_receive_n_bits_min_default)};
    auto const n_bits_max{util::parse_arg_value(parser, opts, "n-bits-max",
      cc::lpd433_receive_n_bits_max_default)};
    auto const glitch{util::parse_arg_value(parser, opts, "glitch",
      cc::lpd433_receive_glitch_default)};

    _433D_rx_CB_t const callback{
      // TODO: This is ugly, because the lambda(s) may not capture
      // `write_format`. Can it be done more elegantly?
      write_format == sensors::WriteFormat::csv ?
        +[](_433D_rx_data_t x){
          std::chrono::system_clock const clock{};
          sensors::write_fields(std::cout, sensors::lpd433_receiver{
            sensors::sample_sensor(clock), {x.code}, {x.bits}, {x.gap}, {x.t0},
            {x.t1}}, sensors::WriteFormat::csv, "");
        } :
        +[](_433D_rx_data_t x){
          std::chrono::system_clock const clock{};
          sensors::write_fields(std::cout, sensors::lpd433_receiver{
            sensors::sample_sensor(clock), {x.code}, {x.bits}, {x.gap}, {x.t0},
            {x.t1}}, sensors::WriteFormat::toml, "");
        }
    };

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    io::LPD433Receiver const
      lpd433_receiver{pi, cc::lpd433_receiver_gpio_index.value(), callback};
    _433D_rx_set_bits(lpd433_receiver, n_bits_min, n_bits_max);
    _433D_rx_set_glitch(lpd433_receiver, glitch);

    sensors::write_field_names(std::cout, sensors::lpd433_receiver{},
      write_format, "");

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
    auto const n_bits{util::parse_arg_value(parser, opts, "n-bits",
      cc::lpd433_send_n_bits_default)};
    auto const n_repeats{util::parse_arg_value(parser, opts, "n-repeats",
      cc::lpd433_send_n_repeats_default)};
    auto const intercode_gap{util::parse_arg_value(parser, opts,
      "intercode-gap", cc::lpd433_send_intercode_gap_default)};
    auto const pulse_length_short{util::parse_arg_value(parser, opts,
      "pulse-length-short", cc::lpd433_send_pulse_length_short_default)};
    auto const pulse_length_long{util::parse_arg_value(parser, opts,
      "pulse-length-long", cc::lpd433_send_pulse_length_long_default)};

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

    io::lpd433_send_oneshot(pi, cc::lpd433_transmitter_gpio_index.value(),
      codes, n_bits, n_repeats, intercode_gap, pulse_length_short,
      pulse_length_long);
  } else if (main_mode == MainMode::buzz_oneshot) {
    if (not cc::buzzer_gpio_index.has_value()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "No buzzer configured in the present binary." << std::endl;
      return cc::exit_code_error;
    }

    flags_t flags{};
    opts_t opts{{"time", {}}, {"frequency", {}}, {"pulse-width", {}}};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());
    auto const parser{+[](std::string const &s){
      return std::stof(s, nullptr); }};
    auto const t_seconds{util::parse_arg_value(parser, opts, "time",
      cc::buzz_t_seconds_default)};
    auto const f_hertz{util::parse_arg_value(parser, opts, "frequency",
      cc::buzz_f_hertz_default)};
    auto const pulse_width{util::parse_arg_value(parser, opts, "pulse-width",
      cc::buzz_pulse_width_default)};

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    buzz_oneshot(pi, cc::buzzer_gpio_index.value(), t_seconds, f_hertz,
      pulse_width);
  } else if (main_mode == MainMode::control) {
    flags_t flags{};
    opts_t opts{};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());

    if (arg_itr >= args.end()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "expected 2 more arguments" << std::endl;
      return cc::exit_code_error;
    }
    auto const &variable{*(arg_itr++)};

    if (arg_itr >= args.end()) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "expected 1 more argument" << std::endl;
      return cc::exit_code_error;
    }
    auto const &setting{*(arg_itr++)};

    auto const var_opt{control::lpd433_control_variable_parse(variable)};
    if (not var_opt.has_value()) return cc::exit_code_error;

    auto const to_opt{util::parse_bool(setting)};
    if (not to_opt.has_value()) return cc::exit_code_error;

    io::Pi const pi{};
    if (io::errored(pi)) return cc::exit_code_error;

    control::set_lpd433_control_variable(pi, var_opt.value(), to_opt.value());

  } else if (main_mode == MainMode::shortly) {
    flags_t flags{{"now", false}, {"write-control", false}};
    opts_t opts{{"write-control", {}}};
    arg_itr = util::get_cmd_args(flags, opts, arg_itr, args.end());

    bool const write_control{
      flags["write-control"] or opts["write-control"].has_value()};

    if (not flags["now"])
      if (interruptible_wait_until(clock, time_point_next_shortly_run))
        return cc::exit_code_interrupt;

    std::chrono::high_resolution_clock const sampling_clock{};
    auto const time_point_filename{clock.now()};
    auto const time_point_reference{sampling_clock.now()};

    std::array<std::ofstream, cc::n_sensors> file_streams;
    auto const print_newlines{[&](){
        std::array<bool, cc::n_sensors> a;
        for (auto &x : a) x = main_opts["base-path"].has_value() or
          write_format != sensors::WriteFormat::csv;
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

    std::ofstream control_file_stream;
    std::ostream * const control_out{opts["write-control"].has_value() ?
      &control_file_stream : &(std::cout)};

    // NOTE: I am not sure if this is even needed. I think all file streams will
    // be closed anyway when they go out of scope.
    auto const close_files{[&](){
        if (main_opts["base-path"].has_value()) for (auto &fs : file_streams)
          if (fs.is_open()) fs.close();
        if (write_control and control_file_stream.is_open())
          control_file_stream.close();
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
      auto const path_dir_shortly{std::filesystem::path{
        main_opts["base-path"].value()} / cc::basename_dir_data /
        cc::basename_dir_shortly};

      if (not util::safe_is_directory(path_dir_shortly))
        return cc::exit_code_error;

      util::for_constexpr([&](auto const &name, auto &fs){
          if (error_during_resource_allocation) return;

          auto const dirname_file{path_dir_shortly / cc::hostname};
          if (not util::safe_create_directory(dirname_file))
            { error_during_resource_allocation = true; return; }

          std::filesystem::path const basename_file{filename_prefix +
            "-" + name + "." + sensors::write_format_ext(write_format)};
          auto const path_file{dirname_file / basename_file};
          if (util::safe_exists(path_file))
            { error_during_resource_allocation = true; return; }

          if (not util::safe_open(fs, path_file, std::ios::out))
            { error_during_resource_allocation = true; return; }

          if constexpr (cc::log_info) std::cerr << log_info_prefix
            << "Log for " << name << " will be written to " << path_file << "."
            << std::endl;
        }, cc::sensors_physical_instance_names, file_streams);
      if (error_during_resource_allocation)
        { close_files(); return cc::exit_code_error; }
    }

    // Open file for writing environment control output
    if (opts["write-control"].has_value()) {
      std::filesystem::path path_file{opts["write-control"].value()};
      if (util::safe_exists(path_file))
        { close_files(); return cc::exit_code_error; }
      if (not util::safe_open(control_file_stream, path_file, std::ios::out))
        { close_files(); return cc::exit_code_error; }
      if constexpr (cc::log_info) std::cerr << log_info_prefix
        << "Control log will be written to " << path_file << "."
        << std::endl;
    }

    // Instantiate control parameters and state
    // NOTE: I have the functionality to (de-)serialize the control parameters
    // in the same way as I do with the control state. I planned on doing this,
    // but now I can't remember any good reason for it. Tuning these parameters
    // is probably best done by changing their respective "default" field in the
    // JSON file and recompiling the binary. And this then allows me to
    // instantiate them as `constexpr`-qualified here:
    control::control_params constexpr control_params{};
    //auto const path_file_control_params_opt{main_opts["base-path"].has_value() ?
    //  std::make_optional(control::path_file_control_params_get(
    //    main_opts["base-path"].value())) :
    //  std::optional<std::filesystem::path>{}};
    auto const path_file_control_state_opt{main_opts["base-path"].has_value() ?
      std::make_optional(control::path_file_control_state_get(
        main_opts["base-path"].value())) :
      std::optional<std::filesystem::path>{}};
    //auto control_params{control::deserialize_or<control::control_params>(
    //  path_file_control_params_opt, "environment control parameters")};
    auto control_state{control::deserialize_or<control::control_state>(
      path_file_control_state_opt, "environment control state")};

    // Initialize sensor IO
    io::Pi const pi{};
    if (io::errored(pi))
      { close_files(); return cc::exit_code_error; }

    error_during_resource_allocation = false;
    auto const sensor_ios{util::map_constexpr(
      [&](auto const &s, auto const &args){
        auto sensor_io{setup_io(s, pi, args)};
        if (io::errored(sensor_io)) error_during_resource_allocation = true;
        return sensor_io;
      }, cc::blueprint, cc::sensors_io_setup_args)};
    if (error_during_resource_allocation)
      { close_files(); return cc::exit_code_error; }

    auto const lpd433_receiver_opt{cc::lpd433_receiver_gpio_index.has_value()
      ? std::optional<io::LPD433Receiver>{
        io::LPD433Receiver{pi, cc::lpd433_receiver_gpio_index.value()}}
      : std::optional<io::LPD433Receiver>{}};
    if (lpd433_receiver_opt.has_value() and
        io::errored(lpd433_receiver_opt.value()))
      { close_files(); return cc::exit_code_error; }

    // Initial output
    util::for_constexpr([&](auto const &s, auto const &name,
          std::ostream * const &out, bool const &print_newline){
        sensors::write_field_names(
            (*out), s, write_format, name, not print_newline); },
      cc::blueprint, cc::sensors_physical_instance_names, outs, print_newlines);

    if (write_control) {
      sensors::write_field_names((*control_out),
        control::as_sensor(control_params, clock), write_format);
      sensors::write_fields((*control_out),
        control::as_sensor(control_params, clock), write_format);
      sensors::write_field_names((*control_out),
        control::as_sensor(control_state, clock), write_format);
    }

    // Start sampling
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

        // NOTE: Turns out `std::future::get` is not marked const.
        auto /*const*/ x_futures{util::map_constexpr(
          [&](auto const &s, auto const &sensor_io){
            return std::async(std::launch::async, [&](){
              return sensors::sample(s, clock, sensor_io); });
          }, cc::blueprint, sensor_ios)};
        auto const xs{util::map_constexpr(
          [&](auto /*const*/ &x_future){ return x_future.get(); }, x_futures)};

        std::tie(aggregate, state) = util::tr(util::map_constexpr([](
              auto const& a, auto const &s, auto const &x){
            return aggregation_step(a, s, x);
          }, aggregate, state, xs));

        if ((sample_index + 1u) == cc::samples_per_aggregate) {
          aggregate = util::map_constexpr([](auto const &a, auto const &s){
              return aggregation_finish(a, s);
            }, aggregate, state);

          util::for_constexpr([&](auto const &a, std::ostream * const &out,
          bool const &print_newline){sensors::write_fields(
              (*out), a, write_format, {}, not print_newline);
            }, aggregate, outs, print_newlines);
        }

        if (write_control) sensors::write_fields((*control_out),
          control::as_sensor(control_state, clock), write_format);
        control_state = control::control_tick(control_state, control_params,
          xs, pi, lpd433_receiver_opt);

        auto const time_point_next_sample{time_point_reference +
          cc::sampling_interval * (aggregate_index * cc::samples_per_aggregate +
          sample_index + 1u)};
        if (interruptible_wait_until(sampling_clock, time_point_next_sample))
          { close_files(); return cc::exit_code_interrupt; }
      }
    }

    //if (path_file_control_params_opt.has_value())
    //  safe_serialize(control_params, path_file_control_params_opt.value());
    if (path_file_control_state_opt.has_value())
      safe_serialize(control_state, path_file_control_state_opt.value());

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
      export_string("COMMAND",
        args.front() + " --base-path=<base_path> [--format=<format>] daily") +
      export_string("FILE_EXTENSION",
        sensors::write_format_ext(write_format)) +
      export_string("HOSTNAME", cc::hostname) +
      export_string("BASE_PATH", path_base.native()) +
      export_string("NAMES", sensors_physical_instance_names_comma_separated) +
      "{ type pypy3 > /dev/null && python_interpreter=pypy3 ||\n"
      "  { type python3 > /dev/null && python_interpreter=python3; }; } &&\n"
      "\"$python_interpreter\" " + quote(daily_py_path.native()) + args_quoted};

    if constexpr (cc::log_info) std::cerr << log_info_prefix
      << "Running the following shell command:\n  "
      << std::regex_replace(command, std::regex{"\\n"}, "\n  ") << std::endl;

    std::cout << std::flush; std::cerr << std::flush;
    auto const status_value{std::system(command.c_str())};
    if (WIFEXITED(status_value)) return WEXITSTATUS(status_value);
    return cc::exit_code_error;
  }

  if (arg_itr < args.end() and *arg_itr == "--") ++arg_itr;
  if constexpr (cc::log_errors) if (arg_itr < args.end()) std::cerr <<
    log_error_prefix << "evaluating trailing arguments starting from `" <<
    *arg_itr << "`." << std::endl;

  return cc::exit_code_success;
} // main()
