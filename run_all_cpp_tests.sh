#!/bin/bash

# Script to run all C++ tests

echo "Building and running ALU test..."
cd cpp_tb/alu
make clean
make
make run

echo ""
echo "Building and running Decoder test..."
cd ../decoder
make clean
make
make run

echo ""
echo "All tests completed!"