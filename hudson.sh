#!/bin/bash

rm ./build -rf
mkdir build
cd ./build
cmake .. && make && ./bin/base_test
lcov -d ../ -c -o pio.cov && genhtml -o cov pio.cov
