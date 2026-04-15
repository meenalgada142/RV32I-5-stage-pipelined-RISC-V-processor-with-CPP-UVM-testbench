////////////////////////////////////////////////////////////////////////////////
//  BRANCH_CONTROL_DESIGN.sv
//  
//  Comprehensive Design Guide for Branch Support in 5-Stage Pipeline
//  
//  Covers: Branch decision logic, PC redirection, pipeline flush,
//          control hazard timing, and integration with forwarding/stalls
//
//==============================================================================

//==============================================================================
// 1. BRANCH HAZARD FUNDAMENTALS
//==============================================================================

/*
  WHAT IS A CONTROL HAZARD?
  
  In a pipelined processor, instruction fetch (IF) happens before branch 
  decision (EX). This creates a 3-cycle gap:
  
  Cycle 1: Branch instruction fetched (IF stage)
  Cycle 2: Branch propagates through ID
  Cycle 3: Branch executes in EX (decision made)
  
  Problem: By cycle 3, IF has already fetched 2 subsequent instructions
           based on assumption that branch NOT taken
  
  Result: If branch IS taken, those 2 instructions are WRONG
  
  EXAMPLE:
    PC=100: BEQ x1, x2, +12    ← Branch instruction
    PC=104: ADDI x3, x0, 5     ← Fetched speculatively (assumes branch not taken)
    PC=108: ADD x4, x5, x6     ← Also fetched speculively
    ...
    PC=112: Target instruction ← Should execute here if branch taken
  
  If branch IS taken:
    - x3, x4 should NOT execute (wrong path)
    - Target instruction at PC=112 should execute
    - Need to flush IF/ID and ID/EX (throw away speculatively fetched)

//==============================================================================
// 2. BRANCH RESOLUTION TIMING
//==============================================================================

  INSTRUCTION TIMELINE (Branch Taken Case):
  
  Cycle 1:
    IF:  BEQ fetched from instr_mem[100]
    ID:  empty
    EX:  empty
    MEM: empty
    WB:  empty
    
    pc_write_enable = 1 (IF advances)
    Action: PC → 104
  
  Cycle 2:
    IF:  ADDI fetched from instr_mem[104] (speculatively)
    ID:  BEQ decoded (rs1, rs2, imm decoded)
         branch=1, branch_type=0 (BEQ)
    EX:  empty
    MEM: empty
    WB:  empty
    
    pc_write_enable = 1 (IF advances)
    Action: PC → 108
    
    *** Register IF/ID with BEQ instruction ***
  
  Cycle 3:
    IF:  ADD fetched from instr_mem[108] (speculatively, WRONG!)
    ID:  ADDI decoded (rs1=x0, imm=5)
    EX:  BEQ executing
         - ALU compares: x1 == x2
         - Condition met: branch_taken = 1
         - PC calculation: pc_next = 100 + 12 = 112
         - Flush signals: flush_if_id = 1, flush_id_ex = 1
    MEM: empty
    WB:  empty
    
    *** BRANCH DECISION MADE IN EX STAGE ***
    *** BOTH FLUSH SIGNALS ASSERTED: pipeline cleared of wrong path ***
    
    pc_write_enable = 1 (normal operation, can still write PC)
    Action: PC ← 112 (redirected from ALU calculation)
  
  Cycle 4:
    IF:  Correct instruction fetched from instr_mem[112] ← NEW PATH
    ID:  empty (IF/ID flushed to NOP)
    EX:  empty (ID/EX flushed to NOP)
    MEM: empty
    WB:  empty
    
    *** PIPELINE RESUMED WITH CORRECT PATH ***
  
  Cycle 5:
    IF:  next instruction fetched
    ID:  Target instruction decoded
    EX:  empty
    MEM: empty
    WB:  empty
  
  Key Observation: 3-cycle penalty from BEQ to target execution
    - Cycle 1: BEQ in IF
    - Cycle 2: BEQ in ID
    - Cycle 3: BEQ in EX (decision made, flush happens)
    - Cycle 4: Empty (flushed stages)
    - Cycle 5: Target starts executing (x1, x2, etc. could update it)

//==============================================================================
// 3. BRANCH NOT TAKEN (No Flush)
//==============================================================================

  For comparison, when branch NOT taken:
  
  Cycle 1:
    IF:  BEQ fetched
    Action: PC → 104
  
  Cycle 2:
    IF:  ADDI fetched (correctly!)
    ID:  BEQ
    Action: PC → 108
  
  Cycle 3:
    IF:  ADD fetched (correctly!)
    ID:  ADDI
    EX:  BEQ
         - ALU compares: x1 != x2 (not equal)
         - Condition NOT met: branch_taken = 0
         - PC calculation: pc_next = PC + 4 (normal)
         - Flush signals: flush_if_id = 0, flush_id_ex = 0
    
    *** NO FLUSH - everything continues normally ***
    
    Action: PC ← 108 + 4 = 112
  
  Cycle 4:
    IF:  next correctly fetched
    ID:  ADD (now in ID)
    EX:  ADDI (now in EX)
    
    *** NO DELAY - pipeline continues uninterrupted ***
  
  Result: Branch not taken has NO penalty (0 cycles)
  
  This is why speculative execution (assume not taken) is common

//==============================================================================
// 4. BRANCH DECISION LOGIC (BEQ vs BNE)
//==============================================================================

  BEQ (Branch if Equal) Instruction:
    - Opcode: 7'b1100011
    - Funct3: 3'b000
    - Condition: x[rs1] == x[rs2]
    - Implementation: Check ALU zero flag
      branch_taken = (branch & branch_type==0 & zero)
    
    Example:
      BEQ x1, x2, +12
      If x1==x2: branch_taken=1, PC←PC_EX+imm_ex
      If x1!=x2: branch_taken=0, PC←PC+4
  
  BNE (Branch if Not Equal) Instruction:
    - Opcode: 7'b1100011
    - Funct3: 3'b001
    - Condition: x[rs1] != x[rs2]
    - Implementation: Check NOT ALU zero flag
      branch_taken = (branch & branch_type==1 & ~zero)
    
    Example:
      BNE x1, x2, +12
      If x1!=x2: branch_taken=1, PC←PC_EX+imm_ex
      If x1==x2: branch_taken=0, PC←PC+4

//==============================================================================
// 5. PC CALCULATION
//==============================================================================

  PC Multiplexer Logic:
  
  Input Signals:
    - pc_ex: Current PC value in EX stage (PC where branch instruction is)
    - imm_ex: Sign-extended immediate (offset)
    - pc_normal: PC + 4 (normal sequential path)
    - branch_taken: Determined by branch condition
  
  PC Next Calculation:
    
    if (branch_taken)
        pc_next = pc_ex + imm_ex          ← Branch target
    else
        pc_next = pc_normal               ← Next sequential PC
  
  Hardware Implementation:
  
    logic [31:0] branch_target = pc_ex + imm_ex;
    assign pc_next = branch_taken ? branch_target : pc_normal;
  
  Example:
    Branch at PC=100, target offset=+12
    pc_ex = 32'h64 (100 decimal)
    imm_ex = 32'h0C (12 decimal)
    branch_target = 100 + 12 = 112
    
    If branch taken: pc_next = 112 ✓
    If not taken: pc_next = 104 (PC+4) ✓

//==============================================================================
// 6. PIPELINE FLUSH MECHANISM
//==============================================================================

  WHAT IS FLUSH?
  
  Flush = Replace instruction in a pipeline stage with NOP (no-op)
  
  NOP is encoded as all zero controls:
    - reg_write = 0 (don't write registers)
    - mem_read = 0 (don't read memory)
    - mem_write = 0 (don't write memory)
    - branch = 0 (not a branch)
    - All other controls = 0
  
  Effect: Instruction executes but does nothing (safe)

  THE TWO FLUSHES:
  
  flush_if_id signal (active when branch_taken=1):
    - Zeroes all control signals in IF/ID register
    - Effect: Speculatively fetched instruction in ID becomes NOP
    
    Pipeline Before:          Pipeline After (flush asserted):
    IF: correct next          IF: correct next
    ID: wrong path (ADDI)     ID: NOP ← Converted to NOP
    EX: BEQ                   EX: BEQ
    MEM: ...                  MEM: ...
    WB: ...                   WB: ...
  
  flush_id_ex signal (active when branch_taken=1):
    - Zeroes all control signals in ID/EX register
    - Effect: Another wrongly fetched instruction in EX becomes NOP
    
    Pipeline Before:          Pipeline After (flush asserted):
    IF: correct next          IF: correct next
    ID: NOP (already from IF flush)
    EX: wrong path (ADD) ← Flushed in PREVIOUS stage
    EX: NOP ← Also flushed here? Or...
    
    WAIT, let me reconsider the timing:
    
    Actually, at the SAME cycle that flush happens:
    - IF/ID currently holds the instruction that will enter EX next cycle
    - ID/EX currently holds the instruction in EX stage
    
    When we flush IF/ID and ID/EX simultaneously:
    - IF/ID becomes NOP (instruction doesn't enter EX after clock edge)
    - ID/EX becomes NOP (instruction currently in EX becomes NOP)
    
    But the instruction in EX was BEQ which we just decided...
    Actually no, BEQ is deciding WHILE in EX, so:
    
    Cycle 3 BEFORE clock edge:
      ID contains: ADDI (speculatively fetched)
      EX contains: BEQ (executing)
    
    Cycle 3 DURING clock edge (decision):
      branch_taken signal asserted
      flush_if_id and flush_id_ex asserted
    
    Cycle 4 AFTER clock edge:
      IF/ID register now holds NOP (flushed version of ADDI)
      ID/EX register now holds NOP (flushed version of the instruction after ADDI)
      EX: N/A (registers have updated)
      PC has been redirected to target
    
    Actually, let me think more carefully...
    
    Stages at Cycle 3 (when BEQ in EX):
    - IF: Fetching ADD (3rd wrong instruction)
    - ID: ADDI (2nd wrong instruction)
    - EX: BEQ (executing, determining branch)
    - MEM: empty
    - WB: empty
    
    Cycle 3 Decisions:
    - BEQ comparison determines zero flag
    - branch_taken = 1 (if condition met)
    - flush_if_id = 1 (flush IF/ID register)
    - flush_id_ex = 1 (flush ID/EX register)
    - PC← 112 (from branch calculation)
    
    Between Cycles 3 and 4 (clock edge):
    - All pipeline registers update with EOC (End of Cycle) values
    - IF/ID gets flushed (controls zeroed, instr=NOP encoding)
    - ID/EX gets flushed (controls zeroed, instr=NOP encoding)
    - PC gets redirected to 112
    
    Cycle 4 State (AFTER clock edge):
    - IF: Fetching from new PC (112) ← Correct path resumes!
    - ID: NOP (was ADDI, flushed by IF/ID flush)
    - EX: NOP (was ADD, flushed by ID/EX flush)
    - MEM: empty (WB was empty)
    - WB: empty
    
    Cycle 5 State:
    - IF: Next instruction after target
    - ID: Target instruction (now in ID)
    - EX: NOP (now executing as NOP, harmless)
    - MEM: empty
    - WB: empty
    
    Cycle 6 State:
    - Target instruction now in EX, executing correctly

//==============================================================================
// 7. INTEGRATION WITH STALL LOGIC
//==============================================================================

  DO FLUSH AND STALL SIGNALS CONFLICT?
  
  Stall Signals:
    - stall: Load-use hazard detected
    - if_id_write_enable = ~stall
    - insert_bubble = stall
    - Effect: PC frozen, IF/ID frozen, ID/EX bubble inserted
  
  Flush Signals:
    - flush_if_id: Active when branch taken
    - flush_id_ex: Active when branch taken
    - Effect: IF/ID controls zeroed, ID/EX controls zeroed
    - Result: Same as bubble but for different reason
  
  Conflict? NO!
  
  Reason 1: If stall is active (load hazard), branch can't be in EX
            (because instructions are stalled before reaching EX)
  
  Reason 2: If branch is in EX and branch_taken, stall shouldn't activate
            because EX has no load instruction (it has branch)
  
  Resolution Strategy:
    - Stall logic for load-use hazards (phases ID/EX)
    - Flush logic for branches (quashes wrong path)
    - No simultaneous activation expected
  
  Conservative Approach (safer):
    If branch_taken, override stall signals:
    
      control_flush = branch_taken | stall;
      if_id_write = ~control_flush;
      insert_bubble = control_flush;
    
    This ensures branch flush dominates if both somehow active

//==============================================================================
// 8. INSTRUCTION SEQUENCE EXAMPLES
//==============================================================================

  EXAMPLE 1: Unconditional Branch Taken (BEQ matching registers)
  
  Program:
    100: ADD x1, x2, x3        ← x1 = x2 + x3 = 5 + 5 = 10
    104: ADD x2, x4, x5        ← x2 = x4 + x5 = 3 + 7 = 10
    108: BEQ x1, x2, +16       ← Branch forward (x1==x2, both 10) TAKEN
    112: ADDI x3, x0, 99       ← Wrong path (should not execute)
    116: ADDI x4, x0, 88       ← Wrong path (should not execute)
    120: ADD x6, x1, x2        ← Target instruction (should execute)
    124: ADD x7, x6, x2
    128: ...
  
  Timing:
    Cycle 1: ADD @100 (IF)
    Cycle 2: ADD @104 (IF), ADD @100 (ID)
    Cycle 3: BEQ @108 (IF), ADD @104 (ID), ADD @100 (EX)
    Cycle 4: ADDI @112 (IF?/flushed), BEQ @108 (ID), ADD @104 (EX), ADD @100 (MEM)
             *** BRANCH DECISION AT EX: x1==x2, zero=1 ✓ ***
             *** branch_taken = 1 ✓ ***
             *** flush_if_id = 1 (discard ADDI) ***
             *** flush_id_ex = 1 (discard ADD @104) ***
             *** PC ← 120 ***
    Cycle 5: ADD @120 (IF, correct!), ADDI @112 (ID?/NOP), BEQ @108 (EX/NOP), ADD @104 (MEM/NOP), ADD @100 (WB)
    Cycle 6: ADD @124 (IF), ADD @120 (ID), ADDI @112 (EX/NOP), ...
    Cycle 7: ... (all wrong instructions executed as NOPs, no harm)
  
  Final State:
    x1 = 10, x2 = 10, x3 = 5, x4 = 3, x5 = 7, x6 = 20 (10+10), x7 = 30 (20+10) ✓ Correct!
  
  EXAMPLE 2: Branch Not Taken (BEQ non-matching registers)
  
  Program:
    100: ADD x1, x2, x3        ← x1 = 5
    104: ADD x2, x4, x5        ← x2 = 10
    108: BEQ x1, x2, +16       ← x1 != x2, NOT TAKEN
    112: ADDI x3, x0, 99       ← Executes (correct path)
    116: ADD x6, x1, x2        ← Executes
    120: ...
  
  Timing:
    Cycle 3: BEQ @108 (EX)
             *** COMPARISON: x1=5, x2=10, zero=0 ***
             *** branch_taken = 0 (no flush) ***
             *** pc_next = 108 + 4 = 112 (normal increment) ***
    Cycle 4: ADDI @112 (IF, correct), ...
             (No flush, no penalty, pipeline continues)
  
  Final State:
    x1 = 5, x2 = 10, x3 = 99, x6 = 15 (5+10) ✓ Correct!
    No flush penalty, normal execution
  
  EXAMPLE 3: Nested Loop (Multiple Branch Instructions)
  
  Program (simple loop):
    100: ADDI x1, x0, 3        ← x1 = 3 (counter)
    104: ADD x2, x0, x0        ← x2 = 0 (accumulator)
    108: add x2, x2, x1        ← x2 += x1 (loop body)
    112: ADDI x1, x1, -1       ← x1--
    116: BNE x1, x0, -8        ← if x1 != 0, goto 108
    120: ADD x3, x0, x2        ← x3 = x2 (after loop)
  
  Execution (simplified):
    Pass 1: x1=3, add x2+=3 (x2=3), dec x1 (x1=2), BNE taken
    Pass 2: x1=2, add x2+=2 (x2=5), dec x1 (x1=1), BNE taken
    Pass 3: x1=1, add x2+=1 (x2=6), dec x1 (x1=0), BNE not taken
    Final: x3 = x2 = 6 ✓

//==============================================================================
// 9. CONTROL SIGNAL COHERENCE IN PIPELINE
//==============================================================================

  When flush_if_id = 1:
    All control signals in IF/ID register set to 0:
      branch ← 0
      branch_type ← 0
      reg_write ← 0
      mem_read ← 0
      mem_write ← 0
      alu_src ← 0
      mem_to_reg ← 0
      Result: Instruction becomes harmless NOP
  
  When flush_id_ex = 1:
    All control signals in ID/EX register set to 0:
      (Same effect, but at different pipeline stage)
  
  Hardware Implementation for flush_if_id:
  
    always_ff @(posedge clk or posedge rst) begin
        if (rst) if_id_regs <= 0;
        else if (flush_if_id) begin
            // Zero all controls, keep instr pointer for tracing
            if_id_branch <= 1'b0;
            if_id_branch_type <= 1'b0;
            if_id_reg_write <= 1'b0;
            if_id_mem_read <= 1'b0;
            if_id_mem_write <= 1'b0;
            // (all other controls zeroed)
        end else if (if_id_write_enable) begin
            // Normal register update from decoder output
            if_id_branch <= id_branch;
            if_id_branch_type <= id_branch_type;
            if_id_reg_write <= id_reg_write;
            // (normal propagation)
        end
    end

//==============================================================================
// 10. SYNTHESIS CONSIDERATIONS
//==============================================================================

  rv32i_branch_control Module:
  
  Combinational Circuits:
    - Branch condition evaluation: 100% combinational (no latches)
    - PC multiplexer: 100% combinational
    - Flush signal generation: 100% combinational (just wire assignment)
  
  Timing Path Analysis:
    - Critical path: branch_type + zero → condition_met → branch_taken → pc_next
    - Length: 3 gates (mux/AND logic)
    - Latency: ~2 ns at 10 FO4 (very fast)
  
  Area Overhead:
    - Branch comparator: Minimal (just AND gate + mux)
    - PC adder: Already exists in most ALUs, can reuse
    - Flush logic: Just multiplexers in pipeline registers
    - Total: ~50 LUT equivalents (small)
  
  No Combinational Loops:
    ✓ No feedback paths
    ✓ PC mux has no cycles
    ✓ Flush signals combinational from branch_taken
    ✓ Safe for synthesis
  
  Clock Domain Crossings:
    - All signals synchronous (single clock)
    - No CDC issues
  
  Data Hazard Analysis:
    - Branch writes no registers (reg_write=0 for branches)
    - Branch only reads operands from ID stage (forwarding needed)
    - No new hazards introduced (forwarding unit handles still)

//==============================================================================
// 11. TEST SCENARIOS
//==============================================================================

  Unit Test Scenarios for Branch Control:
  
  Test 1: BEQ Branch Taken
    Input: branch=1, branch_type=0 (BEQ), zero=1
    Expected: branch_taken=1, flush_if_id=1, flush_id_ex=1
    Type: Positive test
  
  Test 2: BEQ Branch Not Taken
    Input: branch=1, branch_type=0, zero=0
    Expected: branch_taken=0, flush_if_id=0, flush_id_ex=0
    Type: Positive test
  
  Test 3: BNE Branch Taken
    Input: branch=1, branch_type=1, zero=0
    Expected: branch_taken=1, flush_if_id=1, flush_id_ex=1
    Type: Positive test
  
  Test 4: BNE Branch Not Taken
    Input: branch=1, branch_type=1, zero=1
    Expected: branch_taken=0, flush_if_id=0, flush_id_ex=0
    Type: Positive test
  
  Test 5: Non-Branch Instruction
    Input: branch=0, (branch_type, zero don't matter)
    Expected: branch_taken=0, flush_if_id=0, flush_id_ex=0
    Type: Edge case (no flush unless branch signal set)
  
  Test 6: PC Calculation Branch Taken
    Input: pc_ex=100, imm_ex=12, branch_taken=1
    Expected: pc_next=112
    Type: Arithmetic verification
  
  Test 7: PC Calculation Branch Not Taken
    Input: pc_normal=104, branch_taken=0
    Expected: pc_next=104
    Type: Arithmetic verification
  
  Test 8: PC Calculation Backward Branch
    Input: pc_ex=120, imm_ex=-8, branch_taken=1
    Expected: pc_next=112 (loop)
    Type: Backward branch (loop condition)
  
  Integration Test Scenarios:
  
  Test 9: Forward Branch Taken (all instructions execute correctly)
    Program: ADD → ADD → BEQ (taken) → target
    Verify: Correct register values, no execution of skipped code
  
  Test 10: Backward Branch Loop (multiple iterations)
    Program: Loop with conditional branch
    Verify: Loop counter updates, accumulator correct, exit correct
  
  Test 11: Nested Branches (branch inside branch target)
    Program: BEQ → target (another BEQ) → nested target
    Verify: Correct path through nested branches
  
  Test 12: Branch with Hazards (forwarding interaction)
    Program: ADD x1, x2, x3 → BEQ x1, x2 → target
    Verify: x1 forwarded into branch, decision correct, flush timing correct
  
  Test 13: Branch After Load (stall + branch interaction)
    Program: LW x1, ... → (stall) → ADD x2, x1, x3 → BEQ x2, x0 → target
    Verify: Stall releases, forwarding provides x1, branch decides on updated x2

//==============================================================================
// 12. COMMON MISTAKES
//==============================================================================

  Mistake 1: Not Including branch_type in decoder
  - Symptom: All branches execute as BEQ (or all as BNE)
  - Fix: Decoder must distinguish BEQ (funct3=000) vs BNE (funct3=001)
  
  Mistake 2: PC calculation using wrong values
  - Symptom: Branch target wrong address
  - Fix: Use pc_ex (current PC) not pc (global), use imm_ex not imm_id
  
  Mistake 3: Flush signals not connected to pipeline registers
  - Symptom: Speculative instructions still execute (corrupted results)
  - Fix: Ensure flush_if_id and flush_id_ex → zero all pipeline controls
  
  Mistake 4: Forgetting branch_taken=0 when branch=0
  - Symptom: Non-branch instructions flush the pipeline
  - Fix: branch_taken = branch & condition_met (AND with branch signal)
  
  Mistake 5: Conflict between stall and flush
  - Symptom: Mysterious behavior when load-use + branch occur
  - Fix: Implement priority: flush overrides stall (longer flush)
  
  Mistake 6: PC mux selecting wrong input
  - Symptom: Branch target calculated but not used
  - Fix: Ensure pc_next output is actually driving PC input

*/

endmodule
