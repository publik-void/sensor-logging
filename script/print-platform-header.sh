#!/bin/sh

print_define() (
  [ $# -ge 0 ] && printf "%s %s" "#define" "$1"
  [ $# -ge 1 ] && printf " %s" "$2"
  printf "\n"
)

print_define HOSTNAME "\"$(hostname)\""
