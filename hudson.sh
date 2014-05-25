#!/bin/bash

rm ./build -rf
mkdir build
cd ./build
cmake .. && make && ./bin/unit_test
lcov -d ../ -c -o pio.cov && genhtml -o cov pio.cov
