# RV32I 5-Stage Pipelined Processor

A fully verified RV32I RISC-V processor implemented in SystemVerilog with a complete UVM-style verification environment in C++ (Verilator) and SystemVerilog.

---

## Supported Instructions

| Class   | Instructions |
|---------|-------------|
| R-type  | ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA |
| I-type  | ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI |
| Load    | LW |
| Store   | SW |
| Branch  | BEQ, BNE |
| Jump    | JAL |

---

## Pipeline Architecture

```
IF  ->  ID  ->  EX  ->  MEM  ->  WB
```

| Feature | Implementation |
|---------|---------------|
| Forwarding | EX->EX (MEM stage) and MEM->EX (WB stage) |
| Load-use stall | 1-cycle bubble, PC + IF/ID frozen |
| Branch resolution | EX stage, 2-cycle flush on taken |
| JAL | EX stage, 2-cycle flush, link address forwarded |

---

## Repository Structure

```
rtl/                            RTL source (synthesisable)
  rv32i_pipe5_with_branches.sv  Top-level 5-stage pipeline
  rv32i_decoder.sv              Instruction decoder
  rv32i_alu.sv                  32-bit ALU
  rv32i_regfile.sv              32x32 register file
  rv32i_forwarding_unit.sv      RAW forwarding unit
  rv32i_hazard_detection.sv     Load-use hazard + stall control
  rv32i_branch_control.sv       Branch condition + flush + PC mux

cpp_tb/pipeline/                C++ UVM-style pipeline testbench (primary)
  tb_core.cpp                   GoogleTest entry point (14 tests)
  pipe_test.h                   Test runners
  pipe_env.h                    Top-level environment
  pipe_agent.h                  Agent (driver + monitor)
  pipe_driver.h                 Program loader, clock controller, assertions
  pipe_monitor.h                Per-cycle DUT observer
  pipe_scoreboard.h             Windowed-lookahead commit scoreboard
  pipe_coverage.h               Functional coverage (24 bins)
  pipe_sequence.h               Directed and constrained-random sequences
  pipe_transaction.h            Transaction types (ExecStep, CommitRecord)
  reference_model.h             C++ ISS reference model
  program_gen.h                 Constrained-random program generator
  Makefile

cpp_tb/alu/                     C++ UVM-style ALU testbench
cpp_tb/decoder/                 C++ UVM-style decoder testbench

sv_tb/alu/                      SystemVerilog UVM ALU testbench
sv_tb/decoder/                  SystemVerilog UVM decoder testbench

archive/                        Intermediate development files (not active)
```

---

## Prerequisites

| Tool | Version | Install (MSYS2 MinGW64) |
|------|---------|------------------------|
| Verilator | 5.x | `pacman -S mingw-w64-x86_64-verilator` |
| GCC | 12+ | `pacman -S mingw-w64-x86_64-gcc` |
| GoogleTest | any | `pacman -S mingw-w64-x86_64-gtest` |

---

## Build and Run

```bash
cd cpp_tb/pipeline

# Build
make

# Run all 14 tests
./pipeline_tb.exe

# Run a single test
./pipeline_tb.exe --gtest_filter=PipeUVMFixture.DataForwardingTest

# Run the 100-seed stress test (writes pipeline_stress.csv)
./pipeline_tb.exe --gtest_filter=PipeUVMFixture.LongStressTest

# Clean build artifacts (keep obj_dir)
make clean

# Full clean including Verilator model
make distclean && make
```

---

## Test Suite

### Directed tests (7)

| Test | What it verifies |
|------|-----------------|
| `ArithBasicTest` | All R-type and I-type ALU ops, no hazards |
| `DataForwardingTest` | Back-to-back RAW hazards, EX->EX and MEM->EX forwarding |
| `LoadUseTest` | LW followed immediately by consumer, 1-cycle stall |
| `BranchTest` | BEQ taken, BEQ not-taken, BNE countdown loop |
| `JalTest` | JAL link address (PC+4) and jump target |
| `FullProgramTest` | All hazard types in one program |
| `FullRegressionTest` | All directed + random, must close 24/24 coverage bins |

### Adversarial tests (6)

| Test | What it verifies |
|------|-----------------|
| `DeepDepChainTest` | 8-deep same-register dependency chain |
| `BackToBackControlTest` | JAL + BEQ within 3 instructions (two flush windows) |
| `LoadUseBranchTest` | LW then BEQ on loaded register |
| `StoreLoadAdjacentTest` | SW then LW at same address, write-before-read ordering |
| `OverlappingHazardsTest` | Load-use stall + EX->EX forward + taken branch in 6 instructions |
| `ImmEdgeCasesTest` | +-2047, -1, 0 immediates; SRAI vs SRLI on negatives; SLT/SLTU edge cases |

### Stress test

`LongStressTest` - 100 constrained-random seeds x 500 instructions each.

Latest result:
```
Total errors:   0 / 100 seeds
Coverage:       24/24 bins hit (100.0%)
Closed at seed: 1
```

---

## Coverage Model (24 bins)

| Group | Bins |
|-------|------|
| Instruction class (7) | R_TYPE, I_ALU, LW, SW, BEQ, BNE, JAL |
| Branch outcome (2) | taken, not-taken |
| Hazard type (3) | RAW, load-use, control transfer |
| Instr x hazard cross (4) | R_TYPE x RAW, BEQ x RAW, BNE x RAW, R_TYPE x load-use |
| Branch x hazard cross (2) | taken x RAW, not-taken x RAW |
| Operand value (6) | rs1=0, rs1=-1, imm=0, imm=-1, imm=+2047, imm=-2048 |

---

## Runtime Assertions (pipe_driver.h)

Eight invariants checked every clock cycle during simulation:

| ID | Invariant |
|----|-----------|
| A1 | `jump_taken` and `branch_taken` are mutually exclusive |
| A2 | `jump_taken` only when EX instruction opcode is JAL (0x6F) |
| A3 | `branch_taken` only when EX instruction opcode is BEQ/BNE (0x63) |
| A4 | PC after JAL equals `id_ex_pc + id_ex_imm` |
| A5 | PC after taken branch equals `id_ex_pc + id_ex_imm` |
| A6 | PC=0 only on reset or a control transfer that arithmetically targets 0 |
| A7 | Load-use bubble has all ID/EX control signals cleared |
| A8 | Branch/jump flush has all ID/EX control signals cleared |

Any violation prints cycle number, signal state, and aborts immediately.

---

## Known Limitations

- RV32I subset only (no M, F, C extensions)
- Single-issue in-order pipeline
- Word-addressable data memory with modulo-256 wrapping (`alu_result[9:2]`)
- No exception or interrupt handling
- No cache model (single-cycle instruction and data memory)
