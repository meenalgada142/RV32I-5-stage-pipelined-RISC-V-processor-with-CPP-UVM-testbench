#pragma once
// pipe_test.h — GoogleTest fixture and all test runner functions
//
// Original tests (7):
//   run_arith_test, run_forwarding_test, run_load_use_test,
//   run_branch_test, run_jal_test, run_full_program_test,
//   run_regression_test
//
// Adversarial tests (6 new):
//   run_deep_dep_chain_test       — 8-deep same-register dependency chain
//   run_back_to_back_control_test — JAL + BEQ back-to-back flushes
//   run_load_use_branch_test      — LW + branch on loaded value
//   run_store_load_adjacent_test  — SW immediately followed by LW same address
//   run_overlapping_hazards_test  — load-use + RAW + branch in 6 instrs
//   run_imm_edge_cases_test       — ±2047/-1/0 immediates, SRAI/SLT boundaries
//
// Stress test (1 new):
//   run_long_stress_test — 100 constrained-random seeds × 500 instructions
//                          Writes results to pipeline_stress.csv.

#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "obj_dir/Vrv32i_pipe5_with_branches.h"
#include "pipe_env.h"
#include "pipe_sequence.h"

// ============================================================================
// Summary table
// ============================================================================
class PipeSummaryTable {
public:
    static void record(const std::string& label, const std::string& cov) {
        table_.emplace_back(label, cov);
    }
    static void print() {
        std::cout << "\n";
        std::cout << "+--------------------------------------------------+------------------------------+\n";
        std::cout << "|  Pipeline Test / Sequence                        |  Coverage                    |\n";
        std::cout << "+--------------------------------------------------+------------------------------+\n";
        for (const auto& [l, c] : table_) {
            std::cout << "|  " << std::left << std::setw(48) << l
                      << "|  " << std::left << std::setw(28) << c << "|\n";
        }
        std::cout << "+--------------------------------------------------+------------------------------+\n";
    }
private:
    static std::vector<std::pair<std::string, std::string>> table_;
};
inline std::vector<std::pair<std::string, std::string>> PipeSummaryTable::table_;

// ============================================================================
// Shared fixture — fresh DUT per TEST_F
// ============================================================================
class PipeUVMFixture : public ::testing::Test {
protected:
    VerilatedContext*           ctx = nullptr;
    Vrv32i_pipe5_with_branches* dut = nullptr;

    void SetUp() override {
        ctx = new VerilatedContext;
        dut = new Vrv32i_pipe5_with_branches(ctx, "TOP");
    }
    void TearDown() override {
        dut->final();
        delete dut; delete ctx;
        dut = nullptr; ctx = nullptr;
    }
    static void TearDownTestSuite() {
        PipeSummaryTable::print();
    }
};

// ============================================================================
// Original test runners
// ============================================================================

inline void run_arith_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_arith_basic());
    EXPECT_EQ(env.error_count(), 0) << "ArithBasic: scoreboard mismatches";
    PipeSummaryTable::record("ArithBasicTest", env.coverage_summary());
}

inline void run_forwarding_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_data_forwarding());
    EXPECT_EQ(env.error_count(), 0) << "DataForwarding: scoreboard mismatches";
    PipeSummaryTable::record("DataForwardingTest", env.coverage_summary());
}

inline void run_load_use_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_load_use());
    EXPECT_EQ(env.error_count(), 0) << "LoadUse: scoreboard mismatches";
    PipeSummaryTable::record("LoadUseTest", env.coverage_summary());
}

inline void run_branch_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_branch_taken());
    PipeSummaryTable::record("  BranchTest: taken",     env.coverage_summary());
    env.run_sequence(seq_branch_not_taken());
    PipeSummaryTable::record("  BranchTest: not-taken", env.coverage_summary());
    env.run_sequence(seq_bne_loop());
    PipeSummaryTable::record("  BranchTest: bne-loop",  env.coverage_summary());
    EXPECT_EQ(env.error_count(), 0) << "BranchTest: scoreboard mismatches";
    PipeSummaryTable::record("BranchTest (total)", env.coverage_summary());
}

inline void run_jal_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_jal_link());
    EXPECT_EQ(env.error_count(), 0) << "JalLink: scoreboard mismatches";
    PipeSummaryTable::record("JalLinkTest", env.coverage_summary());
}

inline void run_full_program_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_full());
    EXPECT_EQ(env.error_count(), 0) << "FullProgram: scoreboard mismatches";
    PipeSummaryTable::record("FullProgramTest", env.coverage_summary());
}

inline void run_regression_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 1);

    struct { const char* name; PipeSequence (*fn)(); } seqs[] = {
        { "arith-basic",      seq_arith_basic      },
        { "data-forwarding",  seq_data_forwarding  },
        { "load-use",         seq_load_use         },
        { "branch-taken",     seq_branch_taken     },
        { "branch-not-taken", seq_branch_not_taken },
        { "bne-loop",         seq_bne_loop         },
        { "jal-link",         seq_jal_link         },
        { "full-program",     seq_full             },
        { "imm-edge-cases",   seq_imm_edge_cases   },
    };
    for (const auto& s : seqs) {
        env.run_sequence(s.fn());
        PipeSummaryTable::record(std::string("  Regression: ") + s.name,
                                 env.coverage_summary());
    }
    for (uint32_t seed : {42u, 137u, 2718u}) {
        env.run_sequence(seq_random(200, seed));
        PipeSummaryTable::record(
            "  Regression: random[seed=" + std::to_string(seed) + "]",
            env.coverage_summary());
    }
    EXPECT_EQ(env.error_count(), 0)
        << "FullRegression: mismatches";
    EXPECT_TRUE(env.is_coverage_closed())
        << "FullRegression: coverage not closed: " << env.coverage_summary();
    PipeSummaryTable::record("FullRegressionTest (total)", env.coverage_summary());
    if (!env.is_coverage_closed() || env.error_count() > 0)
        std::cout << env.coverage_report();
}

// ============================================================================
// Adversarial test runners
// ============================================================================

// Test 1: 8-deep back-to-back writes to same register + ADD(rd,rd,rd).
// Catches forwarding priority bugs when EX→EX and MEM→EX both target x1.
inline void run_deep_dep_chain_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_deep_dep_chain());
    EXPECT_EQ(env.error_count(), 0)
        << "DeepDepChain: wrong value in long forwarding chain";
    PipeSummaryTable::record("DeepDepChainTest", env.coverage_summary());
}

// Test 2: JAL then BEQ/BNE within 3 instructions (two back-to-back flushes).
// Catches flush-valid register not cleared after first redirect.
inline void run_back_to_back_control_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_back_to_back_control());
    EXPECT_EQ(env.error_count(), 0)
        << "BackToBackControl: second flush window incorrect";
    PipeSummaryTable::record("BackToBackControlTest", env.coverage_summary());
}

// Test 3: LW then BEQ on loaded register (load-use stall + taken branch).
// Catches hazard-detection / branch-resolve interaction.
inline void run_load_use_branch_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_load_use_branch_operand());
    EXPECT_EQ(env.error_count(), 0)
        << "LoadUseBranch: stall+flush interaction wrong";
    PipeSummaryTable::record("LoadUseBranchTest", env.coverage_summary());
}

// Test 4: SW immediately followed by LW at same address; also 4-apart + overwrite.
// Catches data_mem write-before-read ordering at posedge.
inline void run_store_load_adjacent_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_store_load_adjacent());
    EXPECT_EQ(env.error_count(), 0)
        << "StoreLoadAdjacent: data_mem read-before-write ordering bug";
    PipeSummaryTable::record("StoreLoadAdjacentTest", env.coverage_summary());
}

// Test 5: load-use stall + EX→EX RAW + taken branch all within 6 instructions.
// Catches stall counter off-by-one causing branch to see stale operand.
inline void run_overlapping_hazards_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_overlapping_hazards());
    EXPECT_EQ(env.error_count(), 0)
        << "OverlappingHazards: stall+forward+branch interaction wrong";
    PipeSummaryTable::record("OverlappingHazardsTest", env.coverage_summary());
}

// Test 6: ±2047/-1/0 immediates; SRAI vs SRLI on negative values;
//         SLT/SLTU when rs1==rs2; ADD unsigned overflow.
// Catches sign-extension and funct7[5] decode errors.
inline void run_imm_edge_cases_test(Vrv32i_pipe5_with_branches* dut) {
    PipeEnvUVM env(dut, 0);
    env.run_sequence(seq_imm_edge_cases());
    EXPECT_EQ(env.error_count(), 0)
        << "ImmEdgeCases: sign-extension or funct7 decode error";
    PipeSummaryTable::record("ImmEdgeCasesTest", env.coverage_summary());
}

// ============================================================================
// Long-run stress test — 100 constrained seeds × 500 instructions
// ============================================================================
//
// Architecture:
//   • seed_env  : fresh env per seed, writes per-seed CSV rows
//   • accum_env : single env that accumulates coverage across all seeds
//
// Stopping criteria (all must pass):
//   1. Zero scoreboard/commit-count errors across all 100 seeds.
//   2. All 24 coverage bins closed.
//   3. Coverage closure achieved before seed index 80
//      (plateau earlier → bins are reachable; plateau later → suspect dead bins).

inline void run_long_stress_test(Vrv32i_pipe5_with_branches* dut) {
    // Accumulating env for cross-seed coverage tracking (no CSV for this one)
    PipeEnvUVM accum_env(dut, 0, 0);

    // Deterministic seed list from a 64-bit LCG (reproducible failures)
    std::vector<uint32_t> seeds;
    seeds.reserve(100);
    uint64_t lcg = 0xDEADBEEFCAFEBABEULL;
    for (int i = 0; i < 100; ++i) {
        lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL;
        seeds.push_back((uint32_t)(lcg >> 32));
    }

    int total_errors       = 0;
    int coverage_closed_at = -1;
    int prev_bins_hit      = 0;
    int stagnant_run       = 0;

    std::cout << "\n[stress] 100 seeds × 500 instrs. Results → pipeline_stress.csv\n";

    // Seed 0: run all directed + adversarial sequences to warm coverage
    {
        PipeEnvUVM seed0(dut, 0, seeds[0]);
        seed0.set_log_file("pipeline_stress.csv");
        for (auto fn : { seq_arith_basic, seq_data_forwarding, seq_load_use,
                         seq_branch_taken, seq_branch_not_taken, seq_bne_loop,
                         seq_jal_link, seq_full })
            seed0.run_sequence(fn());
        seed0.run_sequence(seq_deep_dep_chain());
        seed0.run_sequence(seq_back_to_back_control());
        seed0.run_sequence(seq_load_use_branch_operand());
        seed0.run_sequence(seq_store_load_adjacent());
        seed0.run_sequence(seq_overlapping_hazards());
        seed0.run_sequence(seq_imm_edge_cases());
        seed0.run_sequence(seq_constrained(500, seeds[0]));
        total_errors += seed0.error_count();

        // Mirror into accum
        for (auto fn : { seq_arith_basic, seq_data_forwarding, seq_load_use,
                         seq_branch_taken, seq_branch_not_taken, seq_bne_loop,
                         seq_jal_link, seq_full })
            accum_env.run_sequence(fn());
        accum_env.run_sequence(seq_deep_dep_chain());
        accum_env.run_sequence(seq_back_to_back_control());
        accum_env.run_sequence(seq_load_use_branch_operand());
        accum_env.run_sequence(seq_store_load_adjacent());
        accum_env.run_sequence(seq_overlapping_hazards());
        accum_env.run_sequence(seq_imm_edge_cases());
        accum_env.run_sequence(seq_constrained(500, seeds[0]));
    }

    // Seeds 1–99: constrained-random only
    for (int i = 1; i < 100; ++i) {
        PipeEnvUVM senv(dut, 0, seeds[i]);
        senv.set_log_file("pipeline_stress.csv");
        senv.run_sequence(seq_constrained(500, seeds[i]));
        total_errors += senv.error_count();

        accum_env.run_sequence(seq_constrained(500, seeds[i]));

        // Track closure
        if (coverage_closed_at < 0 && accum_env.is_coverage_closed())
            coverage_closed_at = i;

        // Track stagnation
        std::string summ = accum_env.coverage_summary();
        int hit = std::stoi(summ.substr(0, summ.find('/')));
        stagnant_run = (hit == prev_bins_hit) ? stagnant_run + 1 : 0;
        prev_bins_hit = hit;

        if ((i + 1) % 10 == 0) {
            std::cout << "[stress] " << std::setw(3) << (i+1)
                      << "/100  err=" << total_errors
                      << "  cov=" << accum_env.coverage_summary();
            if (coverage_closed_at >= 0)
                std::cout << "  [closed@" << coverage_closed_at << "]";
            std::cout << "\n";
        }
    }

    // Final report
    std::cout << "\n[stress] === FINAL REPORT ===\n";
    std::cout << "  Total errors:       " << total_errors << "\n";
    std::cout << "  Coverage:           " << accum_env.coverage_summary() << "\n";
    if (coverage_closed_at >= 0)
        std::cout << "  Closed at seed:     " << coverage_closed_at << " / 99\n";
    else
        std::cout << "  *** NOT CLOSED after 100 seeds ***\n";
    std::cout << "  Final stagnant run: " << stagnant_run << " seeds\n";
    std::cout << "  Results logged:     pipeline_stress.csv\n";

    if (total_errors > 0 || !accum_env.is_coverage_closed())
        std::cout << accum_env.coverage_report();

    // Assertions
    EXPECT_EQ(total_errors, 0)
        << "LongStress: " << total_errors << " errors across 100 seeds";
    EXPECT_TRUE(accum_env.is_coverage_closed())
        << "LongStress: coverage not closed: " << accum_env.coverage_summary();
    EXPECT_GE(coverage_closed_at, 0)
        << "LongStress: coverage never closed";
    EXPECT_LT(coverage_closed_at, 80)
        << "LongStress: coverage closed too late (seed " << coverage_closed_at
        << ") — possible unreachable bins";
}
