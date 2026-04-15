# Makefile for ALU C++ Testbench

# Compiler and flags
CXX = /mingw64/bin/g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I/mingw64/include
LIBS = -L/mingw64/lib -lgtest -lgtest_main -pthread

# Target executable
TARGET = alu_test.exe

# Source files
SRCS = alu.cpp alu_test.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	PATH=/mingw64/bin:$$PATH $(CXX) $(CXXFLAGS) $(OBJS) $(LIBS) -o $(TARGET)

# Compile source files
%.o: %.cpp
	PATH=/mingw64/bin:$$PATH $(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

# Run the test
run: $(TARGET)
	PATH=/mingw64/bin:$$PATH ./$(TARGET)

# Rebuild everything
rebuild: clean all

.PHONY: all clean run rebuild