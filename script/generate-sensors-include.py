import os
import json
import textwrap

def indent(str, n = 1, predicate = None):
  return textwrap.indent(str, "  " * n, predicate)

# def expand(sensors):
#   "Bring JSON spec into a canonical form"
#   for sensor_name, sensor_params in sensors.items():
#     for field_name, field_params in sensor_params.items():
#       if isinstance(field_params["aggregate"], str):
#         field_params["aggregate"] = {
#           "type": field_params["aggregate"], "unless": []}
#   return sensors

def snippet_bool_as_csv_string():
  return textwrap.dedent('''\
    std::string bool_as_csv_string(bool const x) {
      return std::string{x ? "1" : "0"};
    }
    ''')

# def snippet_struct_sensor():
#   return textwrap.dedent('''\
#     struct sensor {
#       std::optional<cc::timestamp_duration_t> timestamp{};
#     };
#     ''')

# def snippet_struct_sensor_state():
#   return textwrap.dedent('''\
#     struct sensor_state {
#       unsigned timestamp_count{0u};
#     };
#     ''')

# def snippet_optional_apply():
#   return textwrap.dedent('''\
#     // NOTE: `optional_apply` could perhaps be defined as a variadic template
#     // and function, but let's keep it simple for now.
#
#     template<class F, class T>
#     std::optional<T> optional_apply(F f, std::optional<T> const &x0) {
#       if (x0.has_value()) return std::optional<T>{f(x0.value())};
#       else return std::optional<T>{};
#     }
#
#     template<class F, class T>
#     std::optional<T> optional_apply(F f,
#         std::optional<T> const &x0, std::optional<T> const &x1) {
#       if (x0.has_value()) {
#         if (x1.has_value()) return std::optional<T>{f(x0.value(), x1.value())};
#         else return std::optional<T>{x0};
#       } else {
#         if (x1.has_value()) return std::optional<T>{x1};
#         else return std::optional<T>{};
#       }
#     }
#     ''')

def snippet_aggregation_step_ops():
  return textwrap.dedent('''\
    auto constexpr aggregation_step_mean{
      [](auto const &x0, auto const &x1){ return x0 + x1; }};
    auto constexpr aggregation_step_min{
      [](auto const &x0, auto const &x1){ return std::min(x0, x1); }};
    auto constexpr aggregation_step_max{
      [](auto const &x0, auto const &x1){ return std::max(x0, x1); }};
    auto constexpr aggregation_step_first{
      [](auto const &x0, auto const &){ return x0; }};
    ''')

# def snippet_write_field():
#   return textwrap.dedent('''\
#     template<class T>
#     std::ostream &write_field(std::ostream &out,
#         T const &field, std::optional<int> const decimals) {
#       if (decimals.has_value()) {
#         out << std::setprecision(decimals.value()) << std::fixed;
#       } else {
#         out << std::setprecision(cc::field_decimals_default)
#           << std::defaultfloat;
#       }
#       return out << field;
#     }
#
#     template<class T>
#     std::ostream &write_field(std::ostream &out,
#         std::optional<T> const &field, std::optional<int> const decimals) {
#       return field.has_value() ? write_field(out, field.value(), decimals)
#                                : out;
#     }
#
#     std::ostream &write_field(std::ostream &out,
#         std::string const &field, std::optional<int> const) {
#       return out << field;
#     }
#
#     std::ostream &write_field(std::ostream &out,
#         bool const &field, std::optional<int> const) {
#       return out << (field ? "1" : "0");
#     }
#     ''')

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
  return textwrap.dedent(f'''\
    auto init_state({sensor_name} const &) {{
      return {sensor_name}_state{{}};
    }}
    ''')

def snippet_setup_io(sensor_name, sensor_params):
  return textwrap.dedent(f'''\
    auto setup_{sensor_name}_io(auto const &pi, auto const &);

    auto setup_io({sensor_name} const &, auto const &pi, auto const &args) {{
      return setup_{sensor_name}_io(pi, args);
    }}
    ''')

def snippet_sample(sensor_name, sensor_params):
  return textwrap.dedent(f'''\
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
    str += indent(textwrap.dedent(f'''\
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
    str += indent(textwrap.dedent(f'''\
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
  str = (f'auto field_names({sensor_name} const &) '
    f'{{\n')
  str += indent(f'return std::array<std::string, {len(sensor_params)}>{{{{')
  for field_name, field_params in sensor_params.items():
    str += indent(f'\nstd::string{{"{field_name}"}},', 2)
  str += indent(f'\n}}}};\n')
  return str + f'}}\n'

# def snippet_as_tuple(sensor_name, sensor_params):
#   str = f'auto as_tuple({sensor_name} const data) {{\n'
#   str += indent(f'return std::tuple{{\n')
#   for field_name, field_params in sensor_params.items():
#     str += indent(f'data.{field_name},\n', 2)
#   str += indent(f'}};\n')
#   return str + f'}}\n'

# def snippet_field_decimals(sensor_name, sensor_params):
#   str = f'auto field_decimals({sensor_name} const) {{\n'
#   str += indent(f'return std::tuple{{\n')
#   for field_name, field_params in sensor_params.items():
#     if "decimals" in field_params:
#       str += indent(
#         f'std::optional<int>{{{field_params["decimals"]}}},\n', 2)
#     else:
#       str += indent(f'std::optional<int>{{}},\n', 2)
#   str += indent(f'}};\n')
#   return str + f'}}\n'

def snippet_write_csv_fields(sensor_name, sensor_params):
  str = f'std::ostream &write_csv_fields(\n'
  str += indent(f'std::ostream &out, {sensor_name} const &data, '
      f'bool const inner = false) {{\n', 2)
  str += indent(f'auto const original_precision{{out.precision()}};\n')
  str += indent(f'auto const original_width{{out.width()}};\n')
  str += indent(f'auto const original_flags{{out.flags()}};\n')
  str += indent(f'auto const original_fill{{out.fill()}};\n')
  if sensor_name != "sensor":
    str += indent(f'write_csv_fields(out, static_cast<sensor>(data), true);\n')
  str += indent(f'out << std::setfill(\' \')')
  is_first_entry = True
  for field_name, field_params in sensor_params.items():
    if is_first_entry:
      is_first_entry = False
    else:
      str += indent(f'\n<< std::setw(0) << cc::csv_delimiter_string', 2)
    if "decimals" in field_params:
      str += indent(f'\n<< std::setprecision({field_params["decimals"]}) '
        f'<< std::fixed', 2)
    else:
      str += indent(f'\n<< std::setprecision(cc::field_decimals_default) '
        f'<< std::defaultfloat', 2)
    if "width" in field_params:
      width = field_params["width"]
      if "decimals" in field_params:
        width = f'1 + {field_params["decimals"]} + {width}'
      str += indent(f'\n<< std::setw({width})', 2)
    str += indent(f'\n<< io::csv::CSVWrapper<std::optional<{field_params["type"]}>>'
      f'{{data.{field_name}}}', 2)
  str += f';\n'
  str += indent(f'if (inner) return out << std::setw(0) '
    f'<< cc::csv_delimiter_string; else {{;\n')
  str += indent(f'out.precision(original_precision);\n', 2)
  str += indent(f'out.width(original_width);\n', 2)
  str += indent(f'out.flags(original_flags);\n', 2)
  str += indent(f'out.fill(original_fill);\n', 2)
  str += indent(f'return out << std::endl;\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_write_csv_field_names():
  def snippet(template, t, base_call):
    return textwrap.dedent(f'''{template}''' f'''\
      std::ostream &write_csv_field_names(std::ostream &out, {t} const &data,
          std::optional<std::string> const sensor_name_arg = {{}},
          bool const inner = false) {{
        auto const original_precision{{out.precision()}};
        auto const original_width{{out.width()}};
        auto const original_flags{{out.flags()}};
        auto const original_fill{{out.fill()}};
        out << std::setw(0);
        std::string sensor_name{{sensor_name_arg.value_or(name(data))}};
        if (sensor_name != "") sensor_name += "_";
        ''' f'''{base_call}
        auto const field_names_buffer{{field_names(data)}};
        for (auto const &field_name : field_names_buffer) {{
          out << "\\"" << sensor_name << field_name << "\\"";
          if (inner or &field_name != &field_names_buffer.back())
            out << cc::csv_delimiter_string;
        }}
        if (inner) return out; else {{
          out.precision(original_precision);
          out.width(original_width);
          out.flags(original_flags);
          out.fill(original_fill);
          return out << std::endl;
        }}
      }}''')

  template = f''
  t = f'sensor'
  base_call = f''
  str = snippet(template, t, base_call) + f'\n\n'

  template = indent(f'template<class T>\n', 3)
  t = f'T'
  base_call = indent(textwrap.dedent(f'''
    write_csv_field_names(out, static_cast<sensor>(data),
      std::optional<std::string>(sensor_name), true);
    '''), 4)
  str += snippet(template, t, base_call)
  return str

def sensors_include(sensors):
  str = f''
  sep = f'\n'

  rel_file_path = os.path.relpath(__file__,
    os.path.join(os.path.dirname(__file__), "..", "src"))
  str += f'// File generated by `{rel_file_path}`\n' + sep

  # includes = ["array", "string", "algorithm",
    # "utility", "optional", "tuple", "ostream", "iomanip", "ios"]

  # for include in includes:
    # str += f'#include <{include}>\n'
    # pass
  # str += sep

  str += f'namespace sensors {{\n' + sep
  str += indent(snippet_bool_as_csv_string() + sep)
  # str += indent(snippet_struct_sensor() + sep)
  # str += indent(snippet_struct_sensor_state() + sep)
  # str += indent(snippet_optional_apply() + sep)
  str += indent(snippet_aggregation_step_ops() + sep)
  # str += indent(snippet_write_field() + sep)

  base_sensor_params = {"timestamp": {
    "type": "cc::timestamp_duration_t",
    "aggregate": "mean",
    "width": "cc::timestamp_width",
    "decimals": "cc::timestamp_decimals"}}

  for snippet in [snippet_struct, snippet_sensor_state, snippet_init_state,
      snippet_setup_io, snippet_sample, snippet_aggregation_step,
      snippet_aggregation_finish, snippet_name, snippet_field_names,
      snippet_write_csv_fields]:
    str += indent(snippet("sensor", base_sensor_params) + sep)
    for sensor_name, sensor_params in sensors.items():
      str += indent(snippet(sensor_name, sensor_params) + sep)

  str += indent(snippet_write_csv_field_names() + sep)

  return str + f'}} // namespace sensors\n'

sensors_json_filename = os.path.join(os.path.dirname(__file__), "sensors.json")
sensors_cpp_filename = os.path.join(os.path.dirname(__file__), "..", "src",
  "sensors.generated.cpp")

with open(sensors_json_filename, "r") as f:
  sensors = json.load(f)

# sensors = expand(sensors)

sensors_include_str = sensors_include(sensors)

with open(sensors_cpp_filename, "w") as f:
  f.write(sensors_include_str)

# print(sensors_include_str, end = "")