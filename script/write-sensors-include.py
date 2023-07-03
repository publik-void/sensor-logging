import os
import json
import textwrap

def indent(str, n = 1):
  return textwrap.indent(str, "  " * n)

def snippet_struct(sensor_name, sensor_params):
  str = f'struct {sensor_name} : sensor {{\n'
  for field_name, field_params in sensor_params.items():
    str += indent(f'std::optional<{field_params["type"]}> {field_name};\n')
  return str + f'}};\n'

def snippet_name(sensor_name, sensor_params):
  str = f'std::string name({sensor_name} const) {{\n'
  str += indent(f'return std::string{{"{sensor_name}"}};\n')
  return str + f'}}\n'

def snippet_field_names(sensor_name, sensor_params):
  str = f'std::string field_names({sensor_name} const x) {{\n'
  str += indent(f'return std::vector{{')
  for field_name, field_params in sensor_params.items():
    str += indent(f'\nstd::string{{"{field_name}"}},', 2)
  str += indent(f'\n}};\n')
  return str + f'}}\n'

def sensors_include(sensors):
  str = f''
  sep = f'\n'

  includes = ["vector", "string", "optional"]

  for include in includes:
    str += f'#include <{include}>\n'
    pass

  str += sep + f'namespace sensors {{\n' + sep
  str += indent(f'struct sensor {{}};\n') + sep

  for sensor_name, sensor_params in sensors.items():
    str += indent(snippet_struct(       sensor_name, sensor_params) + sep)
    str += indent(snippet_name(         sensor_name, sensor_params) + sep)
    str += indent(snippet_field_names(  sensor_name, sensor_params) + sep)
    pass

  return str + f'}} // namespace sensors'

sensors_json_filename = os.path.join(os.path.dirname(__file__), "sensors.json")
sensors_cpp_filename = os.path.join(os.path.dirname(__file__), "..", "src",
  "sensors.cpp")

with open(sensors_json_filename, "r") as f:
  sensors = json.load(f)

sensors_include_str = sensors_include(sensors)

with open(sensors_cpp_filename, "w") as f:
  f.write(sensors_include_str)

# print(sensors_include_str, end = "")
