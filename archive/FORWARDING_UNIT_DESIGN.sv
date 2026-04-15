//============================================================================
// FORWARDING UNIT - DESIGN & TRUTH TABLES
//============================================================================

/*

FORWARDING OVERVIEW:
====================

Data Hazards occur when an instruction needs operands that haven't yet
been written back to the register file. The forwarding unit detects these
situations and routes the result directly from earlier pipeline stages
(MEM or WB) to the ALU inputs in EX stage, bypassing the register file.

Three possible sources for EX stage operands:
  1. Register file (normal path)
  2. MEM stage result (data available, just not written yet)
  3. WB stage result (being written this cycle, but available)


TRUTH TABLE FOR FORWARD_A (ex_rs1):
====================================

Inputs (all relevant combinations):
  mem_regwrite | mem_rd    | wb_regwrite | wb_rd    | (ex_rs1 match check)
  ex_rs1 == mem_rd? | ex_rs1 == wb_rd? | Result

Case 1: No hazard (normal path)
  mem_regwrite=0, wb_regwrite=0, or no match with ex_rs1
  → forward_a = 00 (use register file)
  
Case 2: RAW with MEM stage (immediate result available)
  mem_regwrite=1 AND mem_rd==ex_rs1 AND ex_rs1!=0
  → forward_a = 10 (forward from MEM)
  
Case 3: RAW with WB stage (result being written)
  wb_regwrite=1 AND wb_rd==ex_rs1 AND ex_rs1!=0
  AND NOT (mem_regwrite=1 AND mem_rd==ex_rs1)
  → forward_a = 01 (forward from WB)
  
Case 4: Special case (rd==0)
  Even if mem_rd==0 && ex_rs1==0 || wb_rd==0 && ex_rs1==0
  → forward_a = 00 (never forward from x0)


DETAILED TRUTH TABLE:
====================

| mem_rw | mem_rd | wb_rw | wb_rd | ex_rs1 | mem_match | wb_match | forward_a |
|--------|--------|-------|-------|--------|-----------|----------|-----------|
|   0    |   x    |   0   |   x   |   x    |   don't   |   don't  |    00     |
|   0    |   x    |   1   | !=0   |  ==x   |   don't   |   yes    |    01     |
|   0    |   x    |   1   | ==0   |  any   |   don't   |   no     |    00     |
|   1    | !=0    |   x   |   x   |  ==x   |   yes     |   -      |    10     |
|   1    | ==0    |   x   |   x   |  any   |   no      |   -      |  (check wb)|
|   1    | !=0    |   1   | !=0   |  ==x   |   yes     |   yes    |    10 *   |
|   1    | !=0    |   1   | !=0   |  ==x   |   yes     |   no     |    10     |
|   1    | !=0    |   0   |   x   |  ==x   |   yes     |   don't  |    10     |

* MEM has priority over WB

SIMPLIFIED DECISION TREE:
========================

forward_a = ?
  ├─ If (mem_regwrite && mem_rd!=0 && mem_rd==ex_rs1)
  │   └─ forward_a = 10
  ├─ Else if (wb_regwrite && wb_rd!=0 && wb_rd==ex_rs1)
  │   └─ forward_a = 01
  └─ Else
      └─ forward_a = 00


HAZARD SCENARIOS & SOLUTIONS:
=============================

SCENARIO 1: ADD followed by SUB (ALU-ALU hazard)
================================================

 Cycle 1: ADD x3, x1, x2  [IF]
 Cycle 2: ADD x3, x1, x2  [ID]
 Cycle 3: SUB x4, x3, x5  [IC] | ADD x3  [EX] ← writes x3
 Cycle 4: SUB x4, x3, x5  [EX] | ADD x3  [MEM]← x3 in MEM stage
 Cycle 5: ADD x4          [WB] | SUB x4  [MEM]

Without forwarding (SUB in cycle 4, EX stage):
  - SUB tries to read x3 from register file
  - Register file still has old value of x3 (not yet updated)
  - SUB uses WRONG value (data hazard!)
  - Result: incorrect computation

With forwarding:
  - Hazard detector sees: ex_rs1=x3, mem_rd=x3, mem_regwrite=1
  - Sets forward_a = 10 (MEM priority)
  - EX stage ALU mux selects MEM result for operand A
  - SUB uses CORRECT value from ADD result in MEM stage
  - Result: correct computation!


SCENARIO 2: LW followed by ADD (Load-Use hazard)
================================================

 Cycle 1: LW x1, 0(x0)   [IF]
 Cycle 2: LW x1, 0(x0)   [ID]
 Cycle 3: ADD x2, x1, x3 [IF] | LW x1  [EX] ← addresses memory
 Cycle 4: ADD x2, x1, x3 [ID] | LW x1  [MEM]← reads x1 from memory
 Cycle 5: ADD x2, x1, x3 [EX] | LW x1  [WB] ← x1 being written this cycle
 Cycle 6: ???             [??] | ADD x2 [MEM]

In Cycle 5, when ADD is in EX:
  - LW data not yet in register file (write happens at end of WB)
  - But: mem_rd=x1 (MEM stage), mem_regwrite=1, mem_rd==ex_rs1
  - Forwarding: forward_a = 10 (MEM)
  - ADD gets correct LW result from MEM stage result
  - Hazard resolved! (no stall needed)

  NOTE: Typical processors would stall here for 1 cycle because
  memory data isn't available until MEM stage completes.
  To handle properly, need both forwarding AND stall logic.


SCENARIO 3: Multiple simultaneous hazards
==========================================

 Instr A: ADD x3, x1, x2
 Instr B: SUB x4, x3, x5
 Instr C: AND x5, x4, x3
 Instr D: OR  x6, x4, x3

Timeline:
 Cycle 5: Instr C in EX, Instr B in MEM (writing x4), Instr A in WB (writing x3)

  Instr C: AND x5, x4, x3
    - needs x4: ex_rs1=x4
      - Check MEM: mem_rd=x4, mem_regwrite=1 ✓ → forward_a=10
    - needs x3: ex_rs2=x3
      - Check MEM: mem_rd=x4 ✗
      - Check WB:  wb_rd=x3, wb_regwrite=1 ✓  → forward_b=01
    - Result: ADD correct (from MEM) AND OR correct (from WB)

Forwarding handles both operands independently with priority logic.


SCENARIO 4: Double RAW with same register
===========================================

 Instr A: ADD x1, x2, x3
 Instr B: ADD x1, x4, x5  ← overwrites x1 again
 Instr C: ADD x6, x1, x7

When C is in EX:
  - Both MEM and WB have rd=1 (different values!)
  - MEM has newer result (A's ADD)
  - WB has old result (some previous instruction)
  - Forwarding unit chooses MEM (higher priority)
  - C gets correct result


SCENARIO 5: x0 protection (should NOT forward)
===============================================

 Any instruction with rd=0 (x0)
 Even if: ex_rs1=0, mem_rd=0, mem_regwrite=1

 Forwarding logic:
   if (mem_regwrite && (mem_rd == ex_rs1) && (mem_rd != 5'd0))
                                              ^^^^^^^^^^^^^^
                                         This check prevents forwarding from x0

 Reason:
   - x0 is hardwired to 0 in register file
   - Never updated, reads always return 0
   - Never forward x0 even if write attempted


SCENARIO 6: No hazard (independent instructions)
=================================================

 Instr A: ADD x3, x1, x2
 Instr B: ADD x4, x5, x6  ← no dependency on x3

When B is in EX:
  - ex_rs1=x5, ex_rs2=x6
  - MEM stage (Instr A): mem_rd=x3 ✗ (not x5 or x6)
  - WB stage (previous): various rd ✗ (not x5 or x6)
  - Forwarding: forward_a=00, forward_b=00
  - B reads from register file normally
  - No forwarding needed, no stall needed
  - Correct result


IMPLEMENTATION IN EX STAGE MULTIPLEXERS:
=========================================

In EX stage, add muxes to select ALU inputs:

  always_comb begin
      case (forward_a)
          2'b00: alu_in_a = id_ex_rs1_data;      // From regfile
          2'b01: alu_in_a = wb_alu_result;       // From WB stage
          2'b10: alu_in_a = ex_mem_alu_result;   // From MEM stage
          default: alu_in_a = id_ex_rs1_data;
      endcase
      
      case (forward_b)
          2'b00: alu_in_b = id_ex_rs2_data;      // From regfile
          2'b01: alu_in_b = wb_alu_result;       // From WB stage
          2'b10: alu_in_b = ex_mem_alu_result;   // From MEM stage
          default: alu_in_b = id_ex_rs2_data;
      endcase
  end
  
  // ALU
  assign ex_alu_result = alu(ex_alu_in1, ex_alu_in2);

Notes:
  - WB result must be available combinationally (before clock edge)
  - MEM result already latched in EX/MEM register
  - This adds minimal delay (just mux delay)


LIMITATIONS OF FORWARDING ALONE:
================================

❌ Cannot fix: Load-Use hazard (structural hazard)
  - Data from memory arrives in WB stage
  - By time ADD reads x1 from LW, no forward path exists
  - SOLUTION: Add stall logic to delay dependent instruction

❌ Cannot fix: Branch hazard
  - Decision happens in EX stage
  - IF has already fetched wrong instruction
  - SOLUTION: Add branch predictor or flush logic

✓ CAN fix: Most ALU-to-ALU RAW hazards (within 1-2 cycles)


EXTENSION: Support MEM operand for stores (SW forward)
======================================================

For SW instructions, need to forward data for write operation:

  Instr A: ADD x1, x2, x3
  Instr B: SW x1, offset(x4)  ← needs value of x1

Current forwarding handles x1 read via forward_a.
But SW uses x1 as data (not operand to ALU).
Need extra forwarding path for SW data write.

  add logic [1:0] forward_sw;
  
  if (mem_regwrite && mem_rd==ex_rs2 && mem_rd!=0)
      forward_sw = 10;
  else if (wb_regwrite && wb_rd==ex_rs2 && wb_rd!=0)
      forward_sw = 01;
  else
      forward_sw = 00;
      
  // In MEM stage:
  mem_write_data = (forward_sw==10) ? ex_mem_alu_result : ex_mem_rs2_data;

This is left for v2 implementation.


VERIFICATION CHECKLIST:
=======================

✓ MEM stage forwarding enabled
✓ WB stage forwarding enabled
✓ MEM priority > WB priority
✓ x0 never forwarded
✓ Only forward when regwrite=1
✓ Correct register matching (ex_rs1/rs2 vs mem_rd/wb_rd)
✓ Both forward_a and forward_b work independently
✓ No latches in combinational logic
✓ 32-bit result paths properly sized


PERFORMANCE IMPACT:
===================

Pros:
  - Eliminates ~50% of potential RAW hazards
  - No cycle penalty (combinational)
  - Minimal area (just comparators and muxes)

Cons:
  - Cannot handle all hazard types (see limitations)
  - Still need stall logic for load-use cases
  - Increases EX stage critical path slightly (mux + ALU)

Typical CPI improvement: 1.5 → 1.2 with forwarding + stall logic


*/

//============================================================================
// END OF FORWARDING UNIT DESIGN DOCUMENT
//============================================================================
