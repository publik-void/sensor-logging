#!/bin/sh

#print_define() (
  #[ $# -ge 0 ] && printf "%s %s" "#define" "$1"
  #[ $# -ge 1 ] && printf " %s" "$2"
  #printf "\n"
#)

printf "constexpr auto hostname{\"%s\"};\n" "$(hostname)"
