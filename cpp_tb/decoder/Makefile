# Makefile for Decoder C++ Testbench

# Compiler and flags
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I/C/msys64/mingw64/include
LIBS     = -L/C/msys64/mingw64/lib -lgtest -lgtest_main -pthread

# -----------------------------------------------------------------------
# Original basic test
# -----------------------------------------------------------------------
TARGET   = decoder_test.exe
SRCS     = decoder.cpp decoder_test.cpp
OBJS     = $(SRCS:.cpp=.o)

# -----------------------------------------------------------------------
# UVM-style testbench (new)
# -----------------------------------------------------------------------
UVM_TARGET = decoder_uvm_tb.exe
UVM_SRCS   = decoder.cpp decoder_top.cpp
UVM_OBJS   = $(UVM_SRCS:.cpp=.o)

# -----------------------------------------------------------------------
# Default: build both
# -----------------------------------------------------------------------
all: $(TARGET) $(UVM_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBS) -o $(TARGET)

$(UVM_TARGET): $(UVM_OBJS)
	$(CXX) $(CXXFLAGS) $(UVM_OBJS) $(LIBS) -o $(UVM_TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(UVM_OBJS) $(TARGET) $(UVM_TARGET)

run: $(TARGET)
	./$(TARGET)

run-uvm: $(UVM_TARGET)
	./$(UVM_TARGET)

rebuild: clean all

.PHONY: all clean run run-uvm rebuild