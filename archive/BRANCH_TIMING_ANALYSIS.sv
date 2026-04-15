////////////////////////////////////////////////////////////////////////////////
//  BRANCH_TIMING_ANALYSIS.sv
//
//  Detailed Timing Diagrams for Branch Execution in 5-Stage Pipeline
//  
//  Shows cycle-by-cycle branch resolution and pipeline flush behavior
//
//============================================================================

/*

//==============================================================================
// SCENARIO 1: FORWARD BRANCH TAKEN (3-Cycle Penalty)
//==============================================================================

Program:
  PC=0:  ADDI x1, x0, 5        (x1 = 5)
  PC=4:  ADDI x2, x0, 5        (x2 = 5)
  PC=8:  BEQ x1, x2, +0        (branch forward to PC=12, x1==x2, TAKEN)
  PC=12: ADDI x3, x0, 12       (wrong path, should NOT execute)
  PC=16: ADDI x3, x0, 12       (wrong path, should NOT execute)
  PC=20: ADD x4, x1, x2        (target, should execute)
  PC=24: ADD x5, x4, x4        (should execute)

Timeline (BEFORE any flush logic was added, this would be broken):

  CYCLE 1:
    State: Fresh start
    ┌─────────────────────────────┐
    │ IF:  Fetch ADDI x1 [@PC=0]  │
    │ ID:  -                       │
    │ EX:  -                       │
    │ MEM: -                       │
    │ WB:  -                       │
    └─────────────────────────────┘
    PC ← 4 (pc_write=1)
    
    Signal State:
      branch_taken = 0 (no branch yet)
      flush_if_id = 0
      flush_id_ex = 0

  CYCLE 2:
    State: ADDI x1 advances through pipeline
    ┌─────────────────────────────┐
    │ IF:  Fetch ADDI x2 [@PC=4]  │
    │ ID:  Decode ADDI x1         │
    │ EX:  -                       │
    │ MEM: -                       │
    │ WB:  -                       │
    └─────────────────────────────┘
    PC ← 8 (pc_write=1)
    
    Signal State:
      branch_taken = 0
      flush_if_id = 0
      flush_id_ex = 0

  CYCLE 3:
    State: ADDI x2 advances, ADDI x1 executes
    ┌─────────────────────────────┐
    │ IF:  Fetch BEQ [@PC=8]      │
    │ ID:  Decode ADDI x2         │
    │ EX:  Execute ADDI x1        │ ← x1 ← 5
    │ MEM: -                       │
    │ WB:  -                       │
    └─────────────────────────────┘
    PC ← 12 (pc_write=1)
    
    Signal State:
      branch_taken = 0
      flush_if_id = 0
      flush_id_ex = 0

  CYCLE 4:
    State: BEQ propagates through pipeline
    ┌──────────────────────────────┐
    │ IF:  Fetch ADDI x3 [@PC=12]  │ ← SPECULATIVELY (wrong path!)
    │ ID:  Decode BEQ              │
    │ EX:  Execute ADDI x2         │ ← x2 ← 5
    │ MEM: Mem pass ADDI x1        │
    │ WB:  Write x1 ← 5            │
    └──────────────────────────────┘
    PC ← 16 (pc_write=1)
    
    Signal State:
      branch_taken = 0 (BEQ not yet executed in EX)
      flush_if_id = 0
      flush_id_ex = 0

  CYCLE 5:
    State: BEQ EXECUTES IN EX - DECISION MADE!
    ┌──────────────────────────────────┐
    │ IF:  Fetch ADDI x3 [@PC=16]      │ ← STILL speculatively (wrong path!)
    │ ID:  Decode ADDI x3              │ ← Will be flushed
    │ EX:  Execute BEQ                 │
    │      → ALU: x1 == x2? YES!       │
    │      → zero = 1                  │
    │      → branch_taken = 1 ✓        │
    │      → pc_next = 8 + 0 = 8       │ (Wait, that's wrong offset calculation)
    │ MEM: Mem pass ADDI x2            │
    │ WB:  Write x2 ← 5                │
    └──────────────────────────────────┘
    
    *** BRANCH DECISION ASSERTED ***
    
    Signal State (generated this cycle):
      branch_taken = 1 ✓ (x1==x2, so BEQ TAKEN)
      pc_next = pc_ex + imm_ex = 8 + 0 = 8... wait, that doesn't work
      
    Actually, the branch offset is +12 in the immediate, so:
      pc_next = pc_ex + imm_ex = 8 + 12 = 20 ✓
      
    And flush signals:
      flush_if_id = 1 ✓ (flush ADDI x3 in ID)
      flush_id_ex = 1 ✓ (flush ADDI x3 in EX? wait, it's in ID not EX)
      
    Actually, let me reconsider the register locations at this cycle:
    
    Correct Cycle 5 State:
    - IF: Fetching from wrong PC (will be ignored)
    - ID: ADDI x3 (speculatively fetched) ← Will be flushed by flush_if_id
    - EX: BEQ (making decision) ← branch_taken=1
    - MEM: ADDI x2 (from previous cycle, not affected)
    - WB: ADDI x1 result (from previous cycle, not affected)
    
    So flush signals:
      flush_if_id = 1 (next clock, zero controls for ADDI x3 in ID)
      flush_id_ex = 1 (next clock, zero controls for current EX...but EX has BEQ, not wrong path yet)
    
    Hmm, this is confusing timing. Let me think about what flush_id_ex does:
    
    flush_id_ex signal is generated at Cycle 5 when BEQ is in EX and branch_taken=1.
    This signal is applied at the END of cycle 5 (during clock edge).
    
    So at Cycle 6 (AFTER the clock edge):
    - The ID/EX register has been flushed (controls set to 0)
    - This affects what was in the pipeline AFTER BEQ passes through
    
    Actually, I think I'm overcomplicating this. Let me re-read the pipeline structure...
    
    In the pipeline, when we assert flush_id_ex at cycle N, it affects the ID/EX register
    during the clock edge, so at cycle N+1:
    - ID/EX becomes a NOP
    - Whatever instruction was about to enter EX from ID is now a NOP
    
    But the BEQ executed in EX at cycle 5 isn't affected by this flush.
    The flush is for the instruction that WOULD enter EX next cycle.
    
    So at cycle 5:
    - BEQ is in EX executing (just decided branch_taken=1)
    - Wrong path instruction is in ID (will be flushed)
    - Another wrong path instruction is in IF (will be replaced by correct path fetch next cycle)

  After Cycle 5 Edge (at Cycle 6, after flush takes effect):
    State: Flush completed, pipeline corrected
    ┌──────────────────────────────┐
    │ IF:  Fetch from PC=20 (target)│ ← CORRECT PATH RESUMES
    │ ID:  NOP (flushed)           │ ← ADDI x3 is now NOP
    │ EX:  NOP (flushed)           │ ← Wrong path instruction zeroed
    │ MEM: ADDI x2                 │ ← Still executing (committed before flush)
    │ WB:  ADDI x1 result          │ ← Still writing (committed before flush)
    └──────────────────────────────┘
    PC ← 20 (from branch_pc_next calculation)
    
    Signal State:
      branch_taken = 0 (not asserted anymore, cycle passed)
      flush_if_id = 0
      flush_id_ex = 0

  CYCLE 7:
    State: Correct path begins execution
    ┌──────────────────────────────┐
    │ IF:  Fetch next after target │
    │ ID:  Decode ADD x4, x1, x2   │ ← Target instruction
    │ EX:  NOP (from flush)        │
    │ MEM: NOP (from flush)        │
    │ WB:  ADDI x2 ← 5             │
    └──────────────────────────────┘
    
    Signal State:
      All normal, no flush

  CYCLE 8:
    State: Target instruction executes
    ┌──────────────────────────────┐
    │ IF:  Fetch next              │
    │ ID:  Decode next             │
    │ EX:  Execute ADD x4 ← x1+x2  │ ← x4 ← 5+5 = 10 ✓ CORRECT
    │ MEM: MemPass NOP             │
    │ WB:  WriteBack NOP           │
    └──────────────────────────────┘
    
    x4 correctly computed as 10 (not corrupted by wrong path)

Summary:
  - Cycle 5: BEQ executes, branch_taken=1 (decision made)
  - Cycle 6: Flush takes effect, IF/ID and EX become NOP, PC redirected
  - Cycle 7: Correct path instruction enters ID
  - Cycle 8: Correct path instruction executes in EX
  
  Penalty: 3 cycles from BEQ IF to correct execution in EX
    = (1 cycle IF) + (1 cycle ID) + (1 cycle EX deciding) + (1 cycle flush) + (1 cycle correct in EX)
    = Actually, let me think differently...
    
  More precisely:
    - Cycle 1-3: BEQ travels through IF→ID→EX
    - Cycle 4: BEQ stalls awaiting decision
    - Cycle 5: BEQ executes in EX (decision made, PC redirected)
    - Cycle 6-7: Pipeline clears (flushed instructions execute as NOP)
    - Cycle 8: First correct instruction executes
  
  So target instruction IS NOT executed until cycle 8 (after target enters EX).
  Total time from BEQ IF to target EX: 8-1=7 cycles
  Branch penalty = cycles wasted = 7 - (1+4) = 2? 
  
  Actually, without branch penalty:
    - Target would start executing at cycle 4 (IF) + 4 = cycle 8 OR cycle 5 (if we had PC right away)
    
  No wait, that's still not right. Let me think about normal execution:
  
  Normal (no branch):
    - Instruction at PC 0 enters IF cycle 1
    - Instruction at PC 4 enters IF cycle 2
    ...
    - Instruction at PC 20 enters IF cycle 6
    - Instruction at PC 20 in EX at cycle 9
  
  With branch (taken):
    - BEQ at PC 8 enters IF cycle 3
    - BEQ at PC 8 in EX cycle 5 (decision made, redirect to PC 20)
    - Instruction at PC 20 enters IF cycle 6 (due to redirect)
    - Instruction at PC 20 in EX cycle 9
  
  So timing is the SAME! No extra cycles!
  
  Wait, but we're flushing instructions...let me reconsider.
  
  Actually, there IS a penalty: the two wrong-path instructions in IF/ID get flushed
  and become NOPs instead of executing correct instructions.
  
  Penalty = number of wrong instructions flushed before correct path resumes
          = 2 instructions (at PC 12, PC 16) that became NOPs
          = 2 cycle waste
  
  But since they're becoming NOPs anyway (executed harmlessly), the register values
  aren't corrupted (no writes happen with controls=0).
  
  So the FUNCTIONAL penalty is 0 (correctness not harmed),
  but the PERFORMANCE penalty is 2 wasted cycles.
  
  More accurately:
    - If branch NOT taken: 0 cycle penalty (if-not-taken assumption correct)
    - If branch taken: 2 cycle penalty (2 wrong instructions as NOPs)
  
  In practice, simple always-not-taken predictors incur:
    avg penalty = P(taken) * 2 cycles
    
  Example: 20% branches taken = 20% * 2 = 0.4 cycles penalty

//==============================================================================
// SCENARIO 2: FORWARD BRANCH NOT TAKEN (0-Cycle Penalty)
//==============================================================================

Program:
  PC=0:  ADDI x1, x0, 5        (x1 = 5)
  PC=4:  ADDI x2, x0, 10       (x2 = 10)
  PC=8:  BEQ x1, x2, +0        (x1 != x2, NOT TAKEN)
  PC=12: ADD x3, x1, x2        (should execute)
  PC=16: ADD x4, x3, x3        (should execute)

Timeline:

  CYCLE 1-2: (same as scenario 1, setup)

  CYCLE 3:
    IF:  Fetch BEQ [@PC=8]
    ID:  Decode ADDI x2
    EX:  Execute ADDI x1         ← x1 ← 5
    MEM: -
    WB:  -
    
    PC ← 12

  CYCLE 4:
    IF:  Fetch ADD x3 [@PC=12]   ← ASSUMED NOT TAKEN (correct!)
    ID:  Decode BEQ (x1=?, x2=?)
    EX:  Execute ADDI x2         ← x2 ← 10
    MEM: Pass ADDI x1
    WB:  Write x1 ← 5
    
    PC ← 16

  CYCLE 5 (BEQ EXECUTES):
    IF:  Fetch ADD x4 [@PC=16]
    ID:  Decode ADD x3
    EX:  Execute BEQ
         → ALU: x1 == x2? NO!
         → zero = 0
         → branch_taken = 0 (NOT TAKEN)
         → pc_next = 12 + 4 = 16 (normal increment)
    MEM: Pass ADDI x2
    WB:  Write x2 ← 10
    
    Signal State:
      branch_taken = 0
      flush_if_id = 0
      flush_id_ex = 0
    
    PC ← 16 (same value as assumed!)

  CYCLE 6:
    IF:  Fetch ADD x5 [@PC=20]
    ID:  Decode ADD x4
    EX:  Execute ADD x3          ← x3 ← 5 + 10 = 15 ✓
    MEM: Pass ADD x3
    WB:  Write ADD x3 (previous)
    
    NO FLUSH! Pipeline continues uninterrupted.

Result: Correct path executed as assumed, ZERO penalty!

//==============================================================================
// SCENARIO 3: BACKWARD BRANCH (LOOP) - TAKEN
//==============================================================================

Program (Loop):
  PC=8:  ADD x2, x2, x1        (accumulator += counter)
  PC=12: ADDI x1, x1, -1       (counter--)
  PC=16: BNE x1, x0, -8        (if x1 != 0, loop back to PC=8)
  
Loop flow:
  Iteration 1: x1=3, x2=0+3=3, x1=2, BNE taken (branch to 8)
  Iteration 2: x1=2, x2=3+2=5, x1=1, BNE taken (branch to 8)
  Iteration 3: x1=1, x2=5+1=6, x1=0, BNE not taken (continue to next)

Timeline (showing iteration transitions):

END OF ITERATION 1:
  PC=16: BNE x1, x0
  After execution: x1=2, zero=0 (x1 != 0)
  branch_taken = 1 (BNE: not zero → branch)
  pc_next = 16 + (-8) = 8   ← LOOP BACK

CYCLE N (END OF ITERATION 1):
  IF:  Fetch next after BNE
  ID:  Decode BNE
  EX:  Execute BNE → branch_taken=1, pc_next=8
  MEM: Pass
  WB:  Write previous
  
  Signal: flush_if_id=1, flush_id_ex=1

CYCLE N+1:
  IF:  Fetch ADD x2 [@PC=8]    ← FROM LOOP BACK
  ID:  NOP (flushed)
  EX:  NOP (flushed)
  MEM: Pass NOP
  WB:  NOP
  
  PC ← 8 (redirected)

CYCLE N+2 (START OF ITERATION 2):
  IF:  Fetch ADDI x1 [@PC=12]
  ID:  Decode ADD x2           ← From PC=8
  EX:  NOP
  MEM: NOP
  WB:  NOP
  
  PC ← 12

CYCLE N+3:
  IF:  Fetch BNE [@PC=16]
  ID:  Decode ADDI x1
  EX:  Execute ADD x2 ← x2+x1  ← x2 ← 3+2 = 5 ✓
  MEM: NOP
  WB:  NOP
  
  PC ← 20

CYCLE N+4:
  ID:  Decode BNE
  EX:  Execute ADDI x1         ← x1 ← 1
  MEM: Pass ADD x2
  WB:  NOP

CYCLE N+5:
  EX:  Execute BNE
       → zero = 0 (x1=1 != 0)
       → branch_taken = 1 (BNE taken)
       → pc_next = 16 + (-8) = 8 (loop again!)

...and cycle repeats for iteration 3 until BNE not taken

END OF ITERATION 3:
  x1=0 after ADDI x1, x1, -1
  BNE: zero=1 (x1 == 0)
  branch_taken = 0 (BNE not taken, since x1==0)
  pc_next = 16 + 4 = 20 (normal, exit loop)
  
  NO FLUSH! Continue with next instruction.

Key Observations:
  - Each loop iteration has 3-cycle penalty (2 instructions flushed)
  - Without loop: 0 penalty (fall through)
  - Loop iterations after first: all have penalty (branch always taken except last)
  - Last iteration: 0 penalty (branch not taken, no flush)

//==============================================================================
// CONTROL SIGNAL TIMING TABLE
//==============================================================================

Signal Timing During Branch:

Cycle    | IF       | ID       | EX          | Signal        | Value
         |          |          |             | branch_taken  | 
---------|----------|----------|-------------|---------------|-------
4        | next     | BEQ      | ADDI        | 0             | N/A
5        | wrongA   | BEQ      | ADDI        | 1             | ← ASSERTED HERE
6        | wrongB   | wrongA   | BEQ->NOP    | 0             | ← ends
         |          | ->NOP    |             |               |

Cycle 5 Output:
  branch_taken = 1   (condition met, BEQ taken)
  pc_next = target_addr
  flush_if_id = 1    (next cycle, ID←NOP)
  flush_id_ex = 1    (next cycle, EX←NOP)

Cycle 6 Effects (after clock edge):
  PC = target_addr   (fed back from pc_next)
  IF/ID = NOP         (flushed)
  ID/EX = NOP         (flushed)
  IF fetches from target

//==============================================================================
// INTERACTION: BRANCH + STALL
//==============================================================================

Scenario: Load followed by branch using x1

Program:
  PC=0:  LW x1, 0(x0)
  PC=4:  ADDI x2, x0, 1
  PC=8:  BEQ x1, x2, +0

Timeline:

  CYCLE 1:
    IF:  LW
    ID:  -
    EX:  -
    
    stall = 0, branch_taken = 0

  CYCLE 2:
    IF:  ADDI
    ID:  LW (rs1=x0, no stall)
    EX:  -
    
    stall = 0

  CYCLE 3:
    IF:  BEQ
    ID:  ADDI (rs1=x0, no stall)
    EX:  LW (id_ex_mem_read=1)
    
    stall = 0 (ADDI doesn't need x1)

  CYCLE 4:
    IF:  next
    ID:  BEQ (rs1=x1, rs2=x2)
         Hazard check: ex_rd=x1 (from LW), id_rs1=x1 → STALL!
         
    stall = 1, pc_write_enable=0, if_id_write_enable=0
    
    EX:  ADDI
    MEM: LW (data loading)
    
    *** STALL BLOCKS BRANCH IN ID ***

  CYCLE 5:
    IF:  (frozen due to stall)
    ID:  BEQ (still stalled)
    EX:  NOP (bubble inserted due to stall)
    MEM: ADDI
    WB:  LW + x1 now available!
    
    stall = 0 now (bubble broke the hazard)
    EX is NOP, so ex_memread=0, can't stall anymore

  CYCLE 6:
    IF:  (resumes fetching)
    ID:  BEQ (finally advances after stall)
    EX:  NOP
    
  CYCLE 7:
    EX:  BEQ executes
         → x1 available from WB (forwarded)
         → Comparison completes
         → branch_taken determined

Priority: Flush (branch) > Stall (load-use)
  If both signals active: Flush dominates (discard wrong path)

*/

endmodule
