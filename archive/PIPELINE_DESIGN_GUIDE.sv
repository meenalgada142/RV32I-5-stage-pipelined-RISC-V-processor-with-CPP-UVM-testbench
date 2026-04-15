//============================================================================
// RV32I 5-STAGE PIPELINE - DESIGN GUIDE
//============================================================================

/*
PIPELINE STRUCTURE:
===================

Stage 1: IF (Instruction Fetch)
  - Program Counter (PC) register
  - Instruction memory read
  - PC + 4 calculation
  Output: if_instr, pc_plus4

Stage 2: ID (Instruction Decode)
  - Instruction field extraction (rs1, rs2, rd, funct3, funct7, opcode)
  - Immediate generation (I, S, B, J types)
  - Register file dual-port read
  - Control signal generation
  Output: id_rs1_data, id_rs2_data, id_alu_op, control_signals

Stage 3: EX (Execute)
  - ALU input multiplexer (register vs immediate)
  - ALU computation
  - Branch target calculation
  - Branch condition evaluation
  Output: ex_alu_result, ex_zero, branch_decision

Stage 4: MEM (Memory)
  - Data memory read (for LW)
  - Data memory write (for SW)
  - ALU result passthrough
  Output: mem_dmem_rdata, ex_mem_alu_result

Stage 5: WB (Writeback)
  - Writeback multiplexer (ALU result vs memory read data)
  - Register file write (synchronous on clock edge)
  Output: register file update


PIPELINE REGISTERS:
===================

IF/ID:
  - if_id_pc_plus4     : PC + 4 for potential branch target calculation
  - if_id_instr        : Full 32-bit instruction

ID/EX:
  - id_ex_pc_plus4     : PC + 4 from previous stage
  - id_ex_rs1          : Source register 1 address (for hazard detection)
  - id_ex_rs2          : Source register 2 address (for hazard detection)
  - id_ex_rd           : Destination register address
  - id_ex_rs1_data     : Source register 1 data value
  - id_ex_rs2_data     : Source register 2 data value
  - id_ex_imm          : Immediate value (I/S/B/J type)
  - id_ex_alu_op       : ALU operation code
  - id_ex_alu_src      : ALU source select (0=reg, 1=imm)
  - id_ex_mem_read     : Enable data memory read
  - id_ex_mem_write    : Enable data memory write
  - id_ex_mem_to_reg   : Writeback source select (0=ALU, 1=memory)
  - id_ex_reg_write    : Enable register file write
  - id_ex_branch       : Branch instruction flag
  - id_ex_jump         : Jump instruction flag

EX/MEM:
  - ex_mem_pc_plus4    : PC + 4 from ID stage
  - ex_mem_alu_result  : ALU computation result
  - ex_mem_rs2_data    : Store data for SW instructions
  - ex_mem_rd          : Destination register address
  - ex_mem_mem_read    : Memory read enable
  - ex_mem_mem_write   : Memory write enable
  - ex_mem_mem_to_reg  : Writeback source select
  - ex_mem_reg_write   : Register write enable
  - ex_mem_branch      : Branch instruction flag
  - ex_mem_take_branch : Branch condition result

MEM/WB:
  - mem_wb_alu_result  : ALU result from EX stage
  - mem_wb_dmem_data   : Data memory read result
  - mem_wb_rd          : Destination register address
  - mem_wb_mem_to_reg  : Writeback source select
  - mem_wb_reg_write   : Register write enable


NAMING CONVENTIONS:
===================

<stage>_<signal>
  - if_*   : Signals in IF stage or IF/ID register
  - id_*   : Signals in ID stage or ID/EX register
  - ex_*   : Signals in EX stage or EX/MEM register
  - mem_*  : Signals in MEM stage or MEM/WB register
  - wb_*   : Signals in WB stage

<source>_<signal>
  - if_id_*   : IF/ID pipeline register
  - id_ex_*   : ID/EX pipeline register
  - ex_mem_*  : EX/MEM pipeline register
  - mem_wb_*  : MEM/WB pipeline register


DATA HAZARDS (Known Issues):
=============================

RAW (Read-After-Write) Hazards:
  Example: ADD x1, x2, x3
             SUB x4, x1, x5
  
  Problem: SUB reads x1 in ID stage before ADD writes x1 in WB stage.
  Current: Produces incorrect result (stale x1 value).
  Fix: Add forwarding logic in EX stage.

Load-Use Hazard:
  Example: LW x1, 0(x0)
             ADD x2, x1, x3
  
  Problem: ADD reads x1 in ID before LW writes x1 in WB.
  Current: Produces incorrect result.
  Fix: Add stall logic in hazard detection unit, forward from MEM stage.


CONTROL HAZARDS (Known Issues):
================================

Branch/Jump:
  Problem: Branch decision happens in EX stage, but IF stage already
           fetches the next instruction (assuming no branch).
  Current: Executes 2 extra instructions after taken branch (pipeline flush).
  Fix: Add branch prediction + flush logic.

PC Update:
  Current: PC always increments by 4 (no branch support yet).
  Fix: Add PC mux for branch target + jump target selection in IF stage.


HOW TO EXTEND:
===============

1. Add Forwarding:
   - In EX stage, add muxes before ALU inputs:
     ex_alu_in1 = (hazard_rs1_ex_mem) ? ex_mem_alu_result : id_ex_rs1_data
     ex_alu_in2 = (hazard_rs2_ex_mem) ? ex_mem_alu_result : ex_alu_in2_orig

2. Add Stall / Bubble Insertion:
   - Add pipeline control signal to flush/hold stages
   - Zero out ID/EX control bits on stall

3. Add Branch Support:
   - Capture ex_mem_take_branch in EX/MEM register
   - Use it to flush IF/ID and ID/EX if branch taken
   - Feed branch_target to PC_next mux

4. Add Full Memory System:
   - Replace instr_mem and data_mem stubs with real modules
   - Add address translation / cache if desired

5. Add Debug Output:
   - Expose pipeline signals for waveform trace
   - Add cycle counters and instruction counters


FILES:
======

rv32i_alu.sv        - Combinational ALU (8 operations)
rv32i_regfile.sv    - 32x32 register file (2-read, 1-write)
rv32i_decoder.sv    - Instruction decoder (RV32I subset)
rv32i_id_ex.sv      - Simple 2-stage datapath (for reference)
rv32i_pipe5.sv      - Full 5-stage pipeline (main implementation)
tb_pipe5.sv         - Basic testbench
rv32i_pipe5_test.sv - Advanced testbench with program loader


EXPECTED SIMULATION BEHAVIOR:
==============================

Without forwarding/hazard logic, you will see:
  - Correct results for independent instruction sequences
  - INCORRECT results for dependent instructions
  - No branch support (infinite loop on branches)

With proper program loading (rv32i_pipe5_test.sv):
  Expected Register Values (after pipeline completion):
    x1 = 5     (ADDI result)
    x2 = 3     (ADDI result)
    x3 = 8     (ADD result: 5 + 3)
    x4 = 2     (SUB result: 5 - 3)
    x5 = 1     (AND result: 5 & 3 = 0b101 & 0b011 = 0b001)
    x6 = 7     (OR result:  5 | 3 = 0b101 | 0b011 = 0b111)
    x7 = 6     (XOR result: 5 ^ 3 = 0b101 ^ 0b011 = 0b110)

Without forwarding, you will see x3, x4, x5, x6, x7 as 0 or garbage
because dependent reads happen before previous writes.

*/

//============================================================================
// END OF DESIGN GUIDE
//============================================================================
