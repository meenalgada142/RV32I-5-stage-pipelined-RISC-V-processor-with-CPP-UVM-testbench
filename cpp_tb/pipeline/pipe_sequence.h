#pragma once
// pipe_sequence.h — Test sequences (uvm_sequence equivalent)
//
// Original sequences: arith_basic, data_forwarding, load_use, branch_taken,
//   branch_not_taken, bne_loop, jal_link, full, random
//
// Adversarial sequences (new):
//   deep_dep_chain          — 8-deep same-register dependency chain
//   back_to_back_control    — JAL + BEQ/BNE within 3 instructions
//   load_use_branch_operand — LW + branch on loaded value (stall + flush)
//   store_load_adjacent     — SW immediately followed by LW same address
//   overlapping_hazards     — load-use + RAW + taken branch in 6 instrs
//   imm_edge_cases          — ±2047, -1, 0 immediates; SRAI/SLT boundaries
//
// Constrained-random sequence:
//   seq_constrained(n, seed, cfg) — hazard-biased, edge-imm-biased generator

#include <cstdint>
#include <string>
#include <vector>

#include "program_gen.h"

struct DmemPreload { uint32_t word_addr; uint32_t data; };

struct PipeSequence {
    std::string              name;
    std::vector<uint32_t>    program;
    std::vector<DmemPreload> preloads;

    // Generous cycle budget: 8 cycles per instruction word + 100 drain cycles.
    uint64_t max_cycles() const {
        return (uint64_t)program.size() * 8 + 100;
    }
};

// ============================================================================
// Original sequences
// ============================================================================
inline PipeSequence seq_arith_basic()      { return { "ArithBasic",      prog_arith_basic(),      {} }; }
inline PipeSequence seq_data_forwarding()  { return { "DataForwarding",  prog_data_forwarding(),  {} }; }
inline PipeSequence seq_load_use()         { return { "LoadUse",         prog_load_use(),         {} }; }
inline PipeSequence seq_branch_taken()     { return { "BranchTaken",     prog_branch_taken(),     {} }; }
inline PipeSequence seq_branch_not_taken() { return { "BranchNotTaken",  prog_branch_not_taken(), {} }; }
inline PipeSequence seq_bne_loop()         { return { "BneLoop",         prog_bne_loop(),         {} }; }
inline PipeSequence seq_jal_link()         { return { "JalLink",         prog_jal_link(),         {} }; }
inline PipeSequence seq_full()             { return { "FullProgram",     prog_full(),             {} }; }

inline PipeSequence seq_random(int n = 200, uint32_t seed = 42) {
    return { "Random[n=" + std::to_string(n) + ",s=" + std::to_string(seed) + "]",
             prog_random(n, seed), {} };
}

// ============================================================================
// Adversarial sequences
// ============================================================================

// 8-deep same-register dependency chain + ADD(rd,rd,rd) patterns.
inline PipeSequence seq_deep_dep_chain() {
    return { "DeepDepChain", prog_deep_dep_chain(), {} };
}

// Two JAL + BEQ back-to-back flushes within a short window.
inline PipeSequence seq_back_to_back_control() {
    return { "BackToBackControl", prog_back_to_back_control(), {} };
}

// LW immediately followed by BEQ that uses the loaded register (stall+flush).
inline PipeSequence seq_load_use_branch_operand() {
    return { "LoadUseBranchOperand", prog_load_use_branch_operand(), {} };
}

// SW immediately followed by LW at same address; also 4-apart and overwrite.
inline PipeSequence seq_store_load_adjacent() {
    return { "StoreLoadAdjacent", prog_store_load_adjacent(), {} };
}

// Load-use stall + EX→EX RAW + taken branch all within 6 instructions.
inline PipeSequence seq_overlapping_hazards() {
    return { "OverlappingHazards", prog_overlapping_hazards(), {} };
}

// ±2047/-1/0 immediates, SRAI vs SRLI on negatives, SLT/SLTU boundaries.
inline PipeSequence seq_imm_edge_cases() {
    return { "ImmEdgeCases", prog_imm_edge_cases(), {} };
}

// ============================================================================
// Constrained-random sequence
// ============================================================================
inline PipeSequence seq_constrained(int n = 200, uint32_t seed = 42,
                                     const GenConfig& cfg = {}) {
    return { "Constrained[n=" + std::to_string(n) + ",s=" + std::to_string(seed) + "]",
             prog_constrained(n, seed, cfg), {} };
}
