namespace control {
  // NOTE: This very basic (de-)serialization requires `obj` to satisfy certain
  // constraints. See e.g. `https://stackoverflow.com/a/523933`. Furthermore,
  // the produced data is not cross-platform, but it is not supposed to anyway.

  auto serialize(auto const &obj, std::filesystem::path const &path_file) {
    std::ofstream f{path_file, std::ios::out | std::ios::binary};
    f.write(reinterpret_cast<decltype(f)::char_type const *>(&obj),
      sizeof(obj));
    return obj;
  }

  auto deserialize(auto &obj, std::filesystem::path const &path_file) {
    std::ifstream f{path_file, std::ios::in | std::ios::binary};
    f.read(reinterpret_cast<decltype(f)::char_type *>(&obj), sizeof(obj));
    return obj;
  }

  template <typename T>
  std::optional<T> safe_serialize(T const &obj,
      std::filesystem::path const &path_file) {
    try { serialize(obj, path_file);
    } catch (std::exception const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix << "saving "
        "object as " << path_file << " (" << e.what() << ")." << std::endl;
      return {obj};
    }
    return {};
  }

  template <typename T>
  std::optional<T> safe_deserialize(std::filesystem::path const &path_file) {
    try {
      if (std::filesystem::exists(path_file)) {
        // NOTE: Checking for the correct file size does not guarantee no errors
        // for various reasons (e.g. file size could still be changed in
        // between, file could store data of a different struct type, etc.). It
        // may however still help avoid a good fraction of any potential errors.
        auto file_size{std::filesystem::file_size(path_file)};
        if (file_size == sizeof(T)) {
          T obj;
          return {deserialize(obj, path_file)};
        } else
          if constexpr (cc::log_errors) std::cerr << log_error_prefix
            << "loading object from " << path_file
            << ": file exists, but has wrong size." << std::endl;
      }
    } catch (std::exception const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix << "loading "
        "object from " << path_file << " (" << e.what() << ")." << std::endl;
    }
    return {};
  }

  void file_clear(std::filesystem::path const &path_file) {
    try {
      if (exists(path_file)) {
        std::filesystem::remove(path_file);
      }
    } catch (std::filesystem::filesystem_error const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "making sure " << path_file << " is non-existent ("
        << e.what() << ")." << std::endl;
    }
  }

  template <typename T>
  T deserialize_or(std::optional<std::filesystem::path> const &path_file_opt,
      std::optional<std::string> const &description = {}, T const &val = T{}) {
    auto const opt{[&](){
      if (path_file_opt.has_value())
        return safe_deserialize<T>(path_file_opt.value());
      return std::optional<T>{};
    }()};
    if constexpr (cc::log_info) std::cerr << log_info_prefix
      << "Deserialization"
      << (description.has_value() ? " of " + description.value() : "")
      << (path_file_opt.has_value()
        ? " from " + path_file_opt.value().native() : "")
      << (opt.has_value() ? " succeeded." : " failed – using default.")
      << std::endl;
    if (opt.has_value()) {
      file_clear(path_file_opt.value());
      return opt.value();
    }
    return val;
  }

  auto path_dir_hostname_get(auto const &path_base) {
    return std::filesystem::path{path_base} / cc::basename_dir_data /
      cc::basename_dir_shortly / cc::hostname;
  }

  struct control_state_base {};
  struct control_params_base {};

  // For these overloads/template specializations (`control_state_base`/
  // `control_params_base`, i.e.  no device control and empty state and
  // parameter structs), don't load/save anything, only make sure that no files
  // are present.

  void safe_serialize(control_state_base const &,
      std::filesystem::path const &path_file) {
    file_clear(path_file);
  }

  void safe_serialize(control_params_base const &,
      std::filesystem::path const &path_file) {
    file_clear(path_file);
  }

  template<> std::optional<control_state_base> safe_deserialize<
      control_state_base>(std::filesystem::path const &path_file) {
    file_clear(path_file); return {};
  }

  template<> std::optional<control_params_base> safe_deserialize<
      control_params_base>(std::filesystem::path const &path_file) {
    file_clear(path_file); return {};
  }

  sensors::sensor as_sensor(control_state_base const &,
      std::optional<cc::timestamp_duration_t> const &timestamp = {}) {
    return {timestamp};
  }

  sensors::sensor as_sensor(control_params_base const &,
      std::optional<cc::timestamp_duration_t> const &timestamp = {}) {
    return {timestamp};
  }

  //auto as_sensor(auto const &x, auto const &clock) {
  auto as_sensor(auto const &x, std::chrono::system_clock const &clock) {
    return as_sensor(x, std::chrono::duration_cast<cc::timestamp_duration_t>(
      clock.now().time_since_epoch()));
  }

  std::thread set_lpd433_control_variable(auto const &pi, bool const to,
      std::uint64_t const code_off, std::uint64_t const code_on,
      int const n_bits, int const n_repeats, int const intercode_gap,
      int const pulse_length_short, int const pulse_length_long) {
    return io::lpd433_send_oneshot(pi,
      cc::lpd433_transmitter_gpio_index.value(), {to ? code_on : code_off},
      n_bits, n_repeats, intercode_gap, pulse_length_short,
      pulse_length_long);
  }

  void set_lpd433_control_variable(auto &control_variable, auto const &to,
      auto &ignore_time_counter, auto const ignore_time) {
    control_variable = to;
    ignore_time_counter = ignore_time;
  }

  std::thread set_lpd433_control_variable(auto const &pi,
      bool &control_variable, bool const to, auto &ignore_time_counter,
      auto const ignore_time, std::uint64_t const code_off,
      std::uint64_t const code_on, int const n_bits, int const n_repeats,
      int const intercode_gap, int const pulse_length_short,
      int const pulse_length_long) {
    set_lpd433_control_variable(control_variable, to, ignore_time_counter,
      ignore_time);
    return set_lpd433_control_variable(pi, to, code_off, code_on, n_bits,
      n_repeats, intercode_gap, pulse_length_short, pulse_length_long);
  }
} // namespace control

#include "control.generated.cpp"

namespace control {
  auto path_file_control_state_get(auto const &path_base) {
    return ((path_dir_hostname_get(path_base) /
      cc::basename_file_control_state) += "-") += hash_struct_control_state;
  }

  auto path_file_control_params_get(auto const &path_base) {
    return ((path_dir_hostname_get(path_base) /
      cc::basename_file_control_params) += "-") += hash_struct_control_params;
  }

  // NOTE: The code below could probably be written or even generated with all
  // kinds of abstractions and modularity and whatnot. However, since this is
  // just a small-scale hobby project and as this is code that is highly
  // customized and also subject to change, I think it makes sense to keep it
  // simple and just hard-code the functionality like this, where I overload
  // `control_tick` on the respective state and parameter types. It's the case
  // anyway that the `sensor-logging` binary is meant to be configured by
  // hard-coding and compiling.

  auto control_tick(control_state_base const &state,
      control_params_base const &, auto const &, auto const &,
      std::optional<io::LPD433Receiver> const &) {
    return state;
  }

  auto control_tick(
      control_state_lasse_raspberrypi_1 const &state,
      control_params_lasse_raspberrypi_1 const &params,
      auto const &xs, auto const &pi,
      std::optional<io::LPD433Receiver> const &lpd433_receiver_opt) {
    float constexpr sampling_rate{
      static_cast<float>(decltype(cc::sampling_interval)::period::den) / (
      static_cast<float>(decltype(cc::sampling_interval)::period::num) *
      static_cast<float>(cc::sampling_interval.count()))};
    float constexpr sampling_interval{1.f / sampling_rate};

    control_state succ{state};

    auto const update{
      update_from_lpd433(pi, lpd433_receiver_opt, succ, sampling_interval)};

    // TODO: Generate this too ("update from sensors")?
    auto const &mhz19_0{cc::get_sensor<"mhz19_0">(xs)};
    auto const &dht22_2{cc::get_sensor<"dht22_2">(xs)};
    succ.co2_concentration =
      mhz19_0.co2_concentration.value_or(state.co2_concentration);
    succ.humidity = dht22_2.humidity.value_or(state.humidity);

    //set_ventilation(pi, succ, params, not succ.ventilation);

    return succ;
  }

} // namespace control

