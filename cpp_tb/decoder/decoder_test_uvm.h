#pragma once
// decoder_test_uvm.h — Test fixture + test runner functions
//
// GoogleTest fixture: DecoderUVMFixture
//   - Owns the Decoder DUT
//   - TearDownTestSuite prints the coverage summary table
//
// Test runners (free functions taking Decoder& dut):
//   run_directed_test      — hand-crafted known instructions, must hit every variant
//   run_biased_test        — 80% ADD/ADDI/LW; proves random alone cannot close coverage
//   run_illegal_test       — all illegal encoding kinds; seeds Group F bins
//   run_boundary_test      — boundary immediates + register-zero patterns
//   run_coverage_driven_test — full 6-phase multi-pass closure flow
//   run_full_regression_test — all phases in one env instance, reports per-phase

#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "decoder.h"
#include "decoder_config.h"
#include "decoder_env_uvm.h"
#include "decoder_sequence.h"

// ---------------------------------------------------------------------------
// Summary table — collects (label, coverage%) pairs; printed at teardown
// ---------------------------------------------------------------------------
class DecoderSummaryTable {
public:
    static void record(const std::string& label, const std::string& cov) {
        table_.emplace_back(label, cov);
    }
    static void print() {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════╦══════════════════════════════╗\n";
        std::cout << "║  Decoder Test / Phase                            ║  Coverage                    ║\n";
        std::cout << "╠══════════════════════════════════════════════════╬══════════════════════════════╣\n";
        for (const auto& [label, cov] : table_) {
            std::cout << "║  " << std::left << std::setw(48) << label
                      << "║  " << std::left << std::setw(28) << cov << "║\n";
        }
        std::cout << "╚══════════════════════════════════════════════════╩══════════════════════════════╝\n";
    }
private:
    static std::vector<std::pair<std::string, std::string>> table_;
};
inline std::vector<std::pair<std::string, std::string>> DecoderSummaryTable::table_;

// ---------------------------------------------------------------------------
// Standalone config helper — callable from free functions and fixtures alike
// ---------------------------------------------------------------------------
inline void configure_env(int num_transactions        = 10000,
                           int coverage_threshold      = 10,
                           int max_driven_passes       = 5,
                           bool enable_coverage_driven = true,
                           int verbosity               = 0) {
    DecoderConfig cfg;
    cfg.num_transactions        = num_transactions;
    cfg.coverage_threshold      = coverage_threshold;
    cfg.max_driven_passes       = max_driven_passes;
    cfg.enable_coverage_driven  = enable_coverage_driven;
    cfg.verbosity               = verbosity;
    DecoderConfigDB::set("env", cfg);
}

// ---------------------------------------------------------------------------
// Shared fixture
// ---------------------------------------------------------------------------
class DecoderUVMFixture : public ::testing::Test {
protected:
    Decoder dut;

    static void TearDownTestSuite() {
        DecoderSummaryTable::print();
        DecoderConfigDB::clear();
    }
};

// ---------------------------------------------------------------------------
// run_directed_test — only hand-crafted instructions
// Verifies every legal opcode/variant/boundary is decoded correctly.
// Coverage contribution: should hit all Group A/B/C opcode×funct3 bins.
// ---------------------------------------------------------------------------
inline void run_directed_test(Decoder& dut) {
    configure_env(0, 10, 0, false, 0);
    DecoderEnvUVM env(dut);

    DirectedSequence seq;
    env.run_sequence(seq, "directed");

    EXPECT_EQ(env.error_count(), 0)
        << "Directed test: scoreboard errors detected";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Directed test: structural assertion violations";
    DecoderSummaryTable::record("DirectedTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// run_biased_test — 80% ADD/ADDI/LW; proves random cannot close coverage
// Expected: coverage plateaus significantly below 100%.
// This test does NOT assert coverage_closed(); it asserts coverage is NOT full.
// ---------------------------------------------------------------------------
inline void run_biased_test(Decoder& dut) {
    configure_env(20000, 10, 0, false, 0);
    DecoderEnvUVM env(dut);

    BiasedRandomSequence biased(20000);
    env.run_sequence(biased, "biased-random");

    EXPECT_EQ(env.error_count(), 0)
        << "Biased test: scoreboard errors detected";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Biased test: structural assertion violations";

    // The key assertion: biased random must NOT close coverage.
    // With 80% ADD/ADDI/LW, many bins across Groups B/D/E/F/G remain uncovered.
    EXPECT_FALSE(env.is_coverage_closed())
        << "Biased random unexpectedly closed coverage — "
           "the coverage model may not be discriminating enough";

    DecoderSummaryTable::record("BiasedRandomTest (should not close)", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// run_illegal_test — exercises all 6 illegal encoding kinds
// Verifies Group F illegal bins are hit and assert_illegal_opcode_default passes.
// ---------------------------------------------------------------------------
inline void run_illegal_test(Decoder& dut) {
    configure_env(0, 10, 0, false, 0);
    DecoderEnvUVM env(dut);

    // Prime with a light sweep so legal bins have context
    FullSweepSequence seed(3);
    env.run_sequence(seed, "seed");

    IllegalEncodingSequence illegal;
    env.run_sequence(illegal, "illegal-encodings");

    EXPECT_EQ(env.error_count(), 0)
        << "Illegal test: scoreboard errors (illegal instructions must not mismatch)";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Illegal test: structural assertion violations on illegal encodings";

    DecoderSummaryTable::record("IllegalEncodingTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// run_boundary_test — boundary immediates + register-zero patterns
// Verifies Groups D (ImmBoundary) and E (RegZero) are closed deterministically.
// ---------------------------------------------------------------------------
inline void run_boundary_test(Decoder& dut) {
    configure_env(0, 10, 0, false, 0);
    DecoderEnvUVM env(dut);

    // Seed legal opcode×funct3 bins first
    DirectedSequence directed;
    env.run_sequence(directed, "directed-seed");
    DecoderSummaryTable::record("  BoundaryTest: after directed seed", env.coverage_summary());

    BoundaryImmSequence boundary(8);
    env.run_sequence(boundary, "boundary-imm");
    DecoderSummaryTable::record("  BoundaryTest: after boundary-imm", env.coverage_summary());

    RegisterZeroSequence regzero(8);
    env.run_sequence(regzero, "register-zero");
    DecoderSummaryTable::record("  BoundaryTest: after register-zero", env.coverage_summary());

    RTypeFunct7Sequence rtype_f7(15);
    env.run_sequence(rtype_f7, "rtype-funct7");

    EXPECT_EQ(env.error_count(), 0)
        << "Boundary test: scoreboard errors detected";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Boundary test: structural assertion violations";

    DecoderSummaryTable::record("BoundaryTest (total)", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// run_coverage_driven_test — full 6-phase multi-pass closure
// ---------------------------------------------------------------------------
inline void run_coverage_driven_test(Decoder& dut) {
    configure_env(10000, 10, 8, true, 1);
    DecoderEnvUVM env(dut);
    env.run_coverage_driven_flow();

    EXPECT_EQ(env.error_count(), 0)
        << "Coverage-driven test: scoreboard errors detected";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Coverage-driven test: structural assertion violations";
    EXPECT_TRUE(env.is_coverage_closed())
        << "Coverage not closed after full driven flow. Final: "
        << env.coverage_summary();

    DecoderSummaryTable::record("CoverageDrivenTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// run_full_regression_test — all phases in one env, per-phase reporting
// Shows exactly how each phase contributes to coverage closure.
// ---------------------------------------------------------------------------
inline void run_full_regression_test(Decoder& dut) {
    configure_env(10000, 10, 8, true, 0);
    DecoderEnvUVM env(dut);

    // Phase 0: Directed
    {
        DirectedSequence s;
        env.run_sequence(s, "directed");
        DecoderSummaryTable::record("  Regression: p0 directed", env.coverage_summary());
    }

    // Phase 1: Illegal
    {
        IllegalEncodingSequence s;
        env.run_sequence(s, "illegal");
        DecoderSummaryTable::record("  Regression: p1 illegal-enc", env.coverage_summary());
    }

    // Phase 2: Boundary immediates
    {
        BoundaryImmSequence s(5);
        env.run_sequence(s, "boundary-imm");
        DecoderSummaryTable::record("  Regression: p2 boundary-imm", env.coverage_summary());
    }

    // Phase 3: Register zero
    {
        RegisterZeroSequence s(5);
        env.run_sequence(s, "reg-zero");
        DecoderSummaryTable::record("  Regression: p3 reg-zero", env.coverage_summary());
    }

    // Phase 4: R-type funct7
    {
        RTypeFunct7Sequence s(10);
        env.run_sequence(s, "rtype-f7");
        DecoderSummaryTable::record("  Regression: p4 rtype-f7", env.coverage_summary());
    }

    // Phase 5: Biased random (cannot close on its own — recorded for comparison)
    {
        BiasedRandomSequence s(10000);
        env.run_sequence(s, "biased-random");
        DecoderSummaryTable::record("  Regression: p5 biased-random", env.coverage_summary());
    }

    // Phase 6+: Coverage-driven feedback loop
    for (int pass = 1; pass <= 8 && !env.is_coverage_closed(); ++pass) {
        CoverageDrivenSequence s(env.coverage(), 10000, 10);
        env.run_sequence(s, "driven-" + std::to_string(pass));
        DecoderSummaryTable::record(
            "  Regression: p6+ driven pass " + std::to_string(pass),
            env.coverage_summary());
    }

    EXPECT_EQ(env.error_count(), 0)
        << "Full regression: scoreboard errors detected";
    EXPECT_EQ(env.assertion_violations(), 0)
        << "Full regression: structural assertion violations";
    EXPECT_TRUE(env.is_coverage_closed())
        << "Full regression: coverage not closed. Final: " << env.coverage_summary();

    DecoderSummaryTable::record("FullRegressionTest (total)", env.coverage_summary());
}
