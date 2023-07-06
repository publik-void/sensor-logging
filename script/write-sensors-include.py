import os
import json
import textwrap

def indent(str, n = 1):
  return textwrap.indent(str, "  " * n)

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
    std::string_view bool_as_csv_string(bool const x) {
      return std::string_view{x ? "1" : "0"};
    }
    ''')

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
      [](auto const &x0, auto const &x1){return x0 + x1;}};
    auto constexpr aggregation_step_min{
      [](auto const &x0, auto const &x1){return std::min(x0, x1);}};
    auto constexpr aggregation_step_max{
      [](auto const &x0, auto const &x1){return std::max(x0, x1);}};
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
#         std::string_view const &field, std::optional<int> const) {
#       return out << field;
#     }
#
#     std::ostream &write_field(std::ostream &out,
#         bool const &field, std::optional<int> const) {
#       return out << (field ? "1" : "0");
#     }
#     ''')

def snippet_struct(sensor_name, sensor_params):
  str = f'struct {sensor_name} : public sensor {{\n'
  for field_name, field_params in sensor_params.items():
    str += indent(f'std::optional<{field_params["type"]}> {field_name}{{}};\n')
  return str + f'}};\n'

def snippet_aggregation_state(sensor_name, sensor_params):
  str = f'struct {sensor_name}_state : public aggregation_state {{\n'
  for field_name, field_params in sensor_params.items():
    if field_params["aggregate"] in ["mean"]:
      str += indent(f'unsigned {field_name}_count{{0u}};\n')
  return str + f'}};\n'

def snippet_aggregation_step(sensor_name, sensor_params):
  str = f'auto aggregation_step(\n'
  str += indent(f'{sensor_name} const aggregate,\n', 2)
  str += indent(f'{sensor_name}_state const state,\n', 2)
  str += indent(f'{sensor_name} const sample) {{\n', 2)
  for field_name, field_params in sensor_params.items():
    str += indent(f'auto const {field_name}{{optional_apply('
      f'aggregation_step_{field_params["aggregate"]},\n')
    str += indent(f'aggregate.{field_name}, sample.{field_name})}};\n', 2)
    if field_params["aggregate"] in ["mean"]:
      str += indent(
        f'auto const {field_name}_count{{state.{field_name}_count + \n')
      str += indent(f'(sample.{field_name}.has_value() ? 1u : 0u)}};\n', 2)

  str += indent(
    f'\nreturn std::pair<{sensor_name}, {sensor_name}_state>{{{{\n')
  str += indent(f'{{}},\n', 3)
  for field_name, field_params in sensor_params.items():
    str += indent(f'{field_name},\n', 3)
  str += indent(f'}}, {{\n', 2)
  str += indent(f'{{}},\n', 3)
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
  for field_name, field_params in sensor_params.items():
    if field_params["aggregate"] in ["mean"]:
      str += indent(
        f'auto const {field_name}{{optional_apply([=](auto const &x){{\n', 2)
      str += indent(f'return x / static_cast<{field_params["type"]}>('
        f'state.{field_name}_count); }},\n', 3)
      str += indent(f'aggregate.{field_name})}};\n', 3)
    else:
      str += indent(f'auto const {field_name}{{aggregate.{field_name}}};\n', 2)

  str += indent(
    f'\nreturn {sensor_name}{{\n')
  str += indent(f'{{}},\n', 2)
  for field_name, field_params in sensor_params.items():
    str += indent(f'{field_name},\n', 2)
  str += indent(f'}};\n')
  return str + f'}}\n'

def snippet_name(sensor_name, sensor_params):
  str = f'std::string_view name({sensor_name} const) {{\n'
  str += indent(f'return std::string_view{{"{sensor_name}"}};\n')
  return str + f'}}\n'

def snippet_field_names(sensor_name, sensor_params):
  str = f'std::vector<std::string_view> field_names({sensor_name} const) {{\n'
  str += indent(f'return std::vector{{')
  for field_name, field_params in sensor_params.items():
    str += indent(f'\nstd::string_view{{"{field_name}"}},', 2)
  str += indent(f'\n}};\n')
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
  str += indent(f'std::ostream &out, {sensor_name} const data) {{\n', 2)
  str += indent(f'auto const original_precision{{out.precision()}};\n')
  str += indent(f'auto const original_width{{out.width()}};\n')
  str += indent(f'auto const original_flags{{out.flags()}};\n')
  str += indent(f'out')
  is_first_entry = True
  for field_name, field_params in sensor_params.items():
    if is_first_entry:
      is_first_entry = False
    else:
      str += indent(f'\n<< cc::csv_delimiter_string', 2)
    if "decimals" in field_params:
      str += indent(f'\n<< std::setprecision({field_params["decimals"]}) '
        f'<< std::fixed', 2)
    else:
      str += indent(f'\n<< std::setprecision(cc::field_decimals_default) '
        f'<< std::defaultfloat', 2)
    if "width" in field_params:
      width = field_params["width"]
      if "decimals" in field_params:
        width += 1 + field_params["decimals"]
      str += indent(f'\n<< std::setw({width})', 2)
    str += indent(f'\n<< CSVWrapper{{data.{field_name}}}', 2)
  str += f';\n'
  str += indent(f'out.precision(original_precision);\n')
  str += indent(f'out.width(original_width);\n')
  str += indent(f'out.flags(original_flags);\n')
  str += indent(f'return out << std::endl;\n')
  return str + f'}}\n'

def snippet_write_csv_field_names():
  return textwrap.dedent('''\
    template<class T>
    std::ostream &write_csv_field_names(std::ostream &out, T const &data) {
      auto const &sensor_name{name(data)};
      bool is_first_entry = true;
      for (auto const &field_name : field_names(data)) {
        if (is_first_entry) is_first_entry = false;
        else out << cc::csv_delimiter_string;
        out << "\\"" << sensor_name << "_" << field_name << "\\"";
      }
      return out << std::endl;
    }
    ''')

def sensors_include(sensors):
  str = f''
  sep = f'\n'

  rel_file_path = os.path.relpath(__file__,
    os.path.join(os.path.dirname(__file__), "..", "src"))
  str += f'// File generated by `{rel_file_path}`\n' + sep

  includes = ["vector", "string_view", "utility", "optional", "tuple",
    "ostream", "iomanip", "ios", "algorithm"]

  for include in includes:
    str += f'#include <{include}>\n'
    pass
  str += sep

  str += f'namespace cc {{\n' + sep
  str += indent(f'int constexpr field_decimals_default{{6u}};\n' + sep)
  # str += indent(f'std::string_view constexpr csv_delimiter_string{{", "}};\n')
  # str += indent(f'std::string_view constexpr csv_false_string{{"0"}};\n')
  # str += indent(f'std::string_view constexpr csv_true_string{{"1"}};\n' + sep)
  str += f'}} // namespace cc\n' + sep

  str += f'namespace sensors {{\n' + sep
  str += indent(f'struct sensor {{}};\n' + sep)
  str += indent(f'struct aggregation_state {{}};\n' + sep)
  str += indent(snippet_bool_as_csv_string() + sep)
  # str += indent(snippet_optional_apply() + sep)
  str += indent(snippet_aggregation_step_ops() + sep)
  # str += indent(snippet_write_field() + sep)

  for snippet in [snippet_struct, snippet_aggregation_state,
      snippet_aggregation_step, snippet_aggregation_finish, snippet_name,
      snippet_field_names, snippet_write_csv_fields]:
    for sensor_name, sensor_params in sensors.items():
      str += indent(snippet(sensor_name, sensor_params) + sep)

  str += indent(snippet_write_csv_field_names() + sep)

  return str + f'}} // namespace sensors\n'

sensors_json_filename = os.path.join(os.path.dirname(__file__), "sensors.json")
sensors_cpp_filename = os.path.join(os.path.dirname(__file__), "..", "src",
  "sensors.cpp")

with open(sensors_json_filename, "r") as f:
  sensors = json.load(f)

# sensors = expand(sensors)

sensors_include_str = sensors_include(sensors)

with open(sensors_cpp_filename, "w") as f:
  f.write(sensors_include_str)

# print(sensors_include_str, end = "")
