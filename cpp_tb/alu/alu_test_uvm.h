#pragma once
// alu_test_uvm.h — Test hierarchy (uvm_test equivalent)
//
// In SystemVerilog UVM:
//   class alu_test_base extends uvm_test;
//     alu_env env;
//     virtual function void build_phase(uvm_phase phase);
//       env = alu_env::type_id::create("env", this);
//     endfunction
//   endclass
//
// The C++ equivalent is a plain class hierarchy where each "test" is a runner
// that accepts a DUT reference and a config path, then executes a methodology:
//
//   ALURandomTest        — baseline random stress, no closure requirement
//   ALUDirectedTest      — edge-case + special-value directed sequences
//   ALUCoverageDrivenTest — full multi-pass coverage-closure flow
//   ALUFullRegressionTest — directed → random → coverage-driven in one run
//
// Test classes are NOT GoogleTest fixtures; they are invoked FROM fixtures in
// alu_top.cpp, keeping methodology (what to run) separate from harness
// (how GoogleTest owns the DUT and summary table).

#include <gtest/gtest.h>
#include <array>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "alu.h"
#include "alu_config.h"
#include "alu_env_uvm.h"
#include "alu_sequence.h"
#include "alu_coverage.h"
#include "alu_golden.h"

// ---------------------------------------------------------------------------
// Shared summary table — collects (label, summary) for the final printout
// ---------------------------------------------------------------------------
class ALUSummaryTable {
public:
    static void record(const std::string& label, const std::string& summary) {
        table_.emplace_back(label, summary);
    }

    static void print() {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════╦══════════════════════════════╗\n";
        std::cout << "║  Test / Phase                                ║  Coverage                    ║\n";
        std::cout << "╠══════════════════════════════════════════════╬══════════════════════════════╣\n";
        for (const auto& [label, cov] : table_) {
            std::cout << "║  " << std::left << std::setw(44) << label
                      << "║  " << std::left << std::setw(28) << cov << "║\n";
        }
        std::cout << "╚══════════════════════════════════════════════╩══════════════════════════════╝\n";
    }

private:
    static std::vector<std::pair<std::string, std::string>> table_;
};

inline std::vector<std::pair<std::string, std::string>> ALUSummaryTable::table_;

// ---------------------------------------------------------------------------
// GoogleTest fixture — shared DUT + teardown for all UVM tests
// ---------------------------------------------------------------------------
class ALUUVMFixture : public ::testing::Test {
protected:
    ALU dut;

    // Register a config for the env under path "env" before constructing it.
    static void configure(int num_transactions        = 10000,
                          int coverage_threshold      = 10,
                          int max_driven_passes       = 5,
                          bool enable_coverage_driven = true,
                          int verbosity               = 0) {
        ALUConfig cfg;
        cfg.num_transactions        = num_transactions;
        cfg.coverage_threshold      = coverage_threshold;
        cfg.max_driven_passes       = max_driven_passes;
        cfg.enable_coverage_driven  = enable_coverage_driven;
        cfg.verbosity               = verbosity;
        ALUConfigDB::set("env", cfg);
    }

    static void TearDownTestSuite() {
        ALUSummaryTable::print();
        ALUConfigDB::clear();
    }
};

// ---------------------------------------------------------------------------
// ALURandomTest — baseline random stress
// ---------------------------------------------------------------------------
inline void run_random_test(ALU& dut) {
    ALUUVMFixture::configure(10000, 10, 0, false, 0);
    ALUEnvUVM env(dut);

    RandomStressSequence seq(10000);
    env.run_sequence(seq, "ALURandomTest");

    EXPECT_EQ(env.error_count(), 0);
    ALUSummaryTable::record("ALURandomTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// ALUDirectedTest — edge-case and special-value sequences
// ---------------------------------------------------------------------------
inline void run_directed_test(ALU& dut) {
    ALUUVMFixture::configure(1000, 10, 0, false, 0);
    ALUEnvUVM env(dut);

    EdgeCaseSequence edge;
    env.run_sequence(edge, "edge-cases");

    SpecialValueSweepSequence sweep(15);
    env.run_sequence(sweep, "special-value-sweep");

    OperationSweepSequence op_sweep(1000);
    env.run_sequence(op_sweep, "operation-sweep");

    EXPECT_EQ(env.error_count(), 0);
    ALUSummaryTable::record("ALUDirectedTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// ALUCoverageDrivenTest — full multi-pass coverage-closure flow
// ---------------------------------------------------------------------------
inline void run_coverage_driven_test(ALU& dut) {
    ALUUVMFixture::configure(10000, 10, 5, true, 1);
    ALUEnvUVM env(dut);
    env.run_coverage_driven_flow();

    EXPECT_EQ(env.error_count(), 0);

    auto holes = env.get_3d_coverage_holes(10);
    for (const auto& hole : holes) {
        EXPECT_GE(hole.hits, 10)
            << "Coverage hole — op=" << op_name(static_cast<uint8_t>(hole.op))
            << "  A=" << operand_cat_name(hole.cat_a)
            << "  B=" << operand_cat_name(hole.cat_b)
            << "  hits=" << hole.hits;
    }

    ALUSummaryTable::record("ALUCoverageDrivenTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// ALUFullRegressionTest — sweep → random → coverage-driven in one env
// ---------------------------------------------------------------------------
inline void run_full_regression_test(ALU& dut) {
    ALUUVMFixture::configure(10000, 10, 5, true, 0);
    ALUEnvUVM env(dut);

    // Phase 0: directed sweep
    SpecialValueSweepSequence sweep(15);
    env.run_sequence(sweep, "regression-sweep");
    ALUSummaryTable::record("  FullRegression: after sweep", env.coverage_summary());

    // Phase 1: baseline random
    RandomStressSequence baseline(10000);
    env.run_sequence(baseline, "regression-baseline");
    ALUSummaryTable::record("  FullRegression: after baseline", env.coverage_summary());

    // Phase 2+: coverage-driven passes
    for (int pass = 1; pass <= 5 && !env.is_coverage_closed(); ++pass) {
        CoverageDrivenSequence driven(env.coverage(), 10000, 10);
        env.run_sequence(driven, "regression-driven-" + std::to_string(pass));
        ALUSummaryTable::record(
            "  FullRegression: driven pass " + std::to_string(pass),
            env.coverage_summary());
    }

    EXPECT_EQ(env.error_count(), 0);
    ALUSummaryTable::record("ALUFullRegressionTest (total)", env.coverage_summary());
}
