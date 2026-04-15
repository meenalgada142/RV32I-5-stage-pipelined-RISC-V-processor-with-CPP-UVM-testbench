# Build instructions for ALU C++ Testbench

## Option 1: Using Makefile (Recommended)

```bash
# Build the test
make

# Run the test
make run

# Clean build files
make clean

# Rebuild from scratch
make rebuild
```

## Option 2: Manual Compilation

Assuming Google Test is installed via vcpkg or similar
On Windows with MSVC:
1. Install Google Test: vcpkg install gtest
2. Set VCPKG_ROOT environment variable
3. Run: cl /EHsc /I%VCPKG_ROOT%\installed\x64-windows\include alu.cpp alu_test.cpp /link /LIBPATH:%VCPKG_ROOT%\installed\x64-windows\lib gtest.lib gtest_main.lib

For ALU (MSYS2):
```bash
export PATH=/mingw64/bin:$PATH
g++ -std=c++17 -I/mingw64/include alu.cpp alu_test.cpp -L/mingw64/lib -lgtest -lgtest_main -pthread -o alu_test.exe
./alu_test.exe
```

## Option 3: Verilator with RTL (Advanced)
On MSYS2 with Verilator installed:
1. Navigate to directory: cd /c/Users/gadap/OneDrive/Documents/RISV/cpp_tb/alu
2. Verilate and build:
verilator -Wall --cc --trace --exe --build -j 0 \
  -LDFLAGS "-lgtest -lgtest_main" \
  -I../../../rtl \
  ../../../rtl/rv32i_alu.sv \
  alu_test_verilator.cpp \
  -o alu_test_verilator

3. Run the test:
./obj_dir/alu_test_verilator

Notes:
- Verilator generates C++ from SystemVerilog RTL
- Tests run against actual hardware description
- Much slower than pure C++ but verifies real RTL

# For Decoder:
# g++ -std=c++11 -I/path/to/gtest/include decoder.cpp decoder_test.cpp -L/path/to/gtest/lib -lgtest -lgtest_main -pthread -o decoder_test
# ./decoder_test