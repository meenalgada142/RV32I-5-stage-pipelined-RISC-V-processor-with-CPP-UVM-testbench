// tb_core.cpp — Top-level GoogleTest entry for the pipeline UVM testbench
//
// Build (from cpp_tb/pipeline/):
//   make
//
// Run all tests:
//   ./pipeline_tb
//
// Run individual test:
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.ArithBasicTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.DataForwardingTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.LoadUseTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.BranchTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.JalTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.FullProgramTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.FullRegressionTest
//
// Adversarial tests:
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.DeepDepChainTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.BackToBackControlTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.LoadUseBranchTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.StoreLoadAdjacentTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.OverlappingHazardsTest
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.ImmEdgeCasesTest
//
// Stress test (writes pipeline_stress.csv):
//   ./pipeline_tb --gtest_filter=PipeUVMFixture.LongStressTest

#include <gtest/gtest.h>
#include "verilated.h"
#include "pipe_test.h"

// Required stub for non-SystemC Verilator builds.
double sc_time_stamp() { return 0; }

// ===========================================================================
// Original 7 tests
// ===========================================================================

// Basic R/I arithmetic, no hazards — verifies decoder and all ALU ops.
TEST_F(PipeUVMFixture, ArithBasicTest) {
    run_arith_test(dut);
}

// Back-to-back RAW hazards — exercises EX→EX and MEM→EX forwarding paths.
TEST_F(PipeUVMFixture, DataForwardingTest) {
    run_forwarding_test(dut);
}

// LW immediately followed by consumer — pipeline must insert 1-cycle stall.
TEST_F(PipeUVMFixture, LoadUseTest) {
    run_load_use_test(dut);
}

// BEQ taken, BEQ not-taken, BNE countdown loop — verifies 2-cycle flush.
TEST_F(PipeUVMFixture, BranchTest) {
    run_branch_test(dut);
}

// JAL: correct link address (PC+4) and redirect to jump target.
TEST_F(PipeUVMFixture, JalTest) {
    run_jal_test(dut);
}

// Combined program with all hazard types in one coherent sequence.
TEST_F(PipeUVMFixture, FullProgramTest) {
    run_full_program_test(dut);
}

// Full regression: all directed + random stress, must close original coverage.
TEST_F(PipeUVMFixture, FullRegressionTest) {
    run_regression_test(dut);
}

// ===========================================================================
// Adversarial tests (6 new)
// ===========================================================================

// 8-deep same-register dependency chain + ADD(rd,rd,rd).
// Catches forwarding priority logic errors on long consecutive chains.
TEST_F(PipeUVMFixture, DeepDepChainTest) {
    run_deep_dep_chain_test(dut);
}

// JAL + BEQ back-to-back within 3 instructions (two flush windows).
// Catches flush-valid pipeline register not cleared after first redirect.
TEST_F(PipeUVMFixture, BackToBackControlTest) {
    run_back_to_back_control_test(dut);
}

// LW then BEQ on loaded register (load-use stall + taken branch).
// Catches hazard-detection / branch-resolve interaction.
TEST_F(PipeUVMFixture, LoadUseBranchTest) {
    run_load_use_branch_test(dut);
}

// SW then LW at same address: adjacent, 4-apart, and overwrite patterns.
// Catches data_mem write-before-read ordering at posedge.
TEST_F(PipeUVMFixture, StoreLoadAdjacentTest) {
    run_store_load_adjacent_test(dut);
}

// Load-use stall + EX→EX RAW forward + taken branch in 6-instruction window.
// Catches stall-counter off-by-one causing branch to see stale operand.
TEST_F(PipeUVMFixture, OverlappingHazardsTest) {
    run_overlapping_hazards_test(dut);
}

// ±2047/-1/0 immediates; SRAI vs SRLI on negatives; SLT/SLTU when rs1==rs2.
// Catches sign-extension and funct7[5] decoder errors.
TEST_F(PipeUVMFixture, ImmEdgeCasesTest) {
    run_imm_edge_cases_test(dut);
}

// ===========================================================================
// Long-run stress test
// ===========================================================================

// 100 constrained-random seeds × 500 instructions each.
// Verifies: zero errors, 24/24 coverage bins, coverage plateau before seed 80.
// Output: pipeline_stress.csv in the working directory.
TEST_F(PipeUVMFixture, LongStressTest) {
    run_long_stress_test(dut);
}

// ===========================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
