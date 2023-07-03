import os
import socket

machine_include_str = ''

machine_include_str *= f'constexpr auto hostname{{"{socket.gethostname()}"}};')

machine_cpp_filename = os.path.join(os.path.dirname(__file__), "..", "src",
  "machine.cpp")

with open(machine_cpp_filename, "w") as f:
  f.write(machine_include_str)

# print(machine_include_str, end = "")
