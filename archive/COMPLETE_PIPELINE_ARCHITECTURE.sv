////////////////////////////////////////////////////////////////////////////////
//  COMPLETE_PIPELINE_ARCHITECTURE.sv
//
//  Visual Architecture Guide: Complete 5-Stage Pipeline with All Hazard Handling
//  
//  Includes: 3D pipeline diagram, signal flow, module interactions
//
//==============================================================================

/*

//==============================================================================
// 1. HIGH-LEVEL PIPELINE BLOCK DIAGRAM
//==============================================================================

┌────────────────────────────────────────────────────────────────────────────┐
│                    5-STAGE RISC-V PIPELINE WITH HAZARDS                    │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │ INSTRUCTION MEMORY (I-CACHE)                                       │  │
│  │   instr_mem[256] x 32-bit instructions                            │  │
│  └──────────────────────┬──────────────────────────────────────────────┘  │
│                         │ instr = instr_mem[pc[9:2]]                      │
│                         ▼                                                  │
│  ┌──────────────────────────────────────────────────────────┐             │
│  │ IF (Instruction Fetch)                                  │             │
│  │ ┌────────────────────────────────────────────────────┐  │             │
│  │ │ PC: Program Counter (32-bit)                       │  │             │
│  │ │ PC ← pc_next when pc_write_enable=1 (STALL ctrl) │  │             │
│  │ │ PC ← branch_pc_next when branch_taken=1 (BRANCH) │  │             │
│  │ └────────────────────────────────────────────────────┘  │             │
│  │ ┌────────────────────────────────────────────────────┐  │             │
│  │ │ Instruction Fetch from instr_mem[pc/4]           │  │             │
│  │ │ Output: instr (24-bit immediate part later)       │  │             │
│  │ └────────────────────────────────────────────────────┘  │             │
│  └──────────────────┬───────────────────────────────────────┘             │
│                     │                                                      │
│                     ▼ IF/ID Pipeline Register (32-bit instruction + 32-bit PC+4)
│                     │ (Controlled by if_id_write_enable from stall;       │
│                     │  Flushed by flush_if_id from branch)              │
│                     │                                                     │
│  ┌──────────────────────────────────────────────────────┐                │
│  │ ID (Instruction Decode)                             │                │
│  │ ┌────────────────────────────────────────────────┐  │                │
│  │ │ DECODER: instr → controls + immediate         │  │                │
│  │ │ • Outputs: branch, branch_type, alu_op, ...   │  │                │
│  │ │ • NEW: branch_type (0=BEQ, 1=BNE)            │  │                │
│  │ └────────────────────────────────────────────────┘  │                │
│  │ ┌────────────────────────────────────────────────┐  │                │
│  │ │ REGISTER FILE (2-Read, 1-Write)                │  │                │
│  │ │ rs1_data ← regs[rs1] (after forwarding MUX)    │  │                │
│  │ │ rs2_data ← regs[rs2] (after forwarding MUX)    │  │                │
│  │ └────────────────────────────────────────────────┘  │                │
│  │ ┌────────────────────────────────────────────────┐  │                │
│  │ │ HAZARD DETECTION (Load-Use)                    │  │                │
│  │ │ if (ex_memread && ex_rd == id_rs1/rs2):       │  │                │
│  │ │     stall=1 → pc_write=0, if_id_write=0      │  │                │
│  │ │     insert_bubble=1 (→ ID/EX bubble)          │  │                │
│  │ └────────────────────────────────────────────────┘  │                │
│  └──────────────────┬───────────────────────────────────┘                │
│                     │                                                     │
│                     ▼ ID/EX Pipeline Register (All controls + operands)
│                     │ (Controlled by insert_bubble from stall;           │
│                     │  Flushed by flush_id_ex from branch)             │
│                     │                                                    │
│  ┌──────────────────────────────────────────────────────┐               │
│  │ EX (Execute)                                        │               │
│  │ ┌────────────────────────────────────────────────┐  │               │
│  │ │ FORWARDING UNIT                                │  │               │
│  │ │ • Checks: wb_rd vs ex_rs1/rs2                 │  │               │
│  │ │ • Priority: MEM (most recent) > WB            │  │               │
│  │ │ • Outputs: forward_a, forward_b (2'b mux)    │  │               │
│  │ └────────────────────────────────────────────────┘  │               │
│  │ ┌────────────────────────────────────────────────┐  │               │
│  │ │ ALU OPERAND MUXES                              │  │               │
│  │ │ alu_in1 ← rs1_data or forwarded_mem or        │  │               │
│  │ │           forwarded_wb                         │  │               │
│  │ │ alu_in2 ← rs2_data or immediate or forwarded  │  │               │
│  │ └────────────────────────────────────────────────┘  │               │
│  │ ┌────────────────────────────────────────────────┐  │               │
│  │ │ ALU (Arithmetic / Logic Unit)                  │  │               │
│  │ │ alu_result = ALU_OP(alu_in1, alu_in2)         │  │               │
│  │ │ zero = (alu_result == 0)                       │  │               │
│  │ │ Supports: ADD, SUB, AND, OR, XOR, SLT, etc.   │  │               │
│  │ └────────────────────────────────────────────────┘  │               │
│  │ ┌────────────────────────────────────────────────┐  │               │
│  │ │ ★ BRANCH CONTROL (NEW - Phase 4)              │  │               │
│  │ │ Inputs: branch, branch_type, zero,            │  │               │
│  │ │          pc_ex, imm_ex, pc_normal             │  │               │
│  │ │ Logic:                                         │  │               │
│  │ │  • BEQ: branch_taken = (branch & zero)        │  │               │
│  │ │  • BNE: branch_taken = (branch & ~zero)       │  │               │
│  │ │  • pc_next = branch_taken ?                   │  │               │
│  │ │             (pc_ex + imm_ex) : pc_normal      │  │               │
│  │ │ Outputs:                                       │  │               │
│  │ │  • branch_taken → PC mux (redirect)           │  │               │
│  │ │  • flush_if_id → IF/ID (insert NOP in ID)    │  │               │
│  │ │  • flush_id_ex → ID/EX (insert NOP in EX)    │  │               │
│  │ │  • pc_next → PC mux                           │  │               │
│  │ └────────────────────────────────────────────────┘  │               │
│  └──────────────────┬───────────────────────────────────┘               │
│                     │                                                    │
│                     ▼ EX/MEM Pipeline Register (ALU result + mem data)
│                     │                                                   │
│  ┌──────────────────────────────────────────────────────┐              │
│  │ MEM (Memory Access)                                 │              │
│  │ ┌────────────────────────────────────────────────┐  │              │
│  │ │ DATA MEMORY (D-CACHE)                          │  │              │
│  │ │ data_mem[256] x 32-bit                         │  │              │
│  │ │ Read:  mem_read_data = mem_read ? data_mem[..] │  │              │
│  │ │ Write: if (mem_write) data_mem[alu_result]←   │  │              │
│  │ │        rs2_data                                 │  │              │
│  │ └────────────────────────────────────────────────┘  │              │
│  └──────────────────┬───────────────────────────────────┘              │
│                     │                                                   │
│                     ▼ MEM/WB Pipeline Register (ALU result or mem data)
│                     │                                                  │
│  ┌──────────────────────────────────────────────────────┐             │
│  │ WB (Write Back)                                     │             │
│  │ ┌────────────────────────────────────────────────┐  │             │
│  │ │ REGISTER FILE WRITE                            │  │             │
│  │ │ write_data ← mem_to_reg ? mem_data : alu_result│ │             │
│  │ │ if (reg_write) regs[rd] ← write_data           │ │             │
│  │ │ Note: x0 never written (hardwired 0)           │ │             │
│  │ └────────────────────────────────────────────────┘  │             │
│  └──────────────────┬────────────────────────────────────┘             │
│                     │                                                  │
│                     ▼ Feedback to ID stage (operand forwarding)
│                                                                        │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ PC MULTIPLEXER (At IF stage beginning)                        │  │
│  │                                                                │  │
│  │  PC Generation Logic:                                         │  │
│  │  ┌─────────────────────────────────┐                         │  │
│  │  │ branch_taken (from EX)?         │                         │  │
│  │  │   YES → branch_pc_next (target) │                         │  │
│  │  │   NO  → pc_write_enable?        │                         │  │
│  │  │         YES → pc_plus4 (normal) │                         │  │
│  │  │         NO  → pc (frozen/stall) │                         │  │
│  │  └─────────────────────────────────┘                         │  │
│  │                                                                │  │
│  │  Priority: Branch > Stall > Hold                             │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                       │
└───────────────────────────────────────────────────────────────────────┘

//==============================================================================
// 2. SIGNAL FLOW DIAGRAM (Data Hazard Resolution)
//==============================================================================

Forwarding Path (ALU-to-ALU dependencies):

  ┌─────────────────────────────────┐
  │ WB Stage                        │
  │ write_data (updated register)   │
  └──────────┬──────────────────────┘
             │
             │ wb_write_data
             │
             ▼ ┌─────────────────────────────────┐
               │ EX Stage Forwarding Mux         │
               │                                 │
               │ if (forward_a == 2'b01):       │
               │    alu_in1 ← wb_write_data     │
               │                                 │
               │ if (forward_a == 2'b10):       │
               │    alu_in1 ← mem_alu_result    │
               │ (MEM stage result - priority)   │
               └─────────────────────────────────┘
                      │
                      ▼ ex_alu_in1 → ALU

Stall Path (Load-Use hazard):

  ┌─────────────────────────────────┐
  │ EX Stage                        │
  │ ex_memread (is this a load?)    │
  │ ex_rd (destination register)    │
  └──────────┬──────────────────────┘
             │
             │ Hazard Detector Input
             │
             ▼ ┌─────────────────────────────────┐
               │ Hazard Detection                │
               │                                 │
               │ if (ex_memread &&              │
               │     ex_rd == id_rs1/rs2):      │
               │    load_use_hazard = 1        │
               │                                 │
               │ Outputs:                        │
               │  pc_write_enable ← 0 (freeze)  │
               │  if_id_write ← 0 (freeze)      │
               │  insert_bubble ← 1 (NOP to EX) │
               └─────────────────────────────────┘
                       │
                       │ Control Signals
                       │
                       ▼ PC/IF-ID frozen, ID/EX becomes NOP
                       ▼ ID stage stalled for 1 cycle

Branch Flush Path (Control Hazard):

  ┌─────────────────────────────────┐
  │ EX Stage                        │
  │ branch, branch_type (from ID)   │
  │ zero (ALU comparison result)    │
  └──────────┬──────────────────────┘
             │
             │ Branch Control Inputs
             │
             ▼ ┌─────────────────────────────────┐
               │ Branch Decision Logic           │
               │                                 │
               │ BEQ condition:                  │
               │  condition = zero              │
               │                                 │
               │ BNE condition:                  │
               │  condition = ~zero             │
               │                                 │
               │ branch_taken = branch &        │
               │                condition       │
               │                                 │
               │ PC Calculation:                 │
               │  pc_next = branch_taken ?      │
               │    (pc_ex + imm_ex) : pc_+4    │
               │                                 │
               │ Flush Signals:                  │
               │  flush_if_id ← branch_taken    │
               │  flush_id_ex ← branch_taken    │
               └─────────────────────────────────┘
                       │
                       │ Control Signals
                       │
                       ├─→ pc_next to PC Mux
                       │   (redirects to target)
                       │
                       ├─→ flush_if_id
                       │   (zeros controls in IF/ID register)
                       │   (inserts NOP in ID stage next cycle)
                       │
                       └─→ flush_id_ex
                           (zeros controls in ID/EX register)
                           (inserts NOP in EX stage next cycle)

//==============================================================================
// 3. PIPELINE TIMING: 3-CYCLE BRANCH EXECUTION
//==============================================================================

Time    | IF        | ID        | EX         | MEM     | WB
--------|-----------|-----------|------------|---------|-------
C1      | BEQ @08   | -         | -          | -       | -
        | PC ← 12   |           |            |         |
--------|-----------|-----------|------------|---------|-------
C2      | WRONG @12 | BEQ @08   | -          | -       | -
        | (assumes  |  (decode) |            |         |
        |  not tk)  | PC_plus4  |            |         |
        |           |           |            |         |
        | PC ← 16   |           |            |         |
--------|-----------|-----------|------------|---------|-------
C3      | WRONG @16 | WRONG @12 | BEQ @08    | -       | -
        | (assumes  | (decode)  | (execute)  |         |
        |  not tk)  |           | x1 == x2?  |         |
        |           |           | YES → TK   |         |
        |           |           | pc_next=20 |         |
        |           |           |            |         |
        | START BUT | START BUT | FLUSH      |         |
        | WILL BE   | WILL BE   | ASSERTS:   |         |
        | REPLACED  | REPLACED  | flush_if_id│         |
        | PC ← 16   |           | flush_id_ex│         |
        | (wrong!)  |           |            |         |
--------|-----------|-----------|------------|---------|-------
**      |           |           | Clock Edge / Latch  |
**      | Registers | PC redirected,  IF/ID and ID/EX |
**      | Update:   | Instructions become NOPs         |
--------|-----------|-----------|------------|---------|-------
C4      | CORRECT   | NOP ←flush| NOP ←flush | -       | -
        | @20       | from IF   | from ID    |         |
        | (from     | /ID       | /EX        |         |
        | redirected|           |            |         |
        | PC)       |           |            |         |
        |           |           |            |         |
        | PC ← 24   |           |            |         |
--------|-----------|-----------|------------|---------|-------
C5      | NEXT @24  | CORRECT   | NOP pass   | NOP     | -
        |           | @20       | through    |         |
        |           | (decode)  | (harmless) |         |
        |           |           |            |         |
        | PC ← 28   |           |            |         |
--------|-----------|-----------|------------|---------|-------
C6      | ...       | NEXT @24  | CORRECT    | NOP     | NOP
        |           |           | @20        |         |
        |           |           | (execute)  |         |
        |           |           | ← TARGET   |         |
        |           |           | EXECUTES   |         |
--------|-----------|-----------|------------|---------|-------

Summary:
  - Branch decision: Cycle 3 (EX stage)
  - Flush assertion: Cycle 3 (combinational)
  - Flush effect: Cycle 4 (pipeline registers updated)
  - Target execution: Cycle 6 (target in EX)
  - Penalty: 3 cycles from branch IF to target EX

//==============================================================================
// 4. MODULE INSTANTIATION HIERARCHY
//==============================================================================

rv32i_pipe5_with_branches
  ├── rv32i_decoder (1 instance in ID)
  │   └── Decodes instr → control + immediate
  │       Outputs: branch, branch_type (new), ...
  │
  ├── rv32i_regfile (1 instance in ID, 1 in WB)
  │   └── 32x32 registers, 2R/1W
  │       All forwarding/stall interaction here
  │
  ├── rv32i_hazard_detection (1 instance in ID/EX boundary)
  │   └── Detects load-use hazards
  │       Outputs: stall, pc_write_enable, if_id_write_enable
  │
  ├── rv32i_forwarding_unit (1 instance in EX)
  │   └── Generates forwarding controls
  │       Outputs: forward_a, forward_b (2-bit each)
  │
  ├── rv32i_alu (1 instance in EX)
  │   └── 32-bit ALU (8 operations)
  │       Outputs: result, zero flag
  │
  └── ★ rv32i_branch_control (1 instance in EX) [NEW Phase 4]
      └── Branch decision and PC redirection
          Outputs: branch_taken, pc_next, flush_if_id, flush_id_ex

Pipeline Registers (No separate modules, just groups of flip-flops):
  - if_id register (64-bit: instr + pc_plus4)
  - id_ex register (150+ bits: all controls + operands)
  - ex_mem register (100+ bits: alu_result + mem_data)
  - mem_wb register (80+ bits: alu_result, mem_data, rd)

Memories (External, not modules):
  - instr_mem[256] x 32: Instruction cache
  - data_mem[256] x 32: Data cache

Total Modules: 7 + 4 registers + 2 memories

//==============================================================================
// 5. CONTROL SIGNAL FLOW TABLE
//==============================================================================

Stage    | Control Signals        | Source        | Destination
---------|------------------------|---------------|-----------------------
ID       | branch=1, branch_type  | decoder       | ID/EX register
         | (latched there)        |               |
---------|------------------------|---------------|-----------------------
EX       | id_ex_branch           | ID/EX register| branch_control
         | id_ex_branch_type      |               | branch_control
         | zero flag              | ALU          | branch_control
---------|------------------------|---------------|-----------------------
EX→PC    | branch_taken           | branch_control| PC mux
         | pc_next                | branch_control| PC mux
---------|------------------------|---------------|-----------------------
EX→IF/ID | flush_if_id            | branch_control| IF/ID register
         |                        |               | (zeros all controls)
---------|------------------------|---------------|-----------------------
EX→ID/EX | flush_id_ex            | branch_control| ID/EX register
         |                        |               | (zeros all controls)
---------|------------------------|---------------|-----------------------
ID→PC    | pc_write_enable        | hazard_detect | PC multiplexer
         | if_id_write_enable     | hazard_detect | IF/ID register
         | insert_bubble          | hazard_detect | ID/EX register
---------|------------------------|---------------|-----------------------

Priority When Multiple Signals Active:
  1. branch_taken (highest) - PC redirect, flush both stages
  2. pc_write_enable (medium) - Allow normal increment or stall
  3. flush signals (high) - Zero controls in pipeline stages
  4. stall signals (medium) - Freeze early stages

//==============================================================================
// 6. TESTING VERIFICATION POINTS
//==============================================================================

Branch_taken Signal:
  ✓ Assert when (branch = 1) AND condition_met
  ✓ De-assert when NOT branch
  ✓ BEQ case: condition_met = zero
  ✓ BNE case: condition_met = ~zero

PC_next Signal:
  ✓ branch_taken=1: pc_next = pc_ex + imm_ex (target)
  ✓ branch_taken=0: pc_next = pc_plus4 (sequential)
  ✓ Arithmetic: No overflow truncation until next cycle

Flush_if_id, flush_id_ex Signals:
  ✓ Both equal to branch_taken (synchronized)
  ✓ Both active for exactly 1 cycle (flush pulse)
  ✓ All controls zeroed after flush (NOP inserted)

Register File State:
  ✓ No speculatively corrupted values (flush prevents writes)
  ✓ Only committed results (MEM/WB) written
  ✓ x0 always 0 (unwritable, no side effects from flush)

Pipeline Stages After Flush:
  ✓ IF: Resumes fetching from new PC
  ✓ ID: NOP (flushed instruction now NOP)
  ✓ EX: NOP (another flushed instruction)
  ✓ MEM/WB: Continue normally (before flush point)

*/

endmodule
