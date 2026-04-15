//============================================================================
// FORWARDING UNIT - INTEGRATION SUMMARY
//============================================================================

/*

FILES CREATED:
==============

1. rv32i_forwarding_unit.sv
   - Standalone forwarding unit module
   - Inputs: ex_rs1, ex_rs2, mem_rd, mem_regwrite, wb_rd, wb_regwrite
   - Outputs: forward_a, forward_b (2-bit control signals)
   - Fully synthesizable, combinational

2. FORWARDING_UNIT_DESIGN.sv
   - Comprehensive design documentation
   - Truth tables
   - Hazard scenarios with solutions
   - Implementation guidelines

3. tb_forwarding_unit.sv
   - Standalone testbench for forwarding unit
   - 12 test cases covering all scenarios
   - Pass/fail assertions
   - No dependencies on pipeline

4. rv32i_pipe5_with_forwarding.sv
   - Enhanced 5-stage pipeline with integrated forwarding
   - Forwarding unit instantiated in EX stage
   - Multiplexers for ALU inputs based on forward_a, forward_b
   - All data hazards now resolved

5. tb_pipe5_with_forwarding.sv
   - Testbench for pipelined processor WITH forwarding
   - Tests both simple and hazardous instruction sequences
   - Verifies correct results for dependent instructions
   - Expected results documented


HOW FORWARDING WORKS IN THE PIPELINE:
=====================================

Pipeline Structure:
  IF → ID → EX → MEM → WB

In EX stage, with forwarding:

  ex_rs1 → [forwarding unit] → {forward_a = 00/01/10}
                                        ↓
  id_ex_rs1_data ──┐              case forward_a:
  ex_mem_alu_result├─→ [mux] ────→ 00: id_ex_rs1_data (regfile)
  wb_write_data ───┘               01: wb_write_data (WB result)
                                   10: ex_mem_alu_result (MEM result)
                                        ↓
                                   ex_alu_in1 → ALU

Same logic for forward_b and ex_alu_in2.

Priority:
  MEM > WB > RegFile
  (i.e., most recent result has highest priority)


INTEGRATION STEPS:
==================

Step 1: Replace old pipeline with forwarding version
  OLD:  use rv32i_pipe5.sv
  NEW:  use rv32i_pipe5_with_forwarding.sv

Step 2: Compile modules in order
  $ vlog rv32i_alu.sv
  $ vlog rv32i_regfile.sv
  $ vlog rv32i_decoder.sv
  $ vlog rv32i_forwarding_unit.sv
  $ vlog rv32i_pipe5_with_forwarding.sv
  $ vlog tb_pipe5_with_forwarding.sv

Step 3: Run simulation
  $ vsim -c tb_pipe5_with_forwarding -do "run -all; quit"

Step 4: Verify output
  Expected: All registers match predicted values
  With forwarding: x4=16, x5=32 (dependent on x3)
  Without forwarding: x4, x5 would be 0 or garbage


EXPECTED BEHAVIOR CHANGE:
==========================

BEFORE (without forwarding):
  Program: ADDI x1, x0, 5
           ADDI x2, x0, 3
           ADD x3, x1, x2    ✓ (x1, x2 in regfile)
           ADD x4, x3, x3    ✗ (x3 not yet written)
           
  Result: x4 = 0 or undefined (WRONG)

AFTER (with forwarding):
  Program: ADDI x1, x0, 5
           ADDI x2, x0, 3
           ADD x3, x1, x2    ✓ (x1, x2 in regfile)
           ADD x4, x3, x3    ✓ (x3 forwarded from MEM)
           
  Result: x4 = 16 (CORRECT)


HOW TO VERIFY FORWARDING IS WORKING:
=====================================

Method 1: Check register values in simulation
  After test completes, print final registers:
  If x4=16 and x5=32, forwarding is working!
  If x4=0 or x5=0, forwarding may be broken.

Method 2: Trace forwarding signals in waveform
  Add to testbench:
    initial $dumpvars(0, tb_pipe5_with_forwarding);
  
  Look for:
    - dut.forward_a, dut.forward_b signals
    - When should be 10 (MEM) or 01 (WB)
    - Verify mux selects correct input

Method 3: Run comparison
  Compare results:
    rv32i_pipe5.sv (no forwarding)
    vs
    rv32i_pipe5_with_forwarding.sv (with forwarding)
  
  Without forwarding: x4 should be 0
  With forwarding: x4 should be 16


PERFORMANCE IMPACT:
===================

Pros:
  ✓ Eliminates most RAW (Read-After-Write) hazards
  ✓ No cycle penalty (forwarding is combinational)
  ✓ Minimal area overhead (~5-10 gates per mux)
  ✓ Increases pipeline throughput (CPI closer to 1.0)

Cons:
  ✗ Add mux delay to EX stage critical path
  ✗ Cannot handle load-use hazard (still need stalls)
  ✗ Cannot handle control hazards (still need flushes)

Estimated CPI:
  Without forwarding: 1.5 - 2.0 (many data stalls)
  With forwarding:    1.2 - 1.3 (fewer stalls needed)
  With forwarding + stall logic: 1.1 - 1.2 (near ideal)


NEXT STEPS - PHASE 2:
====================

After forwarding is verified working:

1. Add Stall Logic (load-use hazard)
   - Detect: (id_ex_mem_read && (id_ex_rd == id_rs1 or id_rs2))
   - Stall: Hold ID stage + bubble EX stage
   - Expected impact: Handles LW followed by dependent ALU op

2. Add Branch Support
   - Route exmem_take_branch back to IF
   - Flush IF/ID and ID/EX on branch taken
   - Add PC mux for branch target
   - Expected: Branches execute (with 2-cycle penalty)

3. Add Memory Interface
   - Replace instr_mem/data_mem stubs with real modules
   - Support byte enablers for LB/LH/SB/SH
   - Add address translation if desired

4. Performance Testing
   - Benchmark various programs
   - Measure CPI, throughput
   - Compare with theoretical ideal


DEBUGGING CHECKLIST:
====================

If simulation shows wrong results:

☐ Is forwarding unit correctly computing forward_a/b?
  Check: tb_forwarding_unit.sv results

☐ Are muxes correct size (32 bits)?
  Check: ex_alu_in1, ex_alu_in2 assignments

☐ Are forwarding signals routed correctly?
  Check: ex_forwarding port connections

☐ Is regfile write working?
  Check: mem_wb_reg_write, mem_wb_rd, wb_write_data paths

☐ Clock and reset?
  Check: clk toggling, rst releases properly

☐ Are ALU inputs correct before mux?
  Check waveform: id_ex_rs1_data, id_ex_rs2_data values

☐ Is EX/MEM register capturing ALU result?
  Check: ex_mem_alu_result matches ex_alu_result after clock


COMMON MISTAKES:
================

❌ Mistake: Forwarding always outputs 10 (MEM priority)
   Fix: Check OR/AND logic in forwarding conditions
   Check: wb_regwrite signal propagation

❌ Mistake: Forwarding works for A but not B
   Fix: Copy-paste error in forward_b logic
   Fix: Check ex_rs2 vs ex_rs1 condition

❌ Mistake: x0 still gets forwarded
   Fix: Check (rd != 5'd0) conditions in forwarding

❌ Mistake: MEM result not available in forward mux
   Fix: Ensure ex_mem_alu_result is in EX/MEM register (always_ff)
   Fix: Clock must be running before mux sees result

❌ Mistake: Forwarding mux breaks ALU sign propagation
   Fix: Ensure all 32 bits forwarded (not just lower 16)
   Fix: Use logic [31:0] not logic [15:0]


SIMULATION COMMANDS:
====================

# Test forwarding unit alone
$ vlog rv32i_forwarding_unit.sv tb_forwarding_unit.sv
$ vsim -c tb_forwarding_unit -do "run -all; quit"

# Test full pipeline with forwarding
$ vlog rv32i_*.sv
$ vlog tb_pipe5_with_forwarding.sv
$ vsim -c tb_pipe5_with_forwarding -do "run -all; quit"

# Interactive debugging with waveforms
$ vsim tb_pipe5_with_forwarding
> add wave -r /tb_pipe5_with_forwarding/dut/*
> add wave /tb_pipe5_with_forwarding/dut/forward_a
> add wave /tb_pipe5_with_forwarding/dut/forward_b
> run -all
> wave zoom full

# Check internal signals
> examine /tb_pipe5_with_forwarding/dut/ex_alu_in1
> examine /tb_pipe5_with_forwarding/dut/ex_mem_alu_result


SUMMARY TABLE:
==============

| Aspect           | Without Forwarding | With Forwarding    |
|------------------|--------------------|--------------------|
| Data hazards     | Requires stalls    | Most resolved      |
| Load-use hazard  | 1 stall needed     | 1 stall (MEM late) |
| ALU-ALU hazard   | 2 stalls w/o fwd   | No stall needed    |
| CPI              | ~1.8               | ~1.2               |
| Area overhead    | Baseline           | +5-10%             |
| Critical path    | Register only      | +mux delay         |


KEY TAKEAWAY:
=============

Forwarding is the PRIMARY technique for reducing data hazard stalls
in pipelined processors. It (correctly) bypasses register file reads
when the result is already available in a later pipeline stage.

This implementation demonstrates:
  1. Clean separation of forwarding logic (separate module)
  2. Priority-based forwarding (MEM > WB)
  3. Integration into full 5-stage pipeline
  4. Verification that dependent instructions now work

Next phases add stall logic (structural hazards) and branch support
(control hazards) to achieve near-ideal CPI.


*/

//============================================================================
// END OF FORWARDING INTEGRATION SUMMARY
//============================================================================
