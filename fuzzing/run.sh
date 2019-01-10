#!/bin/bash
./compile.sh
export LD_LIBRARY_PATH=../PCSC/lib
nohup ../AFL/afl-fuzz -t 1500 -i inputs/ -o outputs/ ./tester @@ > afl.stdout 2> afl.stderr &
