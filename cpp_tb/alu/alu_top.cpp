// alu_top.cpp — Top-level GoogleTest entry point for the UVM-style testbench
//
// In SystemVerilog UVM this is tb_top.sv + the uvm_config_db::set() calls
// that parameterize the environment before simulation starts.
//
// Each TEST_F delegates to a run_*_test() free function defined in
// alu_test_uvm.h, passing the fixture's `dut` by reference.  This keeps
// the methodology (what to run) separate from the harness (GoogleTest
// fixture ownership of the DUT and TearDownTestSuite summary table).
//
// Compilation (example, from cpp_tb/alu/):
//   g++ -std=c++17 -I. alu_top.cpp alu.cpp \
//       $(pkg-config --cflags --libs gtest) -lpthread -o alu_uvm_tb
//
// Then run:
//   ./alu_uvm_tb
//   ./alu_uvm_tb --gtest_filter=ALUUVMFixture.FullRegressionTest

#include <gtest/gtest.h>
#include "alu_config.h"
#include "alu_test_uvm.h"

// ---------------------------------------------------------------------------
// Global environment — registers a default config before the first test
// ---------------------------------------------------------------------------
class ALUTopEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        ALUConfig cfg;
        cfg.num_transactions       = 10000;
        cfg.coverage_threshold     = 10;
        cfg.max_driven_passes      = 5;
        cfg.enable_coverage_driven = true;
        cfg.verbosity              = 0;
        ALUConfigDB::set("env", cfg);
    }

    void TearDown() override {
        ALUConfigDB::clear();
    }
};

static ::testing::Environment* const kTopEnv =
    ::testing::AddGlobalTestEnvironment(new ALUTopEnvironment);

// ---------------------------------------------------------------------------
// TEST_F cases — one per methodology
// ---------------------------------------------------------------------------
TEST_F(ALUUVMFixture, RandomTest) {
    run_random_test(dut);
}

TEST_F(ALUUVMFixture, DirectedTest) {
    run_directed_test(dut);
}

TEST_F(ALUUVMFixture, CoverageDrivenTest) {
    run_coverage_driven_test(dut);
}

TEST_F(ALUUVMFixture, FullRegressionTest) {
    run_full_regression_test(dut);
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
