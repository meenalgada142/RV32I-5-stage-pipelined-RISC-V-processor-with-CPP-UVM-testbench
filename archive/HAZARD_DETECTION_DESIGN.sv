//============================================================================
// HAZARD DETECTION & STALL UNIT - DESIGN DOCUMENTATION
//============================================================================

/*

HAZARD DETECTION OVERVIEW:
==========================

Load-Use Hazard (Structural Hazard):
  Memory data arrives at END of MEM stage (entered in WB stage).
  By the time dependent instruction reaches EX, data is NOT yet available
  for use as operand.
  
  Example:
    LW x1, 0(x2)     [reads memory, but x1 not ready until WB]
    ADD x3, x1, x4   [tries to use x1 in EX stage] ← x1 not yet computed!

Solution: 
  Stall the dependent instruction for 1 cycle to allow LW to complete.
  
  Timeline:
    Cycle 1: LW in EX (memread=1, calculates address)
    Cycle 2: LW in MEM (fetches from memory)
    Cycle 3: LW in WB (x1 now available)
    
    Dependent instruction delayed:
    Cycle 1: ADD not yet fetched
    Cycle 2: ADD in ID (stalled: can't proceed)
    Cycle 3: ADD in EX (now x1 available from regfile or WB)


HAZARD DETECTION LOGIC:
=======================

Combinational check in ID stage:

  if (EX.memread == 1 && EX.rd != 0)  // Load instruction active
  {
      if (EX.rd == ID.rs1 || EX.rd == ID.rs2)  // Destination matches operand
      {
          STALL = 1;  // Freeze pipeline
      }
  }

Conditions:
  • EX.memread must be 1 (load in progress)
  • EX.rd must NOT be 0 (x0 never creates hazards)
  • ID.rs1 or ID.rs2 must match EX.rd


TRUTH TABLE:
============

| ex_memread | ex_rd | id_rs1 | id_rs2 | Match? | stall |
|------------|-------|--------|--------|--------|-------|
|     0      |   x   |   x    |   x    |   -    |   0   | No load
|     1      |   0   |   x    |   x    |   no   |   0   | Load to x0
|     1      |   1   |   1    |   x    |  YES   |   1   | ✓ STALL
|     1      |   1   |   x    |   1    |  YES   |   1   | ✓ STALL
|     1      |   1   |   1    |   1    |  YES   |   1   | ✓ STALL (both)
|     1      |   1   |   2    |   3    |   no   |   0   | No match
|     1      |   5   |   5    |   3    |  YES   |   1   | ✓ STALL
|     1      |   5   |   3    |   5    |  YES   |   1   | ✓ STALL


STALL CONTROL SIGNALS:
======================

Signal                  | Function
-----------------------|----------------------------------------------------------
stall                   | 1 when load-use hazard detected
pc_write_enable         | 0 if stalling (freezes PC, prevents IF from advancing)
if_id_write_enable      | 0 if stalling (freezes IF/ID register)
insert_bubble           | 1 if stalling (zeros out ID/EX control signals)


PIPELINE BEHAVIOR WITH STALL:
=============================

NORMAL (no hazard):
  Cycle 1: [IF: ADD] [ID: SUB] [EX: XOR] [MEM: AND] [WB: OR]
  Cycle 2: [IF: ???] [ID: ADD] [EX: SUB] [MEM: XOR] [WB: AND]
  
  PC increments every cycle (pc_write_enable = 1)
  IF/ID updates every cycle (if_id_write_enable = 1)

WITH STALL (load-use hazard):
  Cycle 1: [IF: ADD] [ID: LW x1] [EX: LW x1] [MEM: prev] [WB: prev]
               ↓
  Cycle 2: [IF: NOP] [ID: ADD x3,x1] [EX: (bubble)]
           (PC frozen) (IF/ID frozen)   
               ↓
           (insert_bubble = 1, zeros ID/EX controls)
               ↓
  Cycle 3: [IF: NOP] [ID: ADD x3,x1] [EX: ADD] [MEM: LW x1] [WB: prev]
           
  On Cycle 3:
    - LW reaches WB (x1 now available)
    - ADD advances from ID to EX
    - ADD can now safely read x1 from regfile (or forwarding)


TIMING DIAGRAM:
===============

Without Stall (Incorrect Result):
  
  Cycle:     1       2       3       4
  --------   --      --      --      --
  IF    : [LW x1][ADD x3][ Z ][ Z ]
  ID    :      [LW x1][ADD x3][ Z ]
  EX    :           [LW x1][ADD x3]  ← x1 NOT ready yet!
  MEM   :                [LW x1][???]
  WB    :                     [LW x1] ← x1 ready too late
  
  Result: ADD gets WRONG value from regfile (old x1)


With Stall (Correct Result):

  Cycle:     1       2       3       4
  --------   --      --      --      --
  IF    : [LW x1][ADD x3][ Z ][ Z ]
  ID    :      [LW x1][ADD x3][ADD x3][next]
                              ↑ stalled
  EX    :           [LW x1][stall][ADD x3]
                           ↑ bubble inserted
  MEM   :                [LW x1][ Z ]
  WB    :                     [LW x1]  ← x1 ready, used in cycle 4
  
  Result: ADD gets CORRECT value (x1 = loaded value)
  
  Control signals during Cycle 2:
    pc_write_enable = 0    (PC frozen at next ADD instruction)
    if_id_write_enable = 0 (IF/ID holds ADD)
    insert_bubble = 1      (ID/EX filled with NOP controls)


EXAMPLE INSTRUCTION SEQUENCES:
==============================

CASE 1: Load-Use Dependency (x1)
  
  Instr 0: LW x1, 0(x2)
  Instr 1: ADD x3, x1, x4     ← STALL (Instr 0 not done with WB)
  
  Timeline:
    Cycle 1: LW(EX), prev(MEM), prev(WB)
    Cycle 2: LW(MEM), ADD(ID)←STALLED, prev(WB)
    Cycle 3: LW(WB), ADD(EX), next(ID)
    
  Stall duration: 1 cycle
  
  Detection in Cycle 1:
    id_rs1 = 1 (ADD needs x1)
    ex_rd = 1 (LW writes x1)
    ex_memread = 1 (LW is load)
    → Hazard detected, stall activated


CASE 2: Load → Multiple Dependent Instructions
  
  Instr 0: LW x5, 8(x3)
  Instr 1: SUB x6, x5, x7     ← STALL
  Instr 2: AND x8, x5, x9     ← No stall (already stalled)
  
  Timeline:
    Cycle 1: LW(EX), prev(MEM)
    Cycle 2: LW(MEM), SUB(ID)←STALLED, prev(WB)
    Cycle 3: LW(WB), SUB(EX), AND(ID)
    Cycle 4: next(IF), next(ID), next(EX)
    
  Only first dependent instruction needs stall.
  Forwarding + stall ensures both SUB and AND execute correctly.


CASE 3: No Hazard (Different Registers)
  
  Instr 0: LW x1, 0(x2)
  Instr 1: ADD x3, x4, x5     ← NO STALL (doesn't use x1)
  
  Timeline:
    Cycle 1: LW(EX), prev(MEM)
    Cycle 2: LW(MEM), ADD(EX), prev(WB)
    Cycle 3: LW(WB), next(IF), next(ID)
    
  No stall needed, pipeline proceeds normally.
  
  Detection in Cycle 1:
    id_rs1 = 4, id_rs2 = 5
    ex_rd = 1 (LW to x1)
    → No match, stall = 0


CASE 4: Load to x0 (No Hazard)
  
  Instr 0: LW x0, 0(x1)   ← Legal but unusual (x0 read-only)
  Instr 1: ADD x2, x0, x3
  
  Detection in Cycle 1:
    ex_memread = 1 (LW active)
    ex_rd = 0
    → Check fails (rd == 0), stall = 0 (no hazard possible)


CASE 5: Both Operands are Load Results
  
  Instr -1: LW x5, 0(x1)
  Instr  0: LW x6, 4(x2)
  Instr  1: ADD x7, x5, x6  ← Needs both x5 and x6
  
  Timeline:
    Cycle 1: LW x5(EX), ??(MEM), ??(WB)
    Cycle 2: LW x5(MEM), LW x6(EX)← Stall (LW x6 is active load)
    Cycle 3: LW x5(WB), LW x6(MEM), ADD(ID)←STALLED
    Cycle 4: next, LW x6(WB), ADD(EX)
    
  ADD stalls in Cycle 3 (waiting for LW x6).
  Forwarding handles LW x5 (already in WB).
  By Cycle 4, both x5 and x6 available.


INTERACTION WITH FORWARDING:
=============================

Stall and forwarding work TOGETHER:

  LW x1, 0(x2)
  ADD x2, x1, x3    ← Stall during: cycles 1-2
  
  Cycle 2 (during stall):
    - LW in MEM stage
    - ADD in ID stage
    - Cannot proceed (data not ready)
  
  Cycle 3 (after stall):
    - LW in WB stage (x1 written)
    - ADD in EX stage
    - ALU can read x1 from regfile (or forwarding from WB stage)
    - ADD executes correctly!

  Forwarding helps by:
    - If x1 available in WB stage, forward it without extra cycle
    - But still need the stall for the structural delay


EDGE CASES & CORNER CASES:
=========================

Case A: x0 (Architecture register, hardwired)
  LW x0, 0(x1)      ← Legal but pointless
  ADD x2, x0, x3    ← No hazard (x0 always 0)
  Detection: ex_rd == 0 → Never stall
  ✗ (never stalls from x0 write)

Case B: Two consecutive loads
  LW x1, 0(x2)
  LW x3, 4(x2)      ← Could stall if dependent on first LW
  But LW x3 doesn't USE x1, only produces x3
  → No stall for LW x3 (instruction itself doesn't depend)

Case C: Load then immediate-use ALU
  LW x1, 0(x2)
  ADDI x2, x3, 10   ← No hazard (doesn't use x1 result)
  → No stall

Case D: rs1 == rs2
  LW x5, 0(x1)
  ADD x7, x5, x5    ← Both operands are x5
  Detection:
    (ex_rd == id_rs1) OR (ex_rd == id_rs2)
    → Triggers stall for either match
  → Correct behavior (stalls once, fixes both operands)


WHY EXACTLY 1 CYCLE STALL?
==========================

Memory access timeline:
  Cycle N:   LW in EX stage (calculates address)
  Cycle N+1: LW in MEM stage (data fetched from memory)
  Cycle N+2: LW in WB stage (x1 register written)
  
Dependent instruction timeline WITHOUT stall:
  Cycle N:   LW(EX)
  Cycle N+1: LW(MEM), dependent(ID) ← tries to read x1 here (NOT ready)
  Cycle N+2: LW(WB),  dependent(EX) ← x1 ready here but too late

Dependent instruction timeline WITH stall:
  Cycle N:   LW(EX)
  Cycle N+1: LW(MEM), dependent(ID, STALLED)     ← freeze ID/IF
  Cycle N+2: LW(WB),  dependent(EX) ← x1 ready here NOW available!
  
One stall cycle is sufficient because:
  - LW moves from EX→MEM→WB (2 stages)
  - Dependent instr can advance 1 stage per cycle
  - With 1-cycle stall, dependent reaches EX just as x1 is written


SYNTHESIS NOTES:
================

This module is purely combinational (no storage, no latches):
  - 2 comparators (rs1/rs2 vs rd)
  - 1 OR gate (two matches)
  - 1 AND gate (memread && hz detection)
  - 4 assignment statements

Result:
  ✓ No latches (all always_comb)
  ✓ No combinational loops (inputs → outputs, no feedback)
  ✓ Fast timing (just comparators and gates)
  ✓ Small area (~50-100 gates)


DEBUGGING TIPS:
===============

If stall not working (dependent instr still executes early):
  □ Check ex_memread signal (should be 1 for LW)
  □ Check id_rs1 and id_rs2 signals (should match if hazard present)
  □ Check if_id_write_enable is 0 when stall=1
  □ Check pc_write_enable is 0 when stall=1
  □ Verify insert_bubble is 1 (ID/EX controls should be zeroed)

If excessive stalls (stalls when shouldn't):
  □ Check if ex_rd == 0 case is handled correctly
  □ Verify ex_memread goes to 0 after load instruction passes EX
  □ Check for stuck load instruction


*/

//============================================================================
// END OF HAZARD DETECTION DESIGN DOCUMENTATION
//============================================================================
