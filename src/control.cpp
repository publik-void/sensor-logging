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
      if (util::safe_readable(path_file)) {
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
      if (std::filesystem::exists(path_file)) {
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
      std::optional<std::string> const &description = {}, T const &val = T{},
      bool const clear = true) {
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
      if (clear) file_clear(path_file_opt.value());
      return opt.value();
    }
    return val;
  }

  cc::timestamp_duration_t get_file_timestamp(
      std::filesystem::path const &path_file) {
    auto last_write_time{std::filesystem::last_write_time(path_file)};
    // NOTE: Looks like `std::chrono::clock_cast` is not supported on these
    // older compilers, so I'll have to depend on `high_resolution_clock` being
    // the same as `system_clock` on the platforms I'm compiling this for.
    // I could alternatively compute the number of seconds since the Unix time
    // epoch manually.
    auto last_write_time_sys{std::chrono::file_clock::to_sys(last_write_time)};
    return std::chrono::duration_cast<cc::timestamp_duration_t>(
      last_write_time_sys.time_since_epoch());
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

  std::ostream& write_config_lpd433_control_variables(std::ostream& out,
      control_state_base const &) {
    return out;
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

  std::optional<std::thread> set_lpd433_control_variable(auto const &pi,
      int const var, auto const &to) {
    return {};
  }

  template <typename var, typename time = float>
  inline std::optional<bool> threshold_controller_tick(
      time const &sampling_interval, var const &input, bool const target,
      var const &threshold_baseline, var const &threshold_gap,
      bool const active_region_is_above, bool const active_state_is_on,
      bool const manual_override = false,
      time * const hold_time_counter = nullptr,
      time const &hold_time_inactive_min = static_cast<time>(0.f),
      time const &hold_time_inactive_max =
        std::numeric_limits<time>::infinity(),
      std::optional<time> const &hold_time_inactive_override = nullptr,
      time const &hold_time_active_min = static_cast<time>(0.f),
      time const &hold_time_active_max = std::numeric_limits<time>::infinity(),
      std::optional<time> const &hold_time_active_override = nullptr) {
    bool const has_hold_time_counter{hold_time_counter != nullptr};
    if (has_hold_time_counter) *hold_time_counter += sampling_interval;

    bool const is_active{target == active_state_is_on};
    time const &hold_time_min{is_active
      ? hold_time_active_min : hold_time_inactive_min};
    time const &hold_time_max{is_active
      ? hold_time_active_max : hold_time_inactive_max};
    std::optional<time> const &hold_time_override{is_active
      ? hold_time_active_override : hold_time_inactive_override};
    var const activating_threshold{threshold_baseline};
    var const deactivating_threshold{threshold_baseline +
      (active_region_is_above ? -threshold_gap : threshold_gap)};
    var const threshold{is_active
      ? deactivating_threshold : activating_threshold};
    bool const current_region_is_below{is_active != active_region_is_above};

    // If a manual override happened, set new hold time if applicable and return
    // immediately, so that the target stays in its state for at least 1 sample
    if (manual_override) {
      if (has_hold_time_counter and hold_time_override.has_value())
        *hold_time_counter = hold_time_min - hold_time_override.value();
      return {};
    }

    if ((((input > threshold) == current_region_is_below) and
          (not has_hold_time_counter or (*hold_time_counter > hold_time_min)))
        or (has_hold_time_counter and *hold_time_counter > hold_time_max)){
      if (has_hold_time_counter) *hold_time_counter = static_cast<time>(0.f);
      return not target;
    } else return {};
  }

  template <typename lpd433_control_variable_t>
  struct lpd433_control_variable_override {
    lpd433_control_variable_t var;
    bool to;
    std::optional<float> hold_time_opt = {};
    bool done = false;
  };

  template <typename lpd433_control_variable_t>
  std::optional<std::thread> set_lpd433_control_variable(auto const &pi,
      auto &state, auto const &params,
      lpd433_control_variable_override<lpd433_control_variable_t> &ovr) {
    if (ovr.done) return {};
    ovr.done = true;
    return set_lpd433_control_variable(pi, state, params, ovr.var, ovr.to);
  }
} // namespace control

#include "control.generated.cpp"

namespace control {
  auto path_file_control_state_get(auto const &path_base) {
    return ((path_dir_hostname_get(path_base) /
        cc::basename_prefix_file_control_state) += "-") +=
      hash_struct_control_state;
  }

  auto path_file_control_params_get(auto const &path_base) {
    return ((path_dir_hostname_get(path_base) /
        cc::basename_prefix_file_control_params) += "-") +=
      hash_struct_control_params;
  }

  auto path_dir_control_triggers_get(auto const &path_base) {
    return ((path_dir_hostname_get(path_base) /
        cc::basename_prefix_dir_control_triggers) += "-") +=
      hash_lpd433_control_variable;
  }

  struct control_trigger {
    lpd433_control_variable var;
    bool to;
    std::chrono::system_clock::time_point when;
    bool daily;
    std::optional<float> hold_time;
  };

  bool is_trigger_time_in_interval(control_trigger const &trigger,
      std::chrono::system_clock::time_point const &start,
      std::chrono::system_clock::time_point const &end) {
    if (trigger.daily) {
      auto const when_days_since_epoch{
        std::chrono::floor<std::chrono::days>(trigger.when).time_since_epoch()};
      auto const start_days_since_epoch{
        std::chrono::floor<std::chrono::days>(start).time_since_epoch()};
      auto const aligned_when {trigger.when  -  when_days_since_epoch};
      auto const aligned_start{start         - start_days_since_epoch};
      auto const aligned_end  {end           - start_days_since_epoch};
      return aligned_start <= aligned_when and aligned_when <= aligned_end;
    } else return start <= trigger.when and trigger.when <= end;
  }

  auto when_gmtime(control_trigger const &self) {
    auto const when_ctime{std::chrono::system_clock::to_time_t(self.when)};
    return std::gmtime(&when_ctime);
  }

  template <std::size_t n>
  std::string const when_strftime(control_trigger const &self,
      std::string const &fmt) {
    char buf[n + 1];
    std::strftime(buf, sizeof(buf), fmt.c_str(), when_gmtime(self));
    buf[n] = '\0';
    return {buf};
  }

  std::string when_date(control_trigger const &self,
      char const &sep = '-') {
    return when_strftime<10>(self, std::string{"%Y"} + sep + "%m" + sep + "%d");
  }

  std::string when_time(control_trigger const &self,
      char const &sep = ':') {
    return when_strftime<8>(self, std::string{"%H"} + sep + "%M" + sep + "%S");
  }

  std::string basename(control_trigger const &self) {
    return {(self.daily ? "daily-" : "single-") +
      (self.daily ? std::string{""} : when_date(self) + "-") +
      when_time(self, '-') + "Z-" + name(self.var) + "-" +
      (self.to ? "on" : "off") + (self.hold_time.has_value() ?
        "-" + std::to_string(*self.hold_time) : "")};
  }

  std::ostream& write_header_as_csv(std::ostream& out,
      control_trigger const &) {
    return out
      << "\"control_triggers_" << control::hostname_c << "_time\", "
      << "\"control_triggers_" << control::hostname_c << "_variable\", "
      << "\"control_triggers_" << control::hostname_c << "_value\", "
      << "\"control_triggers_" << control::hostname_c << "_hold_time\"\n";
  }

  std::ostream& write_as_csv(std::ostream& out,
      control_trigger const &trigger, bool const pad = true) {
    if (not trigger.daily) out << "\"" << when_date(trigger) + "T";
    else {
      if (pad) out << "           ";
      out << "\"";
    }
    out << when_time(trigger) + "Z\", " << "\"" << name(trigger.var) << "\", "
      << io::csv::CSVWrapper(trigger.to) << ", "
      << io::csv::CSVWrapper(trigger.hold_time) << "\n";
    return out;
  }

  std::ostream& write_as_toml(std::ostream& out,
      control_trigger const &trigger) {
    out << "[[control_triggers_" << control::hostname_c << "]]\n";
    if (trigger.daily) out << io::toml::TOMLWrapper{std::make_pair("time",
      io::toml::QuotelessWrapper{when_time(trigger) + "Z"})};
    else out << io::toml::TOMLWrapper{std::make_pair("time",
      io::toml::QuotelessWrapper{when_date(trigger) + " " + when_time(trigger) +
        "Z"})};
    out << io::toml::TOMLWrapper{std::make_pair("variable", name(trigger.var))};
    out << io::toml::TOMLWrapper{std::make_pair("value", trigger.to)};
    out << io::toml::TOMLWrapper{
      std::make_pair("hold_time", trigger.hold_time)};
    return out << "\n";
  }

  bool write(control_trigger const &self, auto const &path_base) {
    if (util::safe_create_directory(path_dir_hostname_get(path_base))) {
      auto const path_dir_control_triggers{
        path_dir_control_triggers_get(path_base)};
      if (util::safe_create_directory(path_dir_control_triggers)) {
        auto const path_file{path_dir_control_triggers / basename(self)};
        bool const success{
          safe_serialize<control_trigger>(self, path_file).has_value()};
        if constexpr (cc::log_info) {
          std::cerr << log_info_prefix
            << "Trigger written successfully to `" << path_file
            << "` with data: ";
          write_as_csv(std::cerr, self, false) << std::flush;
        }
        return success;
      }
    }
    return false;
  }

  std::vector<control_trigger> read_triggers(auto const &path_base) {
    auto const path_dir_control_triggers{
      path_dir_control_triggers_get(path_base)};
    std::vector<control_trigger> triggers{};
    if (std::filesystem::is_directory(path_dir_control_triggers))
      for (auto const &entry :
          std::filesystem::directory_iterator{path_dir_control_triggers}) {
        auto const entry_basename{entry.path().filename().string()};
        if (entry_basename.rfind("single-", 0) == 0 or
            entry_basename.rfind("daily-", 0) == 0) {
          auto const trigger_opt{
            safe_deserialize<control_trigger>(entry.path())};
          if (trigger_opt.has_value()) triggers.push_back(trigger_opt.value());
        }
      }
    return triggers;
  }

  struct control_trigger_pending {
    control_trigger trigger;
    bool pending = true;
  };

  std::vector<control_trigger_pending> get_pending_control_triggers(
      std::vector<control_trigger> const &triggers,
      std::chrono::system_clock::time_point const &start_time,
      auto const &duration_shortly_run) {
    auto const end_time{start_time + duration_shortly_run};
    std::vector<control_trigger_pending> triggers_pending{};
    for (auto const &trigger : triggers) {
      control_trigger const trigger_offset{trigger.var, trigger.to,
        trigger.when + cc::trigger_time_safety_offset, trigger.daily,
        trigger.hold_time};

      if (is_trigger_time_in_interval(trigger_offset, start_time, end_time)) {
        triggers_pending.emplace_back(trigger_offset);
        if constexpr (cc::log_info) {
          std::cerr << log_info_prefix << "Trigger pending with data: ";
          control::write_as_csv(std::cerr, trigger_offset) << std::flush;
        }
      }
    }
    return triggers_pending;
  }

  std::vector<lpd433_control_variable_override<
    lpd433_control_variable>> trigger_tick(
      std::vector<control_trigger_pending> &triggers_pending,
      std::chrono::system_clock::time_point const &last,
      std::chrono::system_clock::time_point const &now) {
    std::vector<lpd433_control_variable_override<
      lpd433_control_variable>> overrides{};
    for (auto &trigger_pending : triggers_pending) {
      auto const &trigger{trigger_pending.trigger};
      if (trigger_pending.pending and
          is_trigger_time_in_interval(trigger, last, now)) {
        overrides.emplace_back(trigger.var, trigger.to, trigger.hold_time);
        trigger_pending.pending = false;
      }
    }
    return overrides;
  }

  // NOTE: The code below could probably be written or even generated with all
  // kinds of abstractions and modularity and whatnot. However, since this is
  // just a small-scale hobby project and as this is code that is highly
  // customized and also subject to change, I think it makes sense to keep it
  // simple and just hard-code the functionality like this, where I overload
  // `control_tick` on the respective state and parameter types. It's the case
  // anyway that the `sensor-logging` binary is meant to be configured by
  // hard-coding and compiling.
  // Edit: Well, in the end I ended up with a bunch of generated code after all.

  auto control_tick(auto const &state, auto const &params, auto const &,
      auto const &pi, std::optional<io::LPD433Receiver> const &,
      auto &overrides) {
    auto succ{state};
    for (auto &ovr : overrides)
      set_lpd433_control_variable(pi, succ, params, ovr);
    return succ;
  }

  auto control_tick(
      control_state_lasse_raspberrypi_1 const &state,
      control_params_lasse_raspberrypi_1 const &params,
      auto const &xs, auto const &pi,
      std::optional<io::LPD433Receiver> const &lpd433_receiver_opt,
      std::vector<lpd433_control_variable_override<
        lpd433_control_variable_lasse_raspberrypi_1>> &overrides) {
    float constexpr sampling_rate{
      static_cast<float>(decltype(cc::sampling_interval)::period::den) / (
      static_cast<float>(decltype(cc::sampling_interval)::period::num) *
      static_cast<float>(cc::sampling_interval.count()))};
    float constexpr sampling_interval{1.f / sampling_rate};

    control_state_lasse_raspberrypi_1 succ{state};

    auto const sensor_update{update_from_sensors(xs, succ, params)};
    auto const lpd433_update{
      update_from_lpd433(pi, lpd433_receiver_opt, succ, sampling_interval)};
    if (lpd433_update.has_value()) overrides.emplace_back(
      lpd433_update.value().first, lpd433_update.value().second);
    threshold_controller_tick(pi, sampling_interval, succ, params, overrides);
    for (auto &ovr : overrides)
      set_lpd433_control_variable(pi, succ, params, ovr);

    return succ;
  }

} // namespace control

