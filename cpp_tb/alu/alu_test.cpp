#include <gtest/gtest.h>
#include <array>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "alu.h"
#include "alu_environment.h"
#include "alu_sequence.h"

const uint8_t ADD  = 0;
const uint8_t SUB  = 1;
const uint8_t AND  = 2;
const uint8_t OR   = 3;
const uint8_t XOR  = 4;
const uint8_t SLT  = 5;
const uint8_t SLL  = 6;
const uint8_t SRL  = 7;
const uint8_t SLTU = 8;
const uint8_t SRA  = 9;

// ---------------------------------------------------------------------------
// Test fixture — collects one (label, summary) entry per test/pass and prints
// the whole table in TearDownTestSuite so every percentage is visible in one
// place, without coverage hole noise scattered between test results.
// ---------------------------------------------------------------------------
class ALUTest : public ::testing::Test {
protected:
    ALU dut;

    // Append a row to the shared summary table.
    static void record(const std::string& label, const std::string& summary) {
        summary_table_.emplace_back(label, summary);
    }

    static void TearDownTestSuite() {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════╦══════════════════════════════╗\n";
        std::cout << "║  Test / Pass                             ║  Coverage                    ║\n";
        std::cout << "╠══════════════════════════════════════════╬══════════════════════════════╣\n";
        for (const auto& [label, cov] : summary_table_) {
            std::cout << "║  " << std::left << std::setw(40) << label
                      << "║  " << std::left << std::setw(28) << cov << "║\n";
        }
        std::cout << "╚══════════════════════════════════════════╩══════════════════════════════╝\n";
    }

private:
    static std::vector<std::pair<std::string, std::string>> summary_table_;
};

std::vector<std::pair<std::string, std::string>> ALUTest::summary_table_;

// ---------------------------------------------------------------------------
TEST_F(ALUTest, RandomStressTest) {
    ALUEnvironment env(dut);
    RandomStressSequence seq(10000);
    env.run_sequence(seq, "RandomStressTest");
    EXPECT_EQ(env.error_count(), 0);
    record("RandomStressTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
TEST_F(ALUTest, OperationSweepTest) {
    ALUEnvironment env(dut);
    OperationSweepSequence seq(1000);
    env.run_sequence(seq, "OperationSweepTest");
    EXPECT_EQ(env.error_count(), 0);
    record("OperationSweepTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
TEST_F(ALUTest, EdgeCaseTest) {
    ALUEnvironment env(dut);
    EdgeCaseSequence seq;
    env.run_sequence(seq, "EdgeCaseTest");
    EXPECT_EQ(env.error_count(), 0);
    record("EdgeCaseTest", env.coverage_summary());
}

// ---------------------------------------------------------------------------
TEST_F(ALUTest, StrictEdgeCaseTest) {
    // 1. Signed vs unsigned comparison
    dut.set_inputs(0x80000000, 0x00000001, SLT);
    EXPECT_EQ(dut.get_result(), 1u);
    dut.set_inputs(0x80000000, 0x00000001, SLTU);
    EXPECT_EQ(dut.get_result(), 0u);

    // 2. SRA sign extension
    dut.set_inputs(0x80000000, 1, SRA);
    EXPECT_EQ(dut.get_result(), 0xC0000000u);

    // 3. SRL no sign extension
    dut.set_inputs(0x80000000, 1, SRL);
    EXPECT_EQ(dut.get_result(), 0x40000000u);

    // 4. Shift by 0
    dut.set_inputs(0x12345678, 0, SLL);
    EXPECT_EQ(dut.get_result(), 0x12345678u);
    dut.set_inputs(0x12345678, 0, SRL);
    EXPECT_EQ(dut.get_result(), 0x12345678u);
    dut.set_inputs(0x12345678, 0, SRA);
    EXPECT_EQ(dut.get_result(), 0x12345678u);

    // 5. Shift by 31
    dut.set_inputs(0xFFFFFFFF, 31, SLL);
    EXPECT_EQ(dut.get_result(), 0x80000000u);
    dut.set_inputs(0xFFFFFFFF, 31, SRL);
    EXPECT_EQ(dut.get_result(), 0x00000001u);

    // 6. ADD overflow
    dut.set_inputs(0x7FFFFFFF, 1, ADD);
    EXPECT_EQ(dut.get_result(), 0x80000000u);
    dut.set_inputs(0xFFFFFFFF, 1, ADD);
    EXPECT_EQ(dut.get_result(), 0x00000000u);

    // 7. SUB
    dut.set_inputs(0xDEADBEEF, 0xDEADBEEF, SUB);
    EXPECT_EQ(dut.get_result(), 0u);
    dut.set_inputs(0x00000000, 1, SUB);
    EXPECT_EQ(dut.get_result(), 0xFFFFFFFFu);

    // 8. Logic ops
    dut.set_inputs(0xFFFFFFFF, 0x0, AND);
    EXPECT_EQ(dut.get_result(), 0x0u);
    dut.set_inputs(0xFFFFFFFF, 0x0, OR);
    EXPECT_EQ(dut.get_result(), 0xFFFFFFFFu);
    dut.set_inputs(0xFFFFFFFF, 0xFFFFFFFF, XOR);
    EXPECT_EQ(dut.get_result(), 0x0u);

    // 9. Same operand compare
    dut.set_inputs(0x12345678, 0x12345678, SLT);
    EXPECT_EQ(dut.get_result(), 0u);
    dut.set_inputs(0x12345678, 0x12345678, SLTU);
    EXPECT_EQ(dut.get_result(), 0u);
}

// ---------------------------------------------------------------------------
// CoverageClosureTest — UVM-style coverage enforcement
// ---------------------------------------------------------------------------
TEST_F(ALUTest, CoverageClosureTest) {
    ALUEnvironment env(dut);

    SpecialValueSweepSequence sweep(15);
    env.run_sequence(sweep, "CoverageClosureTest-Sweep");

    RandomStressSequence rand_seq(50000);
    env.run_sequence(rand_seq, "CoverageClosureTest-Random");

    EXPECT_EQ(env.error_count(), 0);

    auto holes = env.get_3d_coverage_holes(10);
    for (const auto& hole : holes) {
        EXPECT_GE(hole.hits, 10)
            << "Coverage closure failure — op=" << op_name(static_cast<uint8_t>(hole.op))
            << "  A=" << operand_cat_name(hole.cat_a)
            << "  B=" << operand_cat_name(hole.cat_b)
            << "  hits=" << hole.hits;
    }

    record("CoverageClosureTest (sweep+50k random)", env.coverage_summary());
}

// ---------------------------------------------------------------------------
// CoverageDrivenTest — feedback-loop coverage closure
// ---------------------------------------------------------------------------
TEST_F(ALUTest, CoverageDrivenTest) {
    ALUEnvironment env(dut);

    RandomStressSequence baseline(10000);
    env.run_sequence(baseline, "CovDriven-P1-baseline");
    record("CoverageDrivenTest  P1 baseline", env.coverage_summary());

    static const int kMaxPasses = 5;
    for (int pass = 1; pass <= kMaxPasses; ++pass) {
        if (env.is_coverage_closed()) break;
        CoverageDrivenSequence driven(env.coverage(), 10000, 10);
        env.run_sequence(driven, "CovDriven-P" + std::to_string(pass + 1));
        record("CoverageDrivenTest  P" + std::to_string(pass + 1) + " driven",
               env.coverage_summary());
    }

    EXPECT_EQ(env.error_count(), 0);
}

// ---------------------------------------------------------------------------
// InvariantTest — algebraic property checks
// ---------------------------------------------------------------------------
TEST_F(ALUTest, InvariantTest) {
    static const std::array<uint32_t, 8> kTestValues = {
        0u, 1u, 0xFFFFFFFFu, 0x7FFFFFFFu, 0x80000000u, 0xAAAAAAAAu, 0x55555555u, 0x12345678u
    };

    for (uint32_t a : kTestValues) {
        // XOR self → 0
        dut.set_inputs(a, a, XOR);
        EXPECT_EQ(dut.get_result(), 0u)
            << "XOR self != 0:  a=0x" << std::hex << a;

        // AND with all-ones → identity
        dut.set_inputs(a, 0xFFFFFFFFu, AND);
        EXPECT_EQ(dut.get_result(), a)
            << "AND all-ones != identity:  a=0x" << std::hex << a;

        // SLT(x, x) = 0
        dut.set_inputs(a, a, SLT);
        EXPECT_EQ(dut.get_result(), 0u)
            << "SLT(x,x) != 0:  x=0x" << std::hex << a;

        for (uint32_t b : kTestValues) {
            // ADD commutativity: a + b == b + a
            dut.set_inputs(a, b, ADD);
            uint32_t ab = dut.get_result();
            dut.set_inputs(b, a, ADD);
            uint32_t ba = dut.get_result();
            EXPECT_EQ(ab, ba)
                << "ADD not commutative:  a=0x" << std::hex << a << "  b=0x" << b;
        }
    }
}
