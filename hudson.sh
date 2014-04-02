#!/bin/bash

rm ./build -rf
mkdir build
cd ./build
cmake .. && make && ./bin/core_test
lcov -d ../ -c -o pio.cov && genhtml -o cov pio.cov
