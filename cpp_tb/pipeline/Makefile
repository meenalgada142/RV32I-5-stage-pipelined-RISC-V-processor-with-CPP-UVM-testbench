# Makefile — Pipeline UVM Testbench (Verilator + GoogleTest)
#
# Prerequisites:
#   /c/msys64/mingw64/bin/verilator_bin.exe  (Verilator 5.x)
#   /c/msys64/mingw64/bin/g++                (MSYS2 mingw64 g++)
#   /mingw64/lib/libgtest.a                  (GoogleTest)
#
# Build:
#   make           → build pipeline_tb.exe
#   make run       → build and run all tests
#   make clean     → remove build artifacts (keeps obj_dir/)
#   make distclean → remove everything including obj_dir/

# ============================================================================
# Paths
# ============================================================================
VERILATOR_ROOT := C:/msys64/mingw64/share/verilator
VERILATOR_BIN  := /c/msys64/mingw64/bin/verilator_bin.exe
CXX            := /c/msys64/mingw64/bin/g++
RTL_DIR        := ../../rtl
OBJ_DIR        := obj_dir
TARGET         := pipeline_tb.exe

# GoogleTest flags
GTEST_INC  := -I/mingw64/include
GTEST_LIBS := -L/mingw64/lib -lgtest -lgtest_main -lpthread

# Verilator include and generated library
VINC       := -I$(VERILATOR_ROOT)/include -I$(VERILATOR_ROOT)/include/vltstd
VLIB       := $(OBJ_DIR)/Vrv32i_pipe5_with_branches__ALL.a \
              $(OBJ_DIR)/libverilated.a

# Compile flags
CXXFLAGS   := -std=c++17 -O2 -Wall -Wno-unused-parameter \
              -I. -I$(OBJ_DIR) $(VINC) $(GTEST_INC)

# RTL source files
RTL_SV := $(RTL_DIR)/rv32i_pipe5_with_branches.sv \
           $(RTL_DIR)/rv32i_decoder.sv              \
           $(RTL_DIR)/rv32i_alu.sv                  \
           $(RTL_DIR)/rv32i_branch_control.sv       \
           $(RTL_DIR)/rv32i_forwarding_unit.sv      \
           $(RTL_DIR)/rv32i_hazard_detection.sv     \
           $(RTL_DIR)/rv32i_regfile.sv

# ============================================================================
# Default target
# ============================================================================
all: $(TARGET)

# ============================================================================
# Step 1: Verilate RTL → generate obj_dir/
# ============================================================================
$(OBJ_DIR)/Vrv32i_pipe5_with_branches.h: $(RTL_SV)
	@echo "[verilate] generating C++ model..."
	VERILATOR_ROOT=$(VERILATOR_ROOT) \
	PATH=/c/msys64/mingw64/bin:$$PATH \
	$(VERILATOR_BIN) --cc --public \
	  -I$(RTL_DIR) \
	  $(RTL_SV)    \
	  --top-module rv32i_pipe5_with_branches \
	  --Mdir $(OBJ_DIR)

# ============================================================================
# Step 2: Build Verilated library using the generated Makefile
# ============================================================================
$(OBJ_DIR)/Vrv32i_pipe5_with_branches__ALL.a: $(OBJ_DIR)/Vrv32i_pipe5_with_branches.h
	@echo "[make] building Verilated library..."
	PATH=/c/msys64/mingw64/bin:$$PATH \
	$(MAKE) -C $(OBJ_DIR) -f Vrv32i_pipe5_with_branches.mk

$(OBJ_DIR)/libverilated.a: $(OBJ_DIR)/Vrv32i_pipe5_with_branches__ALL.a
	@# built by the same sub-make above — nothing extra needed

# ============================================================================
# Step 3: Compile and link the testbench
# ============================================================================
$(TARGET): tb_core.cpp $(VLIB) \
           pipe_transaction.h reference_model.h program_gen.h \
           pipe_scoreboard.h  pipe_monitor.h    pipe_driver.h \
           pipe_coverage.h    pipe_sequence.h   pipe_agent.h  \
           pipe_env.h         pipe_test.h
	@echo "[cxx] linking $(TARGET)..."
	PATH=/c/msys64/mingw64/bin:$$PATH \
	$(CXX) $(CXXFLAGS) \
	  tb_core.cpp \
	  $(OBJ_DIR)/Vrv32i_pipe5_with_branches__ALL.a \
	  $(OBJ_DIR)/libverilated.a \
	  $(GTEST_LIBS) \
	  -o $(TARGET)
	@echo "[done] $(TARGET)"

# ============================================================================
# Utility targets
# ============================================================================
run: $(TARGET)
	PATH=/c/msys64/mingw64/bin:$$PATH ./$(TARGET)

run-%: $(TARGET)
	PATH=/c/msys64/mingw64/bin:$$PATH ./$(TARGET) --gtest_filter=PipeUVMFixture.$*

clean:
	rm -f $(TARGET) *.o *.d

distclean: clean
	rm -rf $(OBJ_DIR)

.PHONY: all run clean distclean
