# Build instructions for Decoder C++ Testbench

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

Same as ALU, but for decoder files.

Compile: g++ -std=c++17 -I/C/msys64/mingw64/include decoder.cpp decoder_test.cpp -L/C/msys64/mingw64/lib -lgtest -lgtest_main -pthread -o decoder_test.exe
Run: ./decoder_test.exe

## Option 3: Verilator with RTL (Advanced)
On MSYS2 with Verilator installed:
1. Navigate to directory: cd /c/Users/gadap/OneDrive/Documents/RISV/cpp_tb/decoder
2. Verilate and build:
verilator -Wall --cc --trace --exe --build -j 0 \
  -LDFLAGS "-lgtest -lgtest_main" \
  -I../../../rtl \
  ../../../rtl/rv32i_decoder.sv \
  decoder_test_verilator.cpp \
  -o decoder_test_verilator

3. Run the test:
./obj_dir/decoder_test_verilator

Notes:
- Verilator generates C++ from SystemVerilog RTL
- Tests run against actual hardware description
- Much slower than pure C++ but verifies real RTL