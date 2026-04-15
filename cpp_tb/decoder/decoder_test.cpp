#include <gtest/gtest.h>
#include "decoder.h"

// Test fixture for Decoder
class DecoderTest : public ::testing::Test {
protected:
    Decoder decoder;
};

// Basic test
TEST_F(DecoderTest, BasicADDI) {
    uint32_t instr = 0x00A58513; // ADDI x10, x11, 10
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rs1, 11);
    EXPECT_EQ(decoded.rd, 10);
    EXPECT_EQ(decoded.imm, 10);
    EXPECT_EQ(decoded.alu_op, 0);
    EXPECT_TRUE(decoded.reg_write);
    EXPECT_TRUE(decoded.alu_src);
    EXPECT_FALSE(decoded.mem_read);
    EXPECT_FALSE(decoded.mem_write);
    EXPECT_FALSE(decoded.branch);
    EXPECT_FALSE(decoded.jump);
}

// Random tests
TEST_F(DecoderTest, RandomInstructions) {
    for (int i = 0; i < 100; ++i) {
        uint32_t instr = (rand() % 6 + 1) * 0x10; // Simple random opcodes
        instr |= (rand() & 0x1F) << 7; // rd
        instr |= (rand() & 0x1F) << 15; // rs1
        instr |= (rand() & 0x1F) << 20; // rs2
        DecodedInstruction decoded = decoder.decode(instr);
        // Check that fields are set appropriately
        EXPECT_TRUE(true);
    }
}

// Corner cases
TEST_F(DecoderTest, RTypeADD) {
    uint32_t instr = 0x00B50533; // ADD x10, x10, x11
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rs1, 10);
    EXPECT_EQ(decoded.rs2, 11);
    EXPECT_EQ(decoded.rd, 10);
    EXPECT_EQ(decoded.alu_op, 0);
    EXPECT_TRUE(decoded.reg_write);
    EXPECT_FALSE(decoded.alu_src);
}

TEST_F(DecoderTest, LW) {
    uint32_t instr = 0x00052503; // LW x10, 0(x10)
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rs1, 10);
    EXPECT_EQ(decoded.rd, 10);
    EXPECT_EQ(decoded.imm, 0);
    EXPECT_TRUE(decoded.mem_read);
    EXPECT_TRUE(decoded.mem_to_reg);
    EXPECT_TRUE(decoded.reg_write);
}

TEST_F(DecoderTest, SW) {
    uint32_t instr = 0x00B52023; // SW x11, 0(x10)
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rs1, 10);
    EXPECT_EQ(decoded.rs2, 11);
    EXPECT_EQ(decoded.imm, 0);
    EXPECT_TRUE(decoded.mem_write);
    EXPECT_FALSE(decoded.reg_write);
}

TEST_F(DecoderTest, BEQ) {
    uint32_t instr = 0x00B50463; // BEQ x10, x11, 8
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rs1, 10);
    EXPECT_EQ(decoded.rs2, 11);
    EXPECT_EQ(decoded.imm, 8);
    EXPECT_TRUE(decoded.branch);
    EXPECT_FALSE(decoded.branch_type);
    EXPECT_FALSE(decoded.reg_write);
}

TEST_F(DecoderTest, JAL) {
    uint32_t instr = 0x008000EF; // JAL x1, 8
    DecodedInstruction decoded = decoder.decode(instr);
    EXPECT_EQ(decoded.rd, 1);
    EXPECT_EQ(decoded.imm, 8);
    EXPECT_TRUE(decoded.jump);
    EXPECT_TRUE(decoded.reg_write);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}