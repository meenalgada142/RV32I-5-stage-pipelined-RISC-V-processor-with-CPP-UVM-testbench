// decoder_top.cpp — Top-level GoogleTest entry for the decoder UVM testbench
//
// Compilation (from cpp_tb/decoder/):
//   g++ -std=c++17 -I. decoder_top.cpp decoder.cpp \
//       $(pkg-config --cflags --libs gtest) -lpthread -o decoder_uvm_tb
//
// Run all:
//   ./decoder_uvm_tb
//
// Run individual:
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.DirectedTest
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.BiasedRandomTest
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.IllegalEncodingTest
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.BoundaryTest
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.CoverageDrivenTest
//   ./decoder_uvm_tb --gtest_filter=DecoderUVMFixture.FullRegressionTest

#include <gtest/gtest.h>
#include "decoder_config.h"
#include "decoder_test_uvm.h"

// ---------------------------------------------------------------------------
// Global environment — default config registered before the first test
// ---------------------------------------------------------------------------
class DecoderTopEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        DecoderConfig cfg;
        cfg.num_transactions       = 10000;
        cfg.coverage_threshold     = 10;
        cfg.max_driven_passes      = 8;
        cfg.enable_coverage_driven = true;
        cfg.verbosity              = 0;
        DecoderConfigDB::set("env", cfg);
    }
    void TearDown() override { DecoderConfigDB::clear(); }
};

static ::testing::Environment* const kTopEnv =
    ::testing::AddGlobalTestEnvironment(new DecoderTopEnvironment);

// ---------------------------------------------------------------------------
// TEST_F cases
// ---------------------------------------------------------------------------

// Every legal opcode / variant / boundary — deterministic, no RNG needed.
// All scoreboard and assertion checks must pass.
TEST_F(DecoderUVMFixture, DirectedTest) {
    run_directed_test(dut);
}

// 80% ADD/ADDI/LW. MUST NOT close coverage.
// Proves the v2 coverage model is not trivially closed by biased traffic.
TEST_F(DecoderUVMFixture, BiasedRandomTest) {
    run_biased_test(dut);
}

// All 6 illegal encoding kinds (unknown opcode, bad funct7, bad funct3…).
// Validates Group F bins and the assert_illegal_opcode_default assertion.
TEST_F(DecoderUVMFixture, IllegalEncodingTest) {
    run_illegal_test(dut);
}

// NEG_MIN / NEG_ONE / ZERO / POS_ONE / POS_MAX per instruction class (Group D)
// + rs1=x0 / rs2=x0 / rd=x0 per class (Group E).
TEST_F(DecoderUVMFixture, BoundaryTest) {
    run_boundary_test(dut);
}

// 6-phase closure: directed → illegal → boundary → reg-zero → rtype-f7 →
// biased-random → coverage-driven passes. Expects coverage_closed() at the end.
TEST_F(DecoderUVMFixture, CoverageDrivenTest) {
    run_coverage_driven_test(dut);
}

// All phases in a single env instance with per-phase coverage progression report.
TEST_F(DecoderUVMFixture, FullRegressionTest) {
    run_full_regression_test(dut);
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
