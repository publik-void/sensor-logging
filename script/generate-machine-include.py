import socket

from common import indent, write_generated_cpp_file, header_sep

def machine_include():
  str, sep = header_sep(__file__)

  str += sep + f'namespace cc {{\n' + sep

  str += indent(f'std::string_view constexpr ' +
    f'hostname{{"{socket.gethostname()}"}};\n' + sep)

  return str + f'}} // namespace cc\n'

write_generated_cpp_file("machine", machine_include())

