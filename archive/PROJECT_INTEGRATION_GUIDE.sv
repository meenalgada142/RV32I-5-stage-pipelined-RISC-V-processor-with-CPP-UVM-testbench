//============================================================================
// RV32I 5-STAGE PIPELINE - PROJECT INTEGRATION & NEXT STEPS
//============================================================================

/*

PROJECT STRUCTURE:
==================

Your RISV workspace now contains:

Core Modules:
  ✓ rv32i_alu.sv              - 32-bit ALU (8 operations)
  ✓ rv32i_regfile.sv          - 32x32 register file (2R1W)
  ✓ rv32i_decoder.sv          - Instruction decoder (RV32I subset)

Datapath Versions:
  ✓ rv32i_id_ex.sv            - Simple combined ID+EX datapath
  ✓ rv32i_pipe5.sv            - Full 5-stage pipeline ← LATEST

Testbenches:
  ✓ tb_cpu.sv                 - Basic testbench (older, for ID+EX module)
  ✓ tb_pipe5.sv               - Basic 5-stage testbench
  ✓ rv32i_pipe5_test.sv       - Advanced testbench with program loader ← USE THIS

Documentation:
  ✓ PIPELINE_DESIGN_GUIDE.sv  - Design reference & architecture docs


COMPILATION & SIMULATION:
=========================

Using ModelSim/VCS/Vivado:

1. Compile order (dependencies):
   $ vlog rv32i_alu.sv
   $ vlog rv32i_regfile.sv
   $ vlog rv32i_decoder.sv
   $ vlog rv32i_pipe5.sv
   $ vlog rv32i_pipe5_test.sv

2. Simulate:
   $ vsim -c rv32i_pipe5_test -do "run -all; quit"

3. With waveform dump (VCS):
   Add to testbench: 
     initial $dumpvars(0, rv32i_pipe5_test);

4. Interactive waveform (ModelSim):
   $ vsim rv32i_pipe5_test
   > run -all
   > wave zoom full


KNOWN ISSUES & LIMITATIONS:
===========================

⚠ Currently Broken (By Design):
  1. Data hazards: Dependent instructions produce wrong results
     - Example: "ADD x3, x1, x2" followed by "SUB x4, x3, x5"
     - x3 not yet written when SUB tries to read it
     - FIX: Add forwarding logic (see NEXT STEPS)

  2. Control hazards: Branches don't work
     - Branch decision in EX, but IF already fetched next instr
     - Need pipeline flush on branch taken
     - FIX: Add branch predictor + PC flush logic

  3. Load-use hazard: LW followed by dependent instruction
     - Data from memory not available until WB
     - FIX: Add stall logic for load-use case

  4. Memory not initialized at reset:
     - Both instr_mem and data_mem start as undefined (X states)
     - Must use testbench to load program
     - FIX: Add $readmemh() for boot ROM, or initialize in design

✓ Working Correctly:
  - Pipeline structure and register flow
  - Independent ALU operations (no hazards)
  - Register file reads in ID stage
  - Writeback in WB stage
  - Instruction fetch and decode


NEXT STEPS:
===========

Phase 1: Add Hazard Detection (Week 1)
  Task 1.1: Implement forwarding unit
    - Compare ex_mem_rd with id_ex_rs1, id_ex_rs2
    - Mux ALU inputs: select from ex_mem_alu_result vs registers
    - Expected: Dependent ALU ops now work
    - Test: rv32i_pipe5_test should show correct x3-x7 values

  Task 1.2: Implement stall logic
    - Detect load-use: (id_ex_mem_read && (id_ex_rd == rs1 || id_ex_rd == rs2))
    - Hold ID/IF stages, bubble in EX stage
    - Disable register write in held instruction
    - Expected: LW followed by dependent op now works

  Task 1.3: Test script
    - Run both independent and dependent tests
    - Verify register values match expectations
    - Waveform inspection for timing


Phase 2: Add Branch Support (Week 2)
  Task 2.1: Connect branch target to PC
    - Add mux in IF: pc_next = (branch_taken) ? branch_target : pc_plus4
    - Route branch decision from EX backwards to IF

  Task 2.2: Implement pipeline flush
    - On branch taken in EX stage:
      - Clear IF/ID register (set to NOP)
      - Clear ID/EX register (set to NOP)
    - Expected: Branches execute with 2-cycle penalty

  Task 2.3: Add branch prediction (optional)
    - Simple "predict not taken" or "always taken"
    - Reduces flush penalty on mispredict

  Task 2.4: Test script
    - Branch to different code sections
    - Verify branch target reached


Phase 3: Add Memory System (Week 3)
  Task 3.1: Replace data_mem stub
    - Add separate DMEM module for better organization
    - Or add byte-enable support for LB/LH instructions

  Task 3.2: Add Store/Load test cases
    - SW x1, offset(x0)
    - LW x2, offset(x0)
    - Verify memory operations work

  Task 3.3: Add Instruction ROM bootloader
    - Instead of testbench program loading
    - Allow auto-boot with program


Phase 4: Optimization & Extension (Week 4+)
  - Add more RV32I instructions (SLI, SRI, SRA, etc.)
  - Add multiply/divide if desired
  - Add CSR (control/status registers) for debug
  - Add interrupt/exception handling
  - Performance analysis (CPI, throughput)


DEBUGGING TIPS:
===============

Waveform Analysis:
  Watch these signals in order:
    1. clk, rst
    2. PC progression (if PC, if_id_instr)
    3. Pipeline registers (if_id_*, id_ex_*, ex_mem_*, mem_wb_*)
    4. ALU result (ex_alu_result)
    5. Register file (dut.id_regfile.regs[1:7])

Common Issues & Fixes:

  Issue: Register values stay at 0
    Cause: Writeback never happens
    Check: mem_wb_reg_write signal, clock, reset
    Fix: Ensure WB stage has control signals from pipeline

  Issue: Wrong ALU results
    Cause: Hazard not forwarded
    Check: ex_alu_in1, ex_alu_in2 vs id_ex_rs1_data, id_ex_rs2_data
    Fix: Add forwarding muxes

  Issue: Instructions stuck in IF
    Cause: PC not updating
    Check: pc_next, always_ff block
    Fix: Ensure reset is released, clock is running

  Issue: Memory read returns 0
    Cause: data_mem not initialized or address wrong
    Check: mem_dmem_rdata, ex_mem_alu_result
    Fix: Load data into data_mem in testbench


SIMULATION COMMANDS (Example - ModelSim):
==========================================

  # Compile all modules
  vlog rv32i_*.sv PIPELINE_DESIGN_GUIDE.sv

  # Run with verbose output
  vsim -c rv32i_pipe5_test -do "run -all; quit"

  # Run with waveforms (interactive)
  vsim rv32i_pipe5_test
  > add wave -r *
  > run -all
  > wave zoom full

  # Run specific cycle count
  vsim -c rv32i_pipe5_test -do "run 500ns; quit"


EXTENSION HOOKS:
================

To add forwarding, create a new module:

  module rv32i_hazard_fw (
      // From pipeline
      input [4:0] id_ex_rs1, id_ex_rs2,
      input [4:0] ex_mem_rd,
      input       ex_mem_reg_write,
      // Control
      output logic fw_a, fw_b  // 0=no forward, 1=forward ex_mem value
  );
  ...
  endmodule

Then in EX stage:
  ex_alu_in1 = (fw_a) ? ex_mem_alu_result : id_ex_rs1_data;
  ex_alu_in2 = (fw_b) ? ex_mem_alu_result : ex_alu_in2_orig;


REFERENCE DOCUMENTS:
====================

  RISC-V ISA Manual: https://riscv.org/specifications/
  RV32I Instruction Set (Appendix A)
  
  5-Stage Pipeline Basics:
    - Hennesy & Paterson "Computer Architecture: A Quantitative Approach"
    - Dave Patterson VLSI Design Course notes
  
  SystemVerilog RTL:
    - IEEE 1800 Standard
    - Sunburst Design Guidelines for RTL


SUPPORT & TROUBLESHOOTING:
===========================

If simulation fails to compile:
  - Check module instantiation names match defined modules
  - Verify port names and widths
  - Ensure all inputs/outputs are declared with logic keyword

If simulation compiles but produces X (unknown) values:
  - Initialize all state variables in reset
  - Load instruction memory before starting simulation
  - Check clock and reset signals

If register values don't match expected:
  - Check pipeline timing with waveforms
  - Verify writeback path (mem_wb_reg_write signal)
  - Check for hazards affecting dependent instructions


*/

//============================================================================
// END OF PROJECT INTEGRATION GUIDE
//============================================================================
