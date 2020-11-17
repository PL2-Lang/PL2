#!/bin/sh

clang pl2.c \
  -Weverything \
  -Wno-zero-length-array \
  -Wno-format-nonliteral \
  -Wno-cast-qual \
  -Wno-padded \
  -fPIC -shared -ldl -o libpl2.so

clang main.c \
  -Weverything \
  -Wno-zero-length-array \
  -Wno-padded \
  -L. -lpl2 -o pl2

clang pldbg.c \
  -Weverything \
  -Wno-zero-length-array \
  -Wno-padded \
  -fPIC -shared -o libpldbg.so

clang echo.c \
  -Weverything \
  -Wno-zero-length-array \
  -Wno-padded \
  -fPIC -shared -o libecho.so
