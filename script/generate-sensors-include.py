from common import (indent, write_generated_cpp_file, header_sep,
  load_and_expand_jsons, dedent)

def snippet_bool_as_csv_string():
  return dedent('''\
    std::string bool_as_csv_string(bool const x) {
      return std::string{x ? "1" : "0"};
    }
    ''')

def snippet_aggregation_step_ops():
  return dedent('''\
    auto constexpr aggregation_step_mean{
      [](auto const &x0, auto const &x1){ return x0 + x1; }};
    auto constexpr aggregation_step_min{
      [](auto const &x0, auto const &x1){ return std::min(x0, x1); }};
    auto constexpr aggregation_step_max{
      [](auto const &x0, auto const &x1){ return std::max(x0, x1); }};
    auto constexpr aggregation_step_first{
      [](auto const &x0, auto const &){ return x0; }};
    ''')

def snippet_struct(sensor_name, sensor_params):
  str = f'struct {sensor_name} '
  if sensor_name != "sensor":
    str += f': public sensor '
  str += f'{{\n'
  for field_name, field_params in sensor_params.items():
    str += indent(f'std::optional<{field_params["type"]}> {field_name}{{}};\n')
  return str + f'}};\n'

def snippet_sensor_state(sensor_name, sensor_params):
  str = f'struct {sensor_name}_state '
  if sensor_name != "sensor":
    str += f': public sensor_state '
  str += f'{{\n'
  for field_name, field_params in sensor_params.items():
    if field_params["aggregate"] in ["mean"]:
      str += indent(f'unsigned {field_name}_count{{0u}};\n')
  return str + f'}};\n'

def snippet_init_state(sensor_name, sensor_params):
  return dedent(f'''\
    auto init_state({sensor_name} const &) {{
      return {sensor_name}_state{{}};
    }}
    ''')

def snippet_setup_io(sensor_name, sensor_params):
  return dedent(f'''\
    auto setup_{sensor_name}_io(auto const &pi, auto const &);

    auto setup_io({sensor_name} const &, auto const &pi, auto const &args) {{
      return setup_{sensor_name}_io(pi, args);
    }}
    ''')

def snippet_sample(sensor_name, sensor_params):
  return dedent(f'''\
    {sensor_name} sample_{sensor_name}(auto const &, auto const &);

    {sensor_name} sample('''
    f'''{sensor_name} const &, auto const &clock, auto const &sensor_io) {{
      return sample_{sensor_name}(clock, sensor_io);
    }}
    ''')

def snippet_aggregation_step(sensor_name, sensor_params):
  str = f'auto aggregation_step(\n'
  str += indent(f'{sensor_name} const aggregate,\n', 2)
  str += indent(f'{sensor_name}_state const state,\n', 2)
  str += indent(f'{sensor_name} const sample) {{\n', 2)
  if sensor_name != "sensor":
    str += indent(dedent(f'''\
      auto const [base_aggregate, base_state]{{
        aggregation_step(static_cast<sensor>(aggregate),
                         static_cast<sensor_state>(state),
                         static_cast<sensor>(sample))}};\n'''))
  for field_name, field_params in sensor_params.items():
    str += indent(f'auto const {field_name}{{util::optional_apply('
      f'aggregation_step_{field_params["aggregate"]},\n')
    str += indent(f'aggregate.{field_name}, sample.{field_name})}};\n', 2)
    if field_params["aggregate"] in ["mean"]:
      str += indent(
        f'auto const {field_name}_count{{state.{field_name}_count + \n')
      str += indent(f'(sample.{field_name}.has_value() ? 1u : 0u)}};\n', 2)

  str += indent(
    f'\nreturn std::pair<{sensor_name}, {sensor_name}_state>{{{{\n')
  if sensor_name != "sensor":
    str += indent(f'base_aggregate,\n', 3)
  for field_name, field_params in sensor_params.items():
    str += indent(f'{field_name},\n', 3)
  str += indent(f'}}, {{\n', 2)
  if sensor_name != "sensor":
    str += indent(f'base_state,\n', 3)
  for field_name, field_params in sensor_params.items():
    if field_params["aggregate"] in ["mean"]:
      str += indent(f'{field_name}_count,\n', 3)
  str += indent(f'}}\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_aggregation_finish(sensor_name, sensor_params):
  str = f'auto aggregation_finish(\n'
  str += indent(f'{sensor_name} const aggregate,\n', 2)
  str += indent(f'{sensor_name}_state const state) {{\n', 2)
  if sensor_name != "sensor":
    str += indent(dedent(f'''\
      auto const base_aggregate{{
        aggregation_finish(static_cast<sensor>(aggregate),
                           static_cast<sensor_state>(state))}};\n'''))
  for field_name, field_params in sensor_params.items():
    if field_params["aggregate"] in ["mean"]:
      str += indent(f'auto const '
        f'{field_name}{{util::optional_apply([=](auto const &x){{\n', 2)
      str += indent(f'return x / static_cast<{field_params["type"]}>('
        f'state.{field_name}_count); }},\n', 3)
      str += indent(f'aggregate.{field_name})}};\n', 3)
    else:
      str += indent(f'auto const {field_name}{{aggregate.{field_name}}};\n', 2)

  str += indent(
    f'\nreturn {sensor_name}{{\n')
  if sensor_name != "sensor":
    str += indent(f'base_aggregate,\n', 2)
  for field_name, field_params in sensor_params.items():
    str += indent(f'{field_name},\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_name(sensor_name, sensor_params):
  str = f'std::string name({sensor_name} const &) {{\n'
  str += indent(f'return std::string{{"{sensor_name}"}};\n')
  return str + f'}}\n'

def snippet_field_names(sensor_name, sensor_params):
  str = (f'std::array<std::string, {len(sensor_params)}> field_names('
    f'{sensor_name} const &) {{\n')
  str += indent(f'return {{{{')
  for field_name, field_params in sensor_params.items():
    str += indent(f'\n{{"{field_name}"}},', 2)
  str += indent(f'\n}}}};\n')
  return str + f'}}\n'

def snippet_write_fields(sensor_name, sensor_params):
  str = (f'std::ostream &write_fields('
    f'std::ostream &out, {sensor_name} const &data,\n')
  str += indent(f'WriteFormat const wf = WriteFormat::csv,'
    f'std::optional<std::string> const sensor_name_arg = {{}},\n'
    f'bool const inner = false) {{\n', 2)
  str += indent(f'auto const original_precision{{out.precision()}};\n')
  str += indent(f'auto const original_width{{out.width()}};\n')
  str += indent(f'auto const original_flags{{out.flags()}};\n')
  str += indent(f'auto const original_fill{{out.fill()}};\n')
  str += indent(f'\n')

  str += indent(
    f'auto const sensor_name{{sensor_name_arg.value_or(name(data))}};\n')
  str += indent(f'\n')

  if bool(sensor_params):
    str += indent(f'if (wf == WriteFormat::toml) if (not inner) {{\n')
    str += indent(
      f'// NOTE: This requires `sensor_name` to be a proper TOML key.\n'
      f'if (sensor_name != "")\n', 2)
    str += indent(f'out << "[[" << sensor_name << ".data]]\\n";\n', 3)
    str += indent(f'else out << "[[data]]\\n";\n', 2)
    str += indent(f'}}\n')
    str += indent(f'\n')

  if sensor_name != "sensor":
    str += indent(f'write_fields(out, static_cast<sensor>(data), wf, '
      f'{{sensor_name}}, {"true" if bool(sensor_params) else "false"});\n')

  if bool(sensor_params):
    str += indent(f'auto const names{{field_names(data)}};\n')
  str += indent(f'out << std::setfill(\' \');\n')
  i = 0
  for field_name, field_params in sensor_params.items():
    if i > 0:
      str += indent(f'if (wf == WriteFormat::csv)\n')
      str += indent(f'out << std::setw(0) << cc::csv_delimiter_string;\n', 2)
    if "decimals" in field_params:
      str += indent(f'out << std::setprecision({field_params["decimals"]}) '
        f'<< std::fixed;\n')
    else:
      str += indent(f'out << std::setprecision(cc::field_decimals_default) '
        f'<< std::defaultfloat;\n')
    if "width" in field_params:
      width = field_params["width"]
      if "decimals" in field_params:
        width = f'1 + {field_params["decimals"]} + {width}'
      str += indent(f'out << std::setw({width});\n')
    str += indent(f'if (wf == WriteFormat::csv)\n')
    str += indent(f'out << '
      f'io::csv::CSVWrapper<std::optional<{field_params["type"]}>>{{\n', 2)
    str += indent(f'data.{field_name}}};\n', 3)
    str += indent(f'else if  (wf == WriteFormat::toml)\n')
    str += indent(f'//NOTE: This requires the sensor field names to be proper'
      f'TOML keys.\n', 2)
    str += indent(f'out << io::toml::TOMLWrapper{{\n', 2)
    str += indent(f'std::make_pair(names[{i}], data.{field_name})}};\n', 3)
    i += 1
  str += indent(f'if (inner) {{\n')
  str += indent(f'if (wf == WriteFormat::csv)\n', 2)
  str += indent(f'out << std::setw(0) << cc::csv_delimiter_string;\n', 3)
  str += indent(f'return out;\n', 2)
  str += indent(f'}} else {{\n')
  str += indent(f'out.precision(original_precision);\n', 2)
  str += indent(f'out.width(original_width);\n', 2)
  str += indent(f'out.flags(original_flags);\n', 2)
  str += indent(f'out.fill(original_fill);\n', 2)
  str += indent(f'return out << std::endl;\n' if bool(sensor_params) else
    f'return out;\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_write_field_names():
  def snippet(t, base_call):
    return dedent(f'''\
      std::ostream &write_field_names(std::ostream &out, {t} const &data,
          WriteFormat const wf = WriteFormat::csv,
          std::optional<std::string> const sensor_name_arg = {{}},
          bool const inner = false) {{
        auto const original_precision{{out.precision()}};
        auto const original_width{{out.width()}};
        auto const original_flags{{out.flags()}};
        auto const original_fill{{out.fill()}};
        out << std::setw(0);

        auto const field_names_buffer{{field_names(data)}};

        std::string sensor_name{{sensor_name_arg.value_or(name(data))}};
        if (wf == WriteFormat::toml)
          if (not inner and field_names_buffer.size() > 0) {{
            // NOTE: This requires `sensor_name` to be a proper TOML key.
            if (sensor_name != "") out << "[" << sensor_name << "]\\n";
            out << "field_names = [\\n";
          }}
        ''' f'''{base_call}
        // NOTE: This code could make use of more abstraction and less
        // reinventing the wheel, but I don't care, for now…
        if (wf == WriteFormat::csv) {{
          if (sensor_name != "") sensor_name += "_";
          for (auto const &field_name : field_names_buffer) {{
            out << "\\"" << sensor_name << field_name << "\\"";
            if (inner or &field_name != &field_names_buffer.back())
              out << cc::csv_delimiter_string;
          }}
        }} else if (wf == WriteFormat::toml) {{
          for (auto const &field_name : field_names_buffer) {{
            io::toml::indent(out) << "\\"" << field_name << "\\"";
            if (inner or &field_name != &field_names_buffer.back())
            out << ",\\n"; else out << "]\\n";
          }}
        }}
        if (inner) return out; else {{
          out.precision(original_precision);
          out.width(original_width);
          out.flags(original_flags);
          out.fill(original_fill);
          if (field_names_buffer.size() > 0) out << "\\n";
          return out << std::flush;
        }}
      }}''')

  t = f'sensor'
  base_call = f''
  str = snippet(t, base_call) + f'\n\n'

  t = f'auto'
  base_call = indent(dedent(f'''
    write_field_names(out, static_cast<sensor>(data), wf,
      std::optional<std::string>(sensor_name), field_names_buffer.size() > 0);
    '''), 4)
  str += snippet(t, base_call)
  return str

def sensors_include(sensors):
  str, sep = header_sep(__file__)

  str += f'namespace sensors {{\n' + sep
  str += indent(snippet_bool_as_csv_string() + sep)
  str += indent(snippet_aggregation_step_ops() + sep)

  base_sensor_params = {"timestamp": {
    "type": "cc::timestamp_duration_t",
    "aggregate": "mean",
    "width": "cc::timestamp_width",
    "decimals": "cc::timestamp_decimals"}}

  for snippet in [snippet_struct, snippet_sensor_state, snippet_init_state,
      snippet_setup_io, snippet_sample, snippet_aggregation_step,
      snippet_aggregation_finish, snippet_name, snippet_field_names,
      snippet_write_fields]:
    str += indent(snippet("sensor", base_sensor_params) + sep)
    for sensor_name, sensor_params in sensors.items():
      str += indent(snippet(sensor_name, sensor_params) + sep)

  str += indent(snippet_write_field_names() + sep)

  return str + f'}} // namespace sensors\n'

sensors, _ = load_and_expand_jsons()
write_generated_cpp_file("sensors", sensors_include(sensors))

