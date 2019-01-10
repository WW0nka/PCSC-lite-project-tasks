#!/bin/bash
../AFL/afl-gcc -g -o tester \
  -I../PCSC/include \
  -I../PCSC/include/PCSC \
  -L../PCSC/lib \
  -lpcsclite main.c
