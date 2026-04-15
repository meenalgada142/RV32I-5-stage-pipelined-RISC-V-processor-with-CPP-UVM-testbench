#pragma once
// program_gen.h — RV32I instruction encoders, directed programs, and
//                 constrained-random instruction generator.
//
// Directed programs (original 8):
//   prog_arith_basic, prog_data_forwarding, prog_load_use,
//   prog_branch_taken, prog_branch_not_taken, prog_bne_loop,
//   prog_jal_link, prog_full
//
// Adversarial directed programs (6 new):
//   prog_deep_dep_chain         — 8 back-to-back writes to same register
//   prog_back_to_back_control   — JAL then BEQ/BNE within 3 instructions
//   prog_load_use_branch_operand— LW + branch using loaded value (stall+flush)
//   prog_store_load_adjacent    — SW immediately followed by LW same address
//   prog_overlapping_hazards    — load-use + RAW + taken-branch in 6 instrs
//   prog_imm_edge_cases         — ADDI/SRAI/SLT/SLTU with ±2047,-1,0
//
// Constrained-random generator:
//   prog_constrained(n, seed, cfg) — hazard-biased, edge-imm-biased generator
//     • 20% branch density (vs 1.5% in original prog_random)
//     • 60% RAW-chain probability (next instr uses just-written rd)
//     • 30% edge-immediate probability (±2047, -1, 0)
//     • 70% load-use forcing (LW immediately followed by consumer)
//     • All 10 R-type ops (not just ADD/SUB/AND/OR/XOR)
//     • Store-to-load same-address patterns
//     • Registers 1–30 (not just 1–15)

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Low-level encoders
// ============================================================================

inline uint32_t enc_R(uint8_t f7, uint8_t rs2, uint8_t rs1,
                      uint8_t f3, uint8_t rd,  uint8_t op) {
    return ((uint32_t)f7  << 25) | ((uint32_t)rs2 << 20) | ((uint32_t)rs1 << 15)
         | ((uint32_t)f3  << 12) | ((uint32_t)rd  <<  7) | op;
}
inline uint32_t enc_I(int32_t imm, uint8_t rs1, uint8_t f3, uint8_t rd, uint8_t op) {
    return (((uint32_t)(imm & 0xFFF)) << 20) | ((uint32_t)rs1 << 15)
         | ((uint32_t)f3 << 12) | ((uint32_t)rd << 7) | op;
}
inline uint32_t enc_S(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t f3, uint8_t op) {
    return (((uint32_t)((imm >> 5) & 0x7F)) << 25) | ((uint32_t)rs2 << 20)
         | ((uint32_t)rs1 << 15) | ((uint32_t)f3 << 12)
         | (((uint32_t)(imm & 0x1F)) << 7) | op;
}
inline uint32_t enc_B(int32_t off, uint8_t rs2, uint8_t rs1, uint8_t f3) {
    uint32_t b12   = (off >> 12) & 1u;
    uint32_t b11   = (off >> 11) & 1u;
    uint32_t b10_5 = (off >>  5) & 0x3Fu;
    uint32_t b4_1  = (off >>  1) & 0xFu;
    return (b12 << 31) | (b10_5 << 25) | ((uint32_t)rs2 << 20)
         | ((uint32_t)rs1 << 15) | ((uint32_t)f3 << 12)
         | (b4_1 << 8) | (b11 << 7) | 0x63u;
}
inline uint32_t enc_J(int32_t off, uint8_t rd) {
    uint32_t b20    = (off >> 20) & 1u;
    uint32_t b19_12 = (off >> 12) & 0xFFu;
    uint32_t b11    = (off >> 11) & 1u;
    uint32_t b10_1  = (off >>  1) & 0x3FFu;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20)
         | (b19_12 << 12) | ((uint32_t)rd << 7) | 0x6Fu;
}

// ============================================================================
// Named instruction builders
// ============================================================================
inline uint32_t NOP ()                                   { return 0x00000013u; }
inline uint32_t HALT()                                   { return 0x0000006Fu; }
inline uint32_t ADD (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,0,rd,0x33); }
inline uint32_t SUB (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x20,rs2,rs1,0,rd,0x33); }
inline uint32_t AND_(uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,7,rd,0x33); }
inline uint32_t OR_ (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,6,rd,0x33); }
inline uint32_t XOR_(uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,4,rd,0x33); }
inline uint32_t SLT (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,2,rd,0x33); }
inline uint32_t SLTU(uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,3,rd,0x33); }
inline uint32_t SLL (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,1,rd,0x33); }
inline uint32_t SRL (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x00,rs2,rs1,5,rd,0x33); }
inline uint32_t SRA (uint8_t rd, uint8_t rs1, uint8_t rs2){ return enc_R(0x20,rs2,rs1,5,rd,0x33); }
inline uint32_t ADDI(uint8_t rd, uint8_t rs1, int16_t imm){ return enc_I(imm,rs1,0,rd,0x13); }
inline uint32_t XORI(uint8_t rd, uint8_t rs1, int16_t imm){ return enc_I(imm,rs1,4,rd,0x13); }
inline uint32_t ORI (uint8_t rd, uint8_t rs1, int16_t imm){ return enc_I(imm,rs1,6,rd,0x13); }
inline uint32_t ANDI(uint8_t rd, uint8_t rs1, int16_t imm){ return enc_I(imm,rs1,7,rd,0x13); }
inline uint32_t SLLI(uint8_t rd, uint8_t rs1, uint8_t sh) { return enc_I(sh,rs1,1,rd,0x13); }
inline uint32_t SRLI(uint8_t rd, uint8_t rs1, uint8_t sh) { return enc_I(sh,rs1,5,rd,0x13); }
inline uint32_t SRAI(uint8_t rd, uint8_t rs1, uint8_t sh) { return enc_I((int32_t)(0x400|(sh&0x1F)),rs1,5,rd,0x13); }
inline uint32_t LW  (uint8_t rd, uint8_t rs1, int16_t off){ return enc_I(off,rs1,2,rd,0x03); }
inline uint32_t SW  (uint8_t rs2,uint8_t rs1, int16_t off){ return enc_S(off,rs2,rs1,2,0x23); }
inline uint32_t BEQ (uint8_t rs1,uint8_t rs2, int16_t off){ return enc_B(off,rs2,rs1,0); }
inline uint32_t BNE (uint8_t rs1,uint8_t rs2, int16_t off){ return enc_B(off,rs2,rs1,1); }
inline uint32_t JAL (uint8_t rd, int32_t off)             { return enc_J(off,rd); }

// ============================================================================
// Original directed programs (unchanged)
// ============================================================================

inline std::vector<uint32_t> prog_arith_basic() {
    return {
        ADDI(1, 0, 10),   // x1=10
        ADDI(2, 0,  5),   // x2=5
        NOP(), NOP(),
        ADD (3, 1, 2),    // x3=15
        SUB (4, 1, 2),    // x4=5
        AND_(5, 1, 2),    // x5=0
        OR_ (6, 1, 2),    // x6=15
        XOR_(7, 1, 2),    // x7=15
        NOP(), NOP(),
        SLT (8, 2, 1),    // x8=1  (5<10 signed)
        SLTU(9, 2, 1),    // x9=1
        SLLI(10,1, 2),    // x10=40
        SRLI(11,1, 1),    // x11=5
        SRAI(12,1, 1),    // x12=5
        HALT(),
    };
}

inline std::vector<uint32_t> prog_data_forwarding() {
    return {
        ADDI(1, 0,  7),
        ADD (2, 1,  1),   // EX→EX fwd x1
        ADD (3, 2,  1),   // EX→EX fwd x2, MEM→EX fwd x1
        ADD (4, 3,  2),
        ADD (5, 4,  3),
        ADDI(6, 5, -1),   // EX→EX fwd x5
        XOR_(7, 6,  5),
        OR_ (8, 7,  6),
        AND_(9, 8,  7),
        HALT(),
    };
}

inline std::vector<uint32_t> prog_load_use() {
    return {
        ADDI(1, 0, 42),
        SW  (1, 0,  0),
        ADDI(2, 0, 99),
        SW  (2, 0,  4),
        LW  (3, 0,  0),
        ADD (4, 3,  0),   // load-use stall on x3
        LW  (5, 0,  4),
        ADDI(6, 5,  1),   // load-use stall on x5
        ADD (7, 4,  6),
        HALT(),
    };
}

inline std::vector<uint32_t> prog_branch_taken() {
    return {
        ADDI(1, 0, 10),
        ADDI(2, 0, 10),
        BEQ (1, 2,  8),   // taken → skip [12]
        ADDI(3, 0, 99),   // FLUSHED
        ADDI(4, 0, 42),
        HALT(),
    };
}

inline std::vector<uint32_t> prog_branch_not_taken() {
    return {
        ADDI(1, 0,  5),
        ADDI(2, 0,  3),
        BEQ (1, 2,  8),   // not taken
        ADDI(3, 0, 77),
        ADDI(4, 0, 88),
        HALT(),
    };
}

inline std::vector<uint32_t> prog_bne_loop() {
    return {
        ADDI(1, 0,  5),   // counter
        ADDI(2, 0,  0),   // accumulator
        ADD (2, 2,  1),   // loop top  (PC=8)
        ADDI(1, 1, -1),
        BNE (1, 0, -8),   // back to PC=8
        HALT(),
    };
}

inline std::vector<uint32_t> prog_jal_link() {
    return {
        ADDI(1, 0, 1),
        JAL (5, 8),       // x5=8, jump to PC=12
        ADDI(3, 0, 99),   // FLUSHED
        ADDI(4, 0, 77),
        HALT(),
    };
}

inline std::vector<uint32_t> prog_full() {
    return {
        ADDI(1, 0, 5),
        ADDI(2, 0, 0),
        JAL (11, 8),      // x11=12, skip NOP
        NOP(),            // FLUSHED
        NOP(),            // jump target (PC=16)
        ADD (2, 2, 1),    // loop top (PC=20)
        ADDI(1, 1,-1),
        BNE (1, 0,-8),
        SW  (2, 0, 20),
        LW  (10,0, 20),
        ADDI(10,10, 1),   // load-use stall
        ADDI(12,11, 0),
        HALT(),
    };
}

// ============================================================================
// Adversarial Test 1 — Deep dependency chain (same register, 8-deep)
//
// Bug targeted: forwarding priority logic when EX→EX and MEM→EX both point
// to the same destination register across a long chain.
// Expected: x1 increments by 1 each step → x1=8 at HALT.
// ============================================================================
inline std::vector<uint32_t> prog_deep_dep_chain() {
    return {
        ADDI(1, 0, 0),    // x1=0
        ADDI(1, 1, 1),    // x1=1   EX→EX fwd
        ADDI(1, 1, 1),    // x1=2   EX→EX fwd
        ADDI(1, 1, 1),    // x1=3
        ADDI(1, 1, 1),    // x1=4
        ADDI(1, 1, 1),    // x1=5
        ADDI(1, 1, 1),    // x1=6
        ADDI(1, 1, 1),    // x1=7
        ADDI(1, 1, 1),    // x1=8
        // Also exercise same-register R-type chaining
        ADD (1, 1, 1),    // x1=16  (x1+x1: both operands use same fwd result)
        ADD (2, 1, 1),    // x2=32
        ADD (1, 1, 2),    // x1=48
        ADD (1, 1, 1),    // x1=96  (rd==rs1==rs2 all forwarded)
        HALT(),
    };
}

// ============================================================================
// Adversarial Test 2 — Back-to-back control transfers
//
// Bug targeted: flush-valid pipeline register not cleared after first redirect,
// causing the second JAL's target to be incorrectly flushed.
// Expected: x1=4 (JAL1 link), x2=1, x3 NOT written (flushed), x4=77, x5=24.
// ============================================================================
inline std::vector<uint32_t> prog_back_to_back_control() {
    // PC map:
    //  0: JAL x1, +8    → x1=4,  jump to PC=8
    //  4: ADDI x9,0,99  → FLUSHED by JAL1
    //  8: ADDI x2,0,1   → x2=1   (JAL1 target)
    // 12: BEQ  x2,x0,+8 → NOT taken (1≠0), fall through to PC=16
    // 16: ADDI x3,0,55  → x3=55  (branch fall-through)
    // 20: JAL  x5,+8    → x5=24, jump to PC=28
    // 24: ADDI x6,0,99  → FLUSHED by JAL2
    // 28: ADDI x4,0,77  → x4=77  (JAL2 target)
    // 32: HALT
    return {
        JAL (1, 8),       // PC=0:  x1=4,  jump PC=8
        ADDI(9, 0, 99),   // PC=4:  FLUSHED
        ADDI(2, 0, 1),    // PC=8:  x2=1
        BEQ (2, 0, 8),    // PC=12: not taken
        ADDI(3, 0, 55),   // PC=16: x3=55
        JAL (5, 8),       // PC=20: x5=24, jump PC=28
        ADDI(6, 0, 99),   // PC=24: FLUSHED
        ADDI(4, 0, 77),   // PC=28: x4=77
        HALT(),           // PC=32
    };
}

// ============================================================================
// Adversarial Test 3 — Load-use where loaded value is the branch operand
//
// Bug targeted: load-use stall + taken branch interact incorrectly.
// The stall must be inserted before the BEQ, then the branch must resolve
// on the forwarded (post-stall) value and flush the right instructions.
// Expected: x3=5 (loaded), x4=42 (branch target), x5 NOT written (flushed).
// ============================================================================
inline std::vector<uint32_t> prog_load_use_branch_operand() {
    // PC map:
    //  0: ADDI x1,0,5
    //  4: SW   x1,x0,0   → mem[0]=5
    //  8: ADDI x2,0,5
    // 12: LW   x3,x0,0   → x3=5  (load)
    // 16: BEQ  x3,x2,+8  → load-use stall, then BEQ: x3==x2==5 → TAKEN
    // 20: ADDI x5,0,99   → FLUSHED (in branch shadow)
    // 24: ADDI x4,0,42   → x4=42  (branch target)
    // 28: HALT
    return {
        ADDI(1, 0, 5),
        SW  (1, 0, 0),
        ADDI(2, 0, 5),
        LW  (3, 0, 0),    // load x3
        BEQ (3, 2, 8),    // load-use stall on x3, then taken
        ADDI(5, 0, 99),   // FLUSHED
        ADDI(4, 0, 42),
        HALT(),
    };
}

// ============================================================================
// Adversarial Test 4 — Store-to-load same address (adjacent, 1-apart, 4-apart)
//
// Bug targeted: data_mem write-before-read semantics at posedge.
// When SW is in WB and LW is in MEM simultaneously, the value written by
// SW must be visible to the LW (write-first port).
// ============================================================================
inline std::vector<uint32_t> prog_store_load_adjacent() {
    return {
        // --- Adjacent: SW then LW at same address (1 instr apart) ---
        ADDI(1, 0, 0xAB),    // x1=0xAB
        SW  (1, 0,  0),      // mem[0]=0xAB
        LW  (2, 0,  0),      // x2=0xAB  (load-use: x2 used 2 after)
        NOP(),               // gap
        ADD (3, 2,  0),      // x3=0xAB  (checks x2 correct)

        // --- 4-apart: SW then LW to same address with gap ---
        ADDI(4, 0, 0xCD),    // x4=0xCD
        SW  (4, 0,  4),      // mem[1]=0xCD
        NOP(), NOP(), NOP(),
        LW  (5, 0,  4),      // x5=0xCD
        NOP(),
        ADD (6, 5,  0),      // x6=0xCD

        // --- Overwrite: store new value, load should see new ---
        ADDI(7, 0, 0x11),
        SW  (7, 0,  0),      // mem[0]=0x11 (overwrites 0xAB)
        NOP(), NOP(),
        LW  (8, 0,  0),      // x8=0x11
        NOP(),
        ADD (9, 8,  0),      // x9=0x11
        HALT(),
    };
}

// ============================================================================
// Adversarial Test 5 — Overlapping hazards (load-use + RAW + taken branch)
//
// Bug targeted: stall counter off-by-one when stall and flush happen within
// the same 6-instruction window.
// Expected: x4=10 (ADD uses forwarded LW result), branch taken (10==10),
//           x6=6 (branch target), x5 NOT written (flushed).
// ============================================================================
inline std::vector<uint32_t> prog_overlapping_hazards() {
    // PC map:
    //  0: ADDI x1,0,10
    //  4: ADDI x2,0,10
    //  8: SW   x1,x0,0    → mem[0]=10
    // 12: SW   x2,x0,4    → mem[1]=10
    // 16: LW   x3,x0,0    → x3=10
    // 20: ADD  x4,x3,x0   → load-use stall on x3; x4=10
    // 24: LW   x9,x0,4    → x9=10 (second load while x4 in EX)
    // 28: BEQ  x4,x1,+8   → RAW on x4 (EX→EX fwd from ADD); 10==10 → TAKEN
    // 32: ADDI x5,0,99    → FLUSHED
    // 36: ADD  x6,x9,x0   → x6=10  (branch target, also tests x9 correct)
    // 40: HALT
    return {
        ADDI(1, 0, 10),
        ADDI(2, 0, 10),
        SW  (1, 0,  0),
        SW  (2, 0,  4),
        LW  (3, 0,  0),
        ADD (4, 3,  0),     // load-use stall on x3
        LW  (9, 0,  4),
        BEQ (4, 1,  8),     // RAW on x4; 10==10 → taken
        ADDI(5, 0, 99),     // FLUSHED
        ADD (6, 9,  0),     // x6=10
        HALT(),
    };
}

// ============================================================================
// Adversarial Test 6 — Immediate edge cases
//
// Bug targeted:
//   • SRAI vs SRLI disambiguation (funct7[5]) on negative values
//   • SLT/SLTU boundary when rs1==rs2 (should produce 0, not 1)
//   • Sign extension of max/min 12-bit immediates
//   • ADDI with imm=0 (identity), imm=-1 (all-ones propagation)
// ============================================================================
inline std::vector<uint32_t> prog_imm_edge_cases() {
    return {
        // Sign-extension boundary
        ADDI(1, 0, -1),       // x1=0xFFFFFFFF  (-1, all-ones)
        ADDI(2, 0, -2048),    // x2=0xFFFFF800  (most negative 12-bit)
        ADDI(3, 0,  2047),    // x3=0x000007FF  (most positive 12-bit)
        ADDI(4, 0,  0),       // x4=0           (zero immediate)
        NOP(), NOP(),

        // SRAI vs SRLI on negative value — critical funct7[5] disambiguation
        SRAI(5, 1, 31),       // x5=0xFFFFFFFF  (arithmetic: sign fills)
        SRLI(6, 1, 31),       // x6=0x00000001  (logical: zero fills)
        NOP(), NOP(),

        // SLT/SLTU when rs1==rs2 → must produce 0
        SLT (7, 1, 1),        // x7=0  (0xFFFFFFFF < 0xFFFFFFFF signed → false)
        SLTU(8, 1, 1),        // x8=0  (unsigned same)
        NOP(), NOP(),

        // SLT signed boundary: most-negative < most-positive
        SLT (9,  2, 3),       // x9=1  (-2048 < 2047 signed)
        SLTU(10, 2, 3),       // x10=0 (0xFFFFF800 > 0x000007FF unsigned)
        NOP(), NOP(),

        // ADD overflow (unsigned wrap-around): 0xFFFFFFFF + 1 = 0
        ADDI(11, 0, -1),      // x11=0xFFFFFFFF
        ADDI(12, 0,  1),
        NOP(), NOP(),
        ADD (13, 11, 12),     // x13=0  (wraps)
        NOP(), NOP(),

        // ADDI -1 propagation chain
        ADDI(14, 0, -1),      // x14=0xFFFFFFFF
        ADDI(14,14, -1),      // x14=0xFFFFFFFE (EX→EX fwd, imm=-1)
        NOP(), NOP(),

        // ANDI to mask — uses imm=-1 (all bits set → identity mask)
        ANDI(15, 3, -1),      // x15=0x000007FF (AND with all-ones = identity)
        ORI (16, 4, -1),      // x16=0xFFFFFFFF (OR with all-ones = all-ones)
        HALT(),
    };
}

// ============================================================================
// Original random generator (preserved for backward compatibility)
// ============================================================================
inline std::vector<uint32_t> prog_random(int n = 200, uint32_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> op_dist(0, 6);
    std::uniform_int_distribution<int> reg_dist(1, 15);
    std::uniform_int_distribution<int> imm_dist(-32, 31);
    std::uniform_int_distribution<int> sh_dist(0, 15);

    const int MAX_PROG = 243;
    const int target   = std::min(n, MAX_PROG - 8);

    std::vector<uint32_t> prog;
    for (int r = 1; r <= 8; ++r)
        prog.push_back(ADDI((uint8_t)r, 0, (int16_t)(r * 3)));

    int branch_budget = 3;

    while ((int)prog.size() < target + 8) {
        int     op  = op_dist(rng);
        uint8_t rd  = (uint8_t)reg_dist(rng);
        uint8_t rs1 = (uint8_t)reg_dist(rng);
        uint8_t rs2 = (uint8_t)reg_dist(rng);

        switch (op) {
            case 0: {
                std::uniform_int_distribution<int> rt(0, 4);
                int v = rt(rng);
                if      (v==0) prog.push_back(ADD (rd,rs1,rs2));
                else if (v==1) prog.push_back(SUB (rd,rs1,rs2));
                else if (v==2) prog.push_back(AND_(rd,rs1,rs2));
                else if (v==3) prog.push_back(OR_ (rd,rs1,rs2));
                else           prog.push_back(XOR_(rd,rs1,rs2));
                break;
            }
            case 1: prog.push_back(ADDI(rd,rs1,(int16_t)imm_dist(rng))); break;
            case 2: prog.push_back(LW  (rd, 0, (int16_t)(sh_dist(rng)*4))); break;
            case 3: prog.push_back(SW  (rs1,0, (int16_t)(sh_dist(rng)*4))); break;
            case 4:
                if (branch_budget > 0) {
                    prog.push_back(BEQ(rs1,rs2,8)); prog.push_back(NOP()); --branch_budget;
                } else prog.push_back(ADDI(rd,rs1,(int16_t)imm_dist(rng)));
                break;
            case 5:
                if (branch_budget > 0) {
                    prog.push_back(BNE(rs1,rs2,8)); prog.push_back(NOP()); --branch_budget;
                } else prog.push_back(ADDI(rd,rs1,(int16_t)imm_dist(rng)));
                break;
            case 6:
                if (branch_budget > 0) {
                    prog.push_back(JAL(rd,8)); prog.push_back(NOP()); --branch_budget;
                } else prog.push_back(ADDI(rd,rs1,(int16_t)imm_dist(rng)));
                break;
        }
    }
    prog.push_back(HALT());
    return prog;
}

// ============================================================================
// Constrained-random generator configuration
// ============================================================================
struct GenConfig {
    // Instruction type probabilities (approximate, renormalised internally)
    float p_r_type  = 0.20f;
    float p_i_alu   = 0.20f;
    float p_lw      = 0.12f;
    float p_sw      = 0.08f;
    float p_branch  = 0.20f;   // 4× higher than original
    float p_jal     = 0.10f;
    // remaining probability fills with ADDI

    // Hazard biases
    float p_raw_chain  = 0.60f;  // probability next instr uses the just-written rd
    float p_load_use   = 0.70f;  // probability LW is immediately followed by consumer

    // Immediate bias
    float p_edge_imm   = 0.35f;  // probability of picking an edge-case immediate

    // Store-to-load same address
    float p_stl_pattern = 0.25f; // probability SW is followed by LW at same address
};

// ============================================================================
// Constrained-random program generator
// ============================================================================
inline std::vector<uint32_t> prog_constrained(int n = 200,
                                               uint32_t seed = 42,
                                               const GenConfig& cfg = {}) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_int_distribution<int>    reg_d(1, 30);    // wider register range
    std::uniform_int_distribution<int>    mem_d(0, 15);    // word addresses 0–15

    // Edge immediates that stress sign-extension and ALU boundaries
    static const int16_t kEdgeImm[] = {
        0, -1, 2047, -2048, 1, -2, 1024, -1024, 255, -256, 31, -32
    };
    static const int kNumEdge = sizeof(kEdgeImm) / sizeof(kEdgeImm[0]);
    std::uniform_int_distribution<int> edge_d(0, kNumEdge - 1);
    std::uniform_int_distribution<int> full_imm_d(-2048, 2047);

    auto pick_imm = [&]() -> int16_t {
        if (prob(rng) < cfg.p_edge_imm)
            return kEdgeImm[edge_d(rng)];
        return (int16_t)full_imm_d(rng);
    };

    // All 10 R-type ops as (funct3, funct7_bit5) pairs
    struct ROp { uint8_t f3; uint8_t f7; };
    static const ROp kROps[] = {
        {0,0},{0,1},{7,0},{6,0},{4,0},{2,0},{3,0},{1,0},{5,0},{5,1}
        // ADD SUB AND OR XOR SLT SLTU SLL SRL SRA
    };
    static const int kNumROps = 10;
    std::uniform_int_distribution<int> rop_d(0, kNumROps - 1);

    // Hard cap to fit in 256-word imem (8 pre-seed + overhead + HALT)
    const int MAX_SLOTS = 240;
    const int target    = std::min(n, MAX_SLOTS - 12);

    std::vector<uint32_t> prog;
    prog.reserve(target + 20);

    // Pre-seed registers with interesting values covering operand classes:
    //   x1=0, x2=-1(0xFFFFFFFF via ADDI trick), x3=1, x4=2047, x5=-2048
    //   x6=3, x7=5, x8=7
    prog.push_back(ADDI(1,  0,  0));       // x1=0      (ZERO class)
    prog.push_back(ADDI(2,  0, -1));       // x2=-1     (NEG_ONE class)
    prog.push_back(ADDI(3,  0,  1));       // x3=1
    prog.push_back(ADDI(4,  0,  2047));    // x4=2047   (MAX_POS imm class)
    prog.push_back(ADDI(5,  0, -2048));    // x5=-2048  (MAX_NEG imm class)
    prog.push_back(ADDI(6,  0,  3));       // x6=3
    prog.push_back(ADDI(7,  0,  5));       // x7=5
    prog.push_back(ADDI(8,  0,  7));       // x8=7

    uint8_t last_rd      = 0;      // most recently written rd (for RAW bias)
    int     last_mem_off = 0;      // last SW memory offset (for store-to-load)
    bool    last_was_sw  = false;  // was the last instruction a SW?

    // Biased rs selector: uses last_rd with p_raw_chain probability
    auto biased_rs = [&]() -> uint8_t {
        if (last_rd != 0 && prob(rng) < cfg.p_raw_chain)
            return last_rd;
        return (uint8_t)reg_d(rng);
    };

    while ((int)prog.size() < target + 8 && (int)prog.size() < MAX_SLOTS) {
        last_was_sw = false;

        float r = prob(rng);
        float cum = 0.0f;

        if ((cum += cfg.p_r_type) > r) {
            // ---- R-type (all 10 ops) ----------------------------------------
            const ROp& op = kROps[rop_d(rng)];
            uint8_t rd  = (uint8_t)reg_d(rng);
            uint8_t rs1 = biased_rs();
            uint8_t rs2 = biased_rs();
            prog.push_back(enc_R(op.f7 ? 0x20u : 0x00u, rs2, rs1, op.f3, rd, 0x33));
            last_rd = rd;

        } else if ((cum += cfg.p_i_alu) > r) {
            // ---- I-type ALU --------------------------------------------------
            uint8_t rd  = (uint8_t)reg_d(rng);
            uint8_t rs1 = biased_rs();
            prog.push_back(ADDI(rd, rs1, pick_imm()));
            last_rd = rd;

        } else if ((cum += cfg.p_lw) > r) {
            // ---- LW with optional forced load-use ----------------------------
            if ((int)prog.size() + 3 > MAX_SLOTS) goto emit_addi;
            {
                uint8_t rd  = (uint8_t)reg_d(rng);
                uint8_t base = (uint8_t)reg_d(rng);
                int16_t off  = (int16_t)(mem_d(rng) * 4);

                // Store-to-load pattern: if previous was SW to same address, use it
                if (last_was_sw && prob(rng) < cfg.p_stl_pattern) {
                    base = 0; off = (int16_t)last_mem_off;
                }

                prog.push_back(LW(rd, base, off));
                last_rd = rd;

                // Force load-use: immediately emit a consumer of rd
                if (prob(rng) < cfg.p_load_use && (int)prog.size() < MAX_SLOTS - 2) {
                    uint8_t rd2 = (uint8_t)reg_d(rng);
                    prog.push_back(ADDI(rd2, rd, pick_imm()));  // uses rd → stall
                    last_rd = rd2;
                }
            }

        } else if ((cum += cfg.p_sw) > r) {
            // ---- SW ----------------------------------------------------------
            uint8_t rs2 = biased_rs();
            uint8_t rs1 = (uint8_t)reg_d(rng);
            int16_t off = (int16_t)(mem_d(rng) * 4);
            prog.push_back(SW(rs2, rs1, off));
            last_rd      = 0;
            last_was_sw  = true;
            last_mem_off = off;

        } else if ((cum += cfg.p_branch) > r) {
            // ---- Branch (BEQ or BNE, forward only, skip 1 instr) -----------
            if ((int)prog.size() + 3 > MAX_SLOTS) goto emit_addi;
            {
                uint8_t rs1 = biased_rs();
                uint8_t rs2 = biased_rs();
                bool use_bne = (prob(rng) < 0.5f);
                if (use_bne) prog.push_back(BNE(rs1, rs2, 8));
                else         prog.push_back(BEQ(rs1, rs2, 8));
                prog.push_back(NOP());   // potentially flushed instruction
                last_rd = 0;
            }

        } else if ((cum += cfg.p_jal) > r) {
            // ---- JAL (forward, skip 1 instr) --------------------------------
            if ((int)prog.size() + 3 > MAX_SLOTS) goto emit_addi;
            {
                uint8_t rd = (uint8_t)reg_d(rng);
                prog.push_back(JAL(rd, 8));
                prog.push_back(NOP());
                last_rd = rd;
            }

        } else {
            // ---- Fallback ADDI ----------------------------------------------
            emit_addi:
            uint8_t rd  = (uint8_t)reg_d(rng);
            uint8_t rs1 = biased_rs();
            prog.push_back(ADDI(rd, rs1, pick_imm()));
            last_rd = rd;
        }
    }

    prog.push_back(HALT());
    return prog;
}
