#!/bin/sh
set -eu

clang -std=c11 -Wall -Wextra -pedantic \
  trideroah.c \
  -o trideroah \
  -framework AppKit \
  -lobjc
