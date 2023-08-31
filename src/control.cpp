namespace control {
  // NOTE: This very basic (de-)serialization requires `obj` to satisfy certain
  // constraints. See e.g. `https://stackoverflow.com/a/523933`. Furthermore,
  // the produced data is not cross-platform, but it is not supposed to anyway.

  void serialize(auto const &obj, std::filesystem::path const &path_file) {
    std::ofstream f{path_file, std::ios::out | std::ios::binary};
    f.write(&obj, sizeof(obj));
  }

  auto deserialize(auto &obj, std::filesystem::path const &path_file) {
    std::ifstream f{path_file, std::ios::in | std::ios::binary};
    f.read(reinterpret_cast<decltype(f)::char_type *>(&obj), sizeof(obj));
    return obj;
  }

  void safe_serialize(auto const &obj, std::filesystem::path const &path_file) {
    try {
      serialize(obj, path_file);
    } catch (std::exception const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix << "saving "
        "object as " << path_file << " (" << e.what() << ")." << std::endl;
    }
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

  auto path_dir_hostname_get(auto const &path_base) {
    return std::filesystem::path{path_base} / cc::basename_dir_data /
      cc::basename_dir_shortly / cc::hostname;
  }

  auto path_file_control_state_get(auto const &path_base) {
    return path_dir_hostname_get(path_base) / cc::basename_file_control_state;
  }

  auto path_file_control_params_get(auto const &path_base) {
    return path_dir_hostname_get(path_base) / cc::basename_file_control_params;
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
} // namespace control

#include "control.generated.cpp"

namespace control {
  // NOTE: The code below could probably be written or even generated with all
  // kinds of abstractions and modularity and whatnot. However, since this is
  // just a small-scale hobby project and as this is code that is highly
  // customized and also subject to change, I think it makes sense to keep it
  // simple and just hard-code the functionality like this, where I overload
  // `control_tick` on the respective state and parameter types. It's the case
  // anyway that the `sensor-logging` binary is meant to be configured by
  // hard-coding and compiling.

  auto control_tick(auto const &, control_state_base const &state,
      auto const &) {
    return state;
  }

  auto control_tick(auto const &pi,
      control_state_lasse_raspberrypi_0 const &state, auto const &xs) {
    control_state succ{state};

    auto const set_ventilation{[&](bool const to){
      // TODO: Adjust the values here
      io::lpd433_oneshot_send(pi, cc::lpd433_transmitter_gpio_index.value(),
        {to ? 4474193u : 4474196u}, 24, 20, 9950, 300, 900);
      succ.ventilation = to;
    }};

    return succ;
  }

} // namespace control

