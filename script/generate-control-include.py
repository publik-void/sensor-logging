from common import (indent, write_generated_cpp_file, header_sep,
  load_and_expand_jsons, dedent, hash)

def snippet_enum_lpd433_control_variable_type_alias():
  return f'using lpd433_control_variable_underlying_t = std::uint8_t;\n'

def snippet_struct_declaration(host_identifier, struct_fields, type_identifier):
  str = (f'struct control_{type_identifier}_{host_identifier} : public '
    f'control_{type_identifier}_base {{\n')
  for field in struct_fields:
    initializer = f'{{{field["default"]}}}' if "default" in field else ""
    str += indent(f'{field["type"]} {field["name"]}{initializer};\n')
  str += f'\n'
  str += indent(f'static std::unordered_map<std::string, '
    f'std::optional<std::string>>\n')
  str += indent(f'get_empty_opts() {{\n', 3)
  str += indent(f'return {{\n', 2)
  for field in struct_fields:
    name_hyphenized = field["name"].replace("_", "-")
    str += indent(f'{{{{"{name_hyphenized}"}}, {{}}}},\n', 3)
  str += indent(f'}};\n', 2)
  str += indent(f'}}\n')
  str += f'}};\n'

  definition_hash = hash(str)
  str += f'\n'
  str += (f'std::string_view constexpr hash_struct_control_'
    f'{type_identifier}_{host_identifier}{{\n')
  str += indent(f'"{definition_hash}"}};\n')
  return str

def snippet_apply_opts(host_identifier, struct_fields, type_identifier):
  str = f'control_{type_identifier}_{host_identifier} apply_opts(\n'
  str += indent(f'std::unordered_map<std::string, std::optional<std::string>> '
    f'&opts,\n', 2)
  str += indent(f'control_{type_identifier}_{host_identifier} const '
    f'&{type_identifier} = {{}}) {{\n', 2)
  str += indent(f'return {{{{}},\n')
  for field in struct_fields:
    name_hyphenized = field["name"].replace("_", "-")
    str += indent(
      f'util::parse_arg_value(util::{field["type"]}_parser, opts,\n', 2)
    str += indent(f'"{name_hyphenized}",\n', 3)
    str += indent(f'{type_identifier}.{field["name"]}),\n', 3)
  str += indent(f'}};\n')
  str += f'}}\n'
  return str

def snippet_as_sensor(host_identifier, struct_fields, type_identifier):
  str = f'sensors::control_{type_identifier}_{host_identifier} as_sensor(\n'
  str += indent(f'control_{type_identifier}_{host_identifier} '
    f'const &{type_identifier},\n'
    f'std::optional<cc::timestamp_duration_t> const &timestamp = {{}}) {{\n', 2)
  str += indent(f'return {{as_sensor(static_cast<control_'
    f'{type_identifier}_base>({type_identifier}), timestamp),\n')
  for field in struct_fields:
    str += indent(f'{{{type_identifier}.{field["name"]}}},\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_enum_lpd433_control_variable(host_identifier, host_structs):
  str = f'enum struct lpd433_control_variable_{host_identifier} :\n'
  str += indent(f'lpd433_control_variable_underlying_t {{\n', 2)
  i = 0
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      str += indent(f'{field["name"]} = {i},\n')
      i += 1
  str += f'}};\n'

  definition_hash = hash(str)
  str += f'\n'
  str += (f'std::string_view constexpr hash_lpd433_control_variable_'
    f'{host_identifier}{{\n')
  str += indent(f'"{definition_hash}"}};\n')
  return str

def snippet_lpd433_control_variable_name(host_identifier, host_structs):
  maybe_unused_str = ("" if
    any("lpd433" in field for field in host_structs["struct_state"]) else
    "[[maybe_unused]] ")

  str = (f'std::string name({maybe_unused_str}lpd433_control_variable_'
    f'{host_identifier} const var) {{\n')
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      str += indent(f'if (var == lpd433_control_variable_{host_identifier}::'
        f'{field["name"]})\n')
      str += indent(f'return {{"{field["name"]}"}};\n', 2)
  str += indent(f'return {{""}};\n')
  return str + f'}}\n'

def snippet_write_config_lpd433_control_variables(host_identifier, host_structs):
  maybe_unused_str = ("" if
    any("lpd433" in field for field in host_structs["struct_state"]) else
    "[[maybe_unused]] ")
  str = f'std::ostream& write_config_lpd433_control_variables(std::ostream& out,\n'
  str += indent(f'{maybe_unused_str}control_state_{host_identifier} '
    f'const &default_state) {{\n', 2)
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      str += indent(f'out << "\\n" << '
        f'"[lpd433_control_variables.{field["name"]}]\\n";\n')
      pairs = field["lpd433"]
      pairs["default"] = f'default_state.{field["name"]}'
      for k, v in pairs.items():
        str += indent(f'out << io::toml::TOMLWrapper{{std::make_pair(\n')
        str += indent(f'"{k}", {v})}};\n', 2)
  str += indent(f'return out;\n')
  return str + f'}}\n'

def snippet_lpd433_control_variable_parse_per_host(host_identifier, host_structs):
  maybe_unused_str = ("" if
    any("lpd433" in field for field in host_structs["struct_state"]) else
    "[[maybe_unused]] ")

  str = (f'std::optional<lpd433_control_variable_{host_identifier}>\n')
  str += indent(f'lpd433_control_variable_parse_{host_identifier}(\n'
    f'{maybe_unused_str}auto const &name) {{\n', 2)
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      str += indent(f'if (name == "{field["name"]}")\n')
      str += indent(f'return lpd433_control_variable_{host_identifier}::'
        f'{field["name"]};\n', 2)
  str += indent(f'if constexpr (cc::log_errors) std::cerr << '
    f'log_error_prefix\n')
  str += indent(f'<< "unrecognized control variable \\"" << name << "\\"." '
    f'<< std::endl;\n', 2)
  str += indent(f'return {{}};\n')
  return str + f'}}\n'

def snippet_update_from_lpd433(host_identifier, host_structs):
  has_ignore_time = (
    any(param["name"] == "lpd433_ignore_time"
      for param in host_structs["struct_params"]) and
    any(state["name"] == "lpd433_ignore_time_counter"
      for state in host_structs["struct_state"]))
  maybe_unused_str = "" if has_ignore_time else "[[maybe_unused]] "
  str = (f'std::optional<std::pair<lpd433_control_variable_{host_identifier}, '
    f'bool>>\n')
  str += indent(
    f'update_from_lpd433(auto const &pi,\n'
    f'std::optional<io::LPD433Receiver> const &lpd433_receiver_opt,\n'
    f'{maybe_unused_str}control_state_{host_identifier} &state,\n'
    f'{maybe_unused_str}auto const &sampling_interval) {{\n', 2)
  str += indent(dedent(f'''\
    if (not lpd433_receiver_opt.has_value()) return {{}};

    auto const &lpd433_receiver{{lpd433_receiver_opt.value()}};
    bool const lpd433_receiver_ready{{_433D_rx_ready(lpd433_receiver) != 0}};

    '''))

  if has_ignore_time:
    str += indent(dedent(f'''\
      decltype(state.lpd433_ignore_time_counter) const zero{{0}};
      state.lpd433_ignore_time_counter -= sampling_interval;
      if (state.lpd433_ignore_time_counter <= zero)
        state.lpd433_ignore_time_counter = zero;

      '''))

  str += indent(dedent(f'''\
    if (lpd433_receiver_ready) {{
      _433D_rx_data_t data;
      _433D_rx_data(lpd433_receiver, &data);

    '''), 1)

  if has_ignore_time:
    str += indent(dedent(f'''\
      if (state.lpd433_ignore_time_counter <= zero) {{
      '''), 2)
  else:
    str += f'\n'

  str += indent(dedent(f'''\
    bool recognized{{true}};
    auto buzz_t_seconds{{cc::buzz_t_seconds_default}};
    auto buzz_f_hertz{{cc::buzz_f_hertz_default}};
    auto buzz_pulse_width{{cc::buzz_pulse_width_default}};
    lpd433_control_variable_{host_identifier} var;
    bool to;

    '''), 2 + has_ignore_time)

  if_branches_present = False
  is_first_entry = True
  def buzz_customizations(field):
    return f''.join(indent(f'buzz_{arg} = {field["lpd433"][f"buzz_{arg}"]};\n',
        3 + has_ignore_time)
      for arg in ["t_seconds", "f_hertz", "pulse_width"]
      if f'buzz_{arg}' in field["lpd433"])
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      str += indent(f'{" else " if not is_first_entry else ""}if (data.code =='
        f' {field["lpd433"]["code_off"]}u) {{\n',
          (2 + has_ignore_time) if is_first_entry else 0)
      str += indent(f'var = lpd433_control_variable_{host_identifier}::'
        f'{field["name"]};\n', 3 + has_ignore_time)
      str += indent(f'to = false;\n', 3 + has_ignore_time)
      str += indent(f'state.{field["name"]} = false;\n', 3 + has_ignore_time)
      str += buzz_customizations(field)
      str += indent(f'}} else if (data.code == {field["lpd433"]["code_on"]}u) '
        f'{{\n', 2 + has_ignore_time)
      str += indent(f'var = lpd433_control_variable_{host_identifier}::'
        f'{field["name"]};\n', 3 + has_ignore_time)
      str += indent(f'to = true;\n', 3 + has_ignore_time)
      str += indent(f'state.{field["name"]} = true;\n', 3 + has_ignore_time)
      str += buzz_customizations(field)
      str += indent(f'}}', 2 + has_ignore_time)
      is_first_entry = False
      if_branches_present = True
  str += indent(f'{" else " if if_branches_present else ""}recognized = false;'
    f'\n',
    (2 + has_ignore_time) if not if_branches_present else 0)
  str += indent(dedent(f'''
        if (recognized) {{
          if constexpr (cc::buzzer_gpio_index.has_value())
            io::buzz_oneshot(pi, cc::buzzer_gpio_index.value(),
              buzz_t_seconds, buzz_f_hertz, buzz_pulse_width);
          if constexpr (cc::log_info) std::cerr << log_info_prefix
            << "recognized external LPD433 command: " << name(var) << " "
            << (to ? "on" : "off") << "." << std::endl;
          return {{{{var, to}}}};
        }}
      }}
      '''), has_ignore_time + 1)
  if has_ignore_time:
    str += indent(f'}}\n')
  str += indent(dedent(f'''\
      return {{}};
    }}
    '''), 0)
  return str

def snippet_update_from_sensors(host_identifier, host_structs):
  str = f'bool update_from_sensors(auto const &xs,\n'
  str += indent(f'control_state_{host_identifier} &succ,\n', 2)
  str += indent(f'control_params_{host_identifier} const &params) {{\n', 2)
  str += indent(f'bool has_updated{{false}};\n')
  str += f'\n'
  for sensor_input in host_structs["sensor_inputs"]:
    variable_name = sensor_input["name"]
    physical_name = sensor_input["sensor_physical_instance_name"]
    input_name = physical_name + "_" + variable_name
    str += indent(f'auto const &{input_name}_opt{{\n')
    str += indent(f'cc::get_sensor<"{physical_name}">(xs).{variable_name}}};'
      f'\n', 2)
    str += indent(f'if ({input_name}_opt.has_value()) {{\n', 1)
    str += indent(f'succ.{input_name} =\n', 2)
    str += indent(f'{input_name}_opt.value();\n', 3)
    if "min" in sensor_input:
      str += indent(f'succ.{input_name} =\n', 2)
      str += indent(f'std::max(succ.{input_name},\n', 3)
      str += indent(f'params.{input_name}_min);\n', 4)
    if "max" in sensor_input:
      str += indent(f'succ.{input_name} =\n', 2)
      str += indent(f'std::min(succ.{input_name},\n', 3)
      str += indent(f'params.{input_name}_max);\n', 4)
    str += indent(f'has_updated = true;\n', 2)
    str += indent(f'}}\n', 1)
    str += f'\n'
  str += indent(f'return has_updated;\n')
  return str + f'}}\n'

def snippet_set_lpd433_control_variable_per_host(host_identifier, host_structs):
  str = f''
  has_ignore_time = (
    any(param["name"] == "lpd433_ignore_time"
      for param in host_structs["struct_params"]) and
    any(state["name"] == "lpd433_ignore_time_counter"
      for state in host_structs["struct_state"]))
  first_entry = True
  for field in host_structs["struct_state"]:
    if "lpd433" in field:
      if not first_entry:
        str += "\n"
      else:
        first_entry = False
      lpd433 = field["lpd433"]
      def get(arg):
        if arg in lpd433:
          return lpd433[arg]
        else:
          return f'cc::lpd433_send_{arg}_default'
      str += f'std::thread set_{field["name"]}_{host_identifier}(auto const &pi,\n'
      str += indent(f'auto const &to) {{\n', 2)
      str += indent(f'return set_lpd433_control_variable(pi, to,\n')
      str += indent(
        f'{lpd433["code_off"]}u, {lpd433["code_on"]}u,\n'
        f'{get("n_bits")},\n'
        f'{get("n_repeats")},\n'
        f'{get("intercode_gap")},\n'
        f'{get("pulse_length_short")},\n'
        f'{get("pulse_length_long")});\n', 2)
      str += f'}}\n'
      str += f'\n'
      str += f'std::thread set_{field["name"]}(auto const &pi,\n'
      str += indent(f'control_state_{host_identifier} &state,\n'
        f'control_params_{host_identifier} const &params,\n'
        f'auto const &to) {{\n', 2)
      str += indent(f'set_lpd433_control_variable(state.{field["name"]}, to,\n')
      str += indent(
        f'{"state.lpd433_ignore_time_counter" if has_ignore_time else "0"}, '
        f'{"params.lpd433_ignore_time" if has_ignore_time else "0"});\n', 2)
      str += indent(f'return set_{field["name"]}_{host_identifier}(pi, to);\n')
      str += f'}}\n'

  has_control_variables = any(
    "lpd433" in field for field in host_structs["struct_state"])
  pi, state, params, var, to = map(lambda x: x if has_control_variables else "",
    ["pi", "state", "params", " var", "to"])

  for with_state in [False, True]:
    if not first_entry:
      str += f'\n'

    str += (f'std::optional<std::thread> '
      f'set_lpd433_control_variable(auto const &{pi},\n')
    if with_state:
      str += indent(f'control_state_{host_identifier} &{state},\n'
        f'control_params_{host_identifier} const &{params},\n', 2)
    str += indent(f'lpd433_control_variable_{host_identifier} const{var}, '
      f'auto const &{to}) {{\n', 2)

    for field in host_structs["struct_state"]:
      if "lpd433" in field:
        host_qualifier = "" if with_state else f"_{host_identifier}"
        state_args = "state, params, " if with_state else ""
        str += indent(dedent(f'''\
          if (var == lpd433_control_variable_{host_identifier}::{field["name"]})
            return {{set_{field["name"]}{host_qualifier}(pi, {state_args}to)}};
          '''))

    str += indent(f'return {{}};\n')
    str += f'}}\n'
  return str

def snippet_threshold_controller_tick(host_identifier, host_structs):
  str = f'void threshold_controller_tick(auto const &pi,\n'
  str += indent(dedent(f'''\
    float const sampling_interval, control_state_{host_identifier} &succ,
    control_params_{host_identifier} const &params,
    std::vector<std::tuple<lpd433_control_variable_{host_identifier}, bool,
    '''), 2)
  str += indent(f'std::optional<float>>> const overrides) {{\n', 3)
  for threshold_spec in host_structs["thresholds"]:
    str += indent(f'std::optional<std::pair<bool, std::optional<float>>>\n')
    str += indent(f'override_{threshold_spec["target"]}{{}};\n', 2)
  str += indent(f'for (auto &[var, to, hold_time_opt] : overrides) {{\n')
  for threshold_spec in host_structs["thresholds"]:
    str += indent(f'if (var == lpd433_control_variable_{host_identifier}'
      f'::{threshold_spec["target"]})\n', 2)
    str += indent(f'override_{threshold_spec["target"]} = '
      f'{{to, hold_time_opt}};\n', 3)
  str += indent(f'}}\n')
  for threshold_spec in host_structs["thresholds"]:
    target, variable = threshold_spec["target"], threshold_spec["variable"]
    target_by_variable = f'{target}_by_{variable}'
    str += f'\n'
    str += indent(f'std::optional<bool> const update_{target}{{'
      f'threshold_controller_tick(\n')
    str += indent(dedent(f'''\
      sampling_interval, succ.{variable},
      succ.{target},
      params.{target_by_variable}_threshold,
      params.{target_by_variable}_threshold_gap,
      params.{target_by_variable}_active_region_is_above,
      params.{target_by_variable}_active_state_is_on,
      override_{target}.has_value(),
      &(succ.{target_by_variable}_hold_time_counter),
      '''), 2)
    for prefix in ["in", ""]:
      if f"hold_time_{prefix}active_min" in threshold_spec:
        str += indent(
          f'{threshold_spec[f"hold_time_{prefix}active_min"]},\n', 2)
      else:
        str += indent(f'0.f,\n', 2)
      if f"hold_time_{prefix}active_max" in threshold_spec:
        str += indent(
          f'{threshold_spec[f"hold_time_{prefix}active_max"]},\n', 2)
      else:
        str += indent(f'std::numeric_limits<float>::infinity(),\n', 2)
      str += indent(f'override_{target}.has_value() and\n', 2)
      str += indent(f'override_{target}.value().second.has_value()\n', 3)
      str += indent(f'? std::optional<float>{{override_{target}.value().'
        f'second.value()}}\n', 4)
      if f"hold_time_{prefix}active_override" in threshold_spec:
        str += indent(f': std::optional<float>{{'
          f'{threshold_spec[f"hold_time_{prefix}active_override"]}}}', 4)
      else:
        str += indent(f': {{}}', 4)
      str += f',\n' if prefix == "in" else f')}};\n'
    str += indent(f'if (update_{target}.has_value())\n')
    str += indent(
      f'set_{target}(pi, succ, params, update_{target}.value());\n', 2)
  return str + f'}}\n'

def snippet_maker_type_conditionals(control_structs, name, default):
  str = default
  for host_identifier, _ in reversed(control_structs.items()):
    str = (f'std::conditional<cc::host == cc::Host::{host_identifier},\n'
      f'{name}_{host_identifier},\n'
      f'{str}>::type')
  return f'using {name} =\n' + indent(f'{str};\n')

def snippet_maker_hashes(control_structs, name):
  str = f'std::string_view{{""}}'
  for host_identifier, _ in reversed(control_structs.items()):
    str = (f'cc::host == cc::Host::{host_identifier} ?\n'
      f'hash_{name}_{host_identifier} :\n' +
      str)
  return (f'std::string_view constexpr hash_{name}'
    f'{{\n' + indent(f'{str}\n') + f'}};\n')

def snippet_type_conditionals_by_struct_type(control_structs, type_identifier):
  return snippet_maker_type_conditionals(control_structs,
    f'control_{type_identifier}', f'control_{type_identifier}_base')

def snippet_hashes_by_struct_type(control_structs, type_identifier):
  return snippet_maker_hashes(control_structs,
    f'struct_control_{type_identifier}')

def snippet_type_conditionals(control_structs):
  return snippet_maker_type_conditionals(control_structs,
    f'lpd433_control_variable', f'lpd433_control_variable_underlying_t')

def snippet_hashes(control_structs):
  return snippet_maker_hashes(control_structs,
    f'lpd433_control_variable')

def snippet_lpd433_control_variable_parse(control_structs):
  str = f'auto lpd433_control_variable_parse(auto const &name) {{\n'
  is_first_entry = True
  for host_identifier, _ in control_structs.items():
    str += indent(("" if is_first_entry else "else ") +
      f'if constexpr (cc::host == cc::Host::{host_identifier})\n')
    str += indent(f'return lpd433_control_variable_parse_{host_identifier}('
      f'name);\n', 2)
    if is_first_entry:
      is_first_entry = False
  str_default_case = indent(f'return std::make_optional(0);\n')
  if bool(control_structs.items()):
    str += indent(f'else\n' + str_default_case)
  else:
    str += str_default_case
  return str + f'}}\n'

def snippet_set_lpd433_control_variable(control_structs):
  return dedent(f'''\
    // Placeholder for `snippet_set_lpd433_control_variable`, i.e. something
    // like `set_ventilation` without state args and without host qualifier in
    // the name, so that it dispatches to the `cc:host`. I'm not sure I need a
    // function like that at the moment.
    ''')

def control_cpp_include(control_structs, sensors):
  str, sep = header_sep(__file__)

  str += f'namespace control {{\n' + sep

  type_identifiers = ["state", "params"]

  str += snippet_enum_lpd433_control_variable_type_alias() + sep

  for snippet in [snippet_struct_declaration, snippet_apply_opts,
      snippet_as_sensor]:
    for host_identifier, host_structs in control_structs.items():
      for type_identifier in type_identifiers:
        struct_fields = host_structs[f'struct_{type_identifier}']
        str += indent(snippet(host_identifier, struct_fields, type_identifier)
          + sep)

  for snippet in [snippet_enum_lpd433_control_variable,
      snippet_lpd433_control_variable_name,
      snippet_write_config_lpd433_control_variables,
      snippet_lpd433_control_variable_parse_per_host,
      snippet_update_from_lpd433, snippet_update_from_sensors,
      snippet_set_lpd433_control_variable_per_host,
      snippet_threshold_controller_tick]:
    for host_identifier, host_structs in control_structs.items():
      str += indent(snippet(host_identifier, host_structs) + sep)

  for snippet in [snippet_type_conditionals_by_struct_type,
      snippet_hashes_by_struct_type]:
    for type_identifier in type_identifiers:
      str += indent(snippet(control_structs, type_identifier) + sep)

  for snippet in [snippet_type_conditionals,
      snippet_hashes,
      snippet_lpd433_control_variable_parse,
      snippet_set_lpd433_control_variable]:
    str += indent(snippet(control_structs) + sep)

  return str + f'}} // namespace control\n'

sensors, control_structs = load_and_expand_jsons()
write_generated_cpp_file("control",
  control_cpp_include(control_structs, sensors))

