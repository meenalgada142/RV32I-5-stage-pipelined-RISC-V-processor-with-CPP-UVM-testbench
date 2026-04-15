#pragma once
// decoder_golden.h — RISC-V RV32I decoder golden reference model
//
// Implements the *specification* (not the DUT's C++ model) so that the
// testbench can find bugs in decoder.cpp.  Immediate encodings match the
// RISC-V Unprivileged ISA spec and the RTL in rv32i_decoder.sv.
//
// Also provides:
//   - Instruction encoders for all supported opcodes (for sequence generation)
//   - Enum/name helpers for instruction classes

#include <cstdint>
#include <string>
#include <array>
#include <random>

#include "decoder_transaction.h"

// ---------------------------------------------------------------------------
// Instruction classes (used by coverage model and sequences)
// ---------------------------------------------------------------------------
enum class InstrClass : int {
    // R-type
    R_ADD = 0, R_SUB, R_AND, R_OR, R_XOR,
    R_SLT, R_SLTU, R_SLL, R_SRL, R_SRA,
    // I-type ALU
    I_ADDI, I_SLTI, I_SLTIU, I_XORI, I_ORI, I_ANDI, I_SLLI, I_SRLI, I_SRAI,
    // Memory
    LOAD_LW, STORE_SW,
    // Branch
    BRANCH_BEQ, BRANCH_BNE,
    // Jump
    JUMP_JAL,
    // Sentinel
    NUM_CLASSES
};

static const int kNumInstrClasses = static_cast<int>(InstrClass::NUM_CLASSES);

static const char* instr_class_name(InstrClass c) {
    static const char* names[] = {
        "R_ADD","R_SUB","R_AND","R_OR","R_XOR",
        "R_SLT","R_SLTU","R_SLL","R_SRL","R_SRA",
        "I_ADDI","I_SLTI","I_SLTIU","I_XORI","I_ORI","I_ANDI","I_SLLI","I_SRLI","I_SRAI",
        "LW","SW","BEQ","BNE","JAL"
    };
    return names[static_cast<int>(c)];
}

// ---------------------------------------------------------------------------
// Immediate categories (for coverage binning)
// ---------------------------------------------------------------------------
enum class ImmCategory : int {
    NOT_APPLICABLE = 0,  // R-type (no immediate)
    ZERO,
    SMALL_POS,   // 1 .. 127
    LARGE_POS,   // 128 .. max_positive
    SMALL_NEG,   // -128 .. -1
    LARGE_NEG,   // min_negative .. -129
    NUM_IMM_CATS
};

static const int kNumImmCats = static_cast<int>(ImmCategory::NUM_IMM_CATS);

static const char* imm_cat_name(ImmCategory c) {
    static const char* names[] = {"N/A","ZERO","SMALL_POS","LARGE_POS","SMALL_NEG","LARGE_NEG"};
    return names[static_cast<int>(c)];
}

// ---------------------------------------------------------------------------
// Register categories (for coverage binning)
// ---------------------------------------------------------------------------
enum class RegCategory : int {
    X0 = 0,    // x0 — hardwired zero, special in many instructions
    LOW,       // x1 .. x15
    HIGH,      // x16 .. x31
    NUM_REG_CATS
};

static const int kNumRegCats = static_cast<int>(RegCategory::NUM_REG_CATS);

static const char* reg_cat_name(RegCategory c) {
    static const char* names[] = {"X0","LOW","HIGH"};
    return names[static_cast<int>(c)];
}

static RegCategory classify_reg(uint8_t r) {
    if (r == 0)       return RegCategory::X0;
    if (r <= 15)      return RegCategory::LOW;
    return             RegCategory::HIGH;
}

// ---------------------------------------------------------------------------
// Immediate field extraction — matches rv32i_decoder.sv exactly
// ---------------------------------------------------------------------------
namespace imm {

// I-type: instr[31:20], sign-extended to 32 bits
inline uint32_t i_type(uint32_t instr) {
    uint32_t raw = (instr >> 20) & 0xFFF;
    return (raw & 0x800u) ? (raw | 0xFFFFF000u) : raw;
}

// S-type: {instr[31:25], instr[11:7]}, sign-extended
inline uint32_t s_type(uint32_t instr) {
    uint32_t raw = (((instr >> 25) & 0x7Fu) << 5) | ((instr >> 7) & 0x1Fu);
    return (raw & 0x800u) ? (raw | 0xFFFFF000u) : raw;
}

// B-type: {instr[31], instr[7], instr[30:25], instr[11:8], 1'b0}, sign-extended
inline uint32_t b_type(uint32_t instr) {
    uint32_t raw = (((instr >> 31) & 1u) << 12) |
                   (((instr >>  7) & 1u) << 11) |
                   (((instr >> 25) & 0x3Fu) << 5) |
                   (((instr >>  8) & 0xFu) << 1);
    return (raw & 0x1000u) ? (raw | 0xFFFFE000u) : raw;
}

// J-type: {instr[31], instr[19:12], instr[20], instr[30:21], 1'b0}, sign-extended
inline uint32_t j_type(uint32_t instr) {
    uint32_t raw = (((instr >> 31) & 1u)    << 20) |
                   (((instr >> 12) & 0xFFu) << 12) |
                   (((instr >> 20) & 1u)    << 11) |
                   (((instr >> 21) & 0x3FFu) << 1);
    return (raw & 0x100000u) ? (raw | 0xFFE00000u) : raw;
}

} // namespace imm

// ---------------------------------------------------------------------------
// Instruction encoders — produce valid 32-bit instruction words
// ---------------------------------------------------------------------------
namespace encode {

// R-type: {funct7[6:0], rs2[4:0], rs1[4:0], funct3[2:0], rd[4:0], 7'b0110011}
inline uint32_t r(uint8_t funct7, uint8_t rs2, uint8_t rs1,
                  uint8_t funct3, uint8_t rd) {
    return (uint32_t(funct7 & 0x7F) << 25) | (uint32_t(rs2 & 0x1F) << 20) |
           (uint32_t(rs1 & 0x1F) << 15) | (uint32_t(funct3 & 0x7) << 12) |
           (uint32_t(rd  & 0x1F) <<  7) | 0x33u;
}

// I-type generic
inline uint32_t i(uint8_t opcode, uint8_t funct3, uint8_t rd, uint8_t rs1, int32_t imm12) {
    uint32_t imm = uint32_t(imm12) & 0xFFF;
    return (imm << 20) | (uint32_t(rs1 & 0x1F) << 15) | (uint32_t(funct3 & 0x7) << 12) |
           (uint32_t(rd & 0x1F) << 7) | (opcode & 0x7Fu);
}

// I-type shift (SLLI/SRLI/SRAI) — funct7 encodes type, shamt in imm[4:0]
inline uint32_t i_shift(uint8_t funct7, uint8_t funct3, uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return (uint32_t(funct7 & 0x7F) << 25) | (uint32_t(shamt & 0x1F) << 20) |
           (uint32_t(rs1 & 0x1F) << 15) | (uint32_t(funct3 & 0x7) << 12) |
           (uint32_t(rd  & 0x1F) <<  7) | 0x13u;
}

// S-type (SW): {imm[11:5], rs2, rs1, 3'b010, imm[4:0], 7'b0100011}
inline uint32_t s(uint8_t rs1, uint8_t rs2, int32_t imm12) {
    uint32_t imm = uint32_t(imm12) & 0xFFF;
    return ((imm >> 5) << 25) | (uint32_t(rs2 & 0x1F) << 20) |
           (uint32_t(rs1 & 0x1F) << 15) | (0x2u << 12) |
           ((imm & 0x1F) << 7) | 0x23u;
}

// B-type (BEQ/BNE)
inline uint32_t b(uint8_t rs1, uint8_t rs2, uint8_t funct3, int32_t offset) {
    uint32_t i = uint32_t(offset);
    return (((i >> 12) & 1u) << 31) | (((i >> 5) & 0x3Fu) << 25) |
           (uint32_t(rs2 & 0x1F) << 20) | (uint32_t(rs1 & 0x1F) << 15) |
           (uint32_t(funct3 & 0x7) << 12) | (((i >> 1) & 0xFu) << 8) |
           (((i >> 11) & 1u) << 7) | 0x63u;
}

// J-type (JAL)
inline uint32_t j(uint8_t rd, int32_t offset) {
    uint32_t i = uint32_t(offset);
    return (((i >> 20) & 1u) << 31) | (((i >> 1) & 0x3FFu) << 21) |
           (((i >> 11) & 1u) << 20) | (((i >> 12) & 0xFFu) << 12) |
           (uint32_t(rd & 0x1F) << 7) | 0x6Fu;
}

} // namespace encode

// ---------------------------------------------------------------------------
// Golden reference model — fills tx.exp_* from tx.instr
// ---------------------------------------------------------------------------
inline void golden_decode(DecoderTransaction& tx) {
    const uint32_t instr  = tx.instr;
    const uint8_t  opcode = instr & 0x7Fu;
    const uint8_t  funct3 = (instr >> 12) & 0x7u;
    const uint8_t  funct7 = (instr >> 25) & 0x7Fu;

    tx.exp_rs1 = (instr >> 15) & 0x1F;
    tx.exp_rs2 = (instr >> 20) & 0x1F;
    tx.exp_rd  = (instr >>  7) & 0x1F;

    // defaults
    tx.exp_imm        = 0;
    tx.exp_alu_op     = 0;
    tx.exp_reg_write  = false;
    tx.exp_mem_read   = false;
    tx.exp_mem_write  = false;
    tx.exp_mem_to_reg = false;
    tx.exp_alu_src    = false;
    tx.exp_branch     = false;
    tx.exp_branch_type= false;
    tx.exp_jump       = false;

    switch (opcode) {
        case 0x33: { // R-type
            tx.exp_reg_write = true;
            tx.exp_alu_src   = false;
            switch (funct3) {
                case 0: tx.exp_alu_op = (funct7 == 0x20) ? 1u : 0u; break;
                case 7: tx.exp_alu_op = 2u;  break; // AND
                case 6: tx.exp_alu_op = 3u;  break; // OR
                case 4: tx.exp_alu_op = 4u;  break; // XOR
                case 2: tx.exp_alu_op = 5u;  break; // SLT
                case 3: tx.exp_alu_op = 8u;  break; // SLTU
                case 1: tx.exp_alu_op = 6u;  break; // SLL
                case 5: tx.exp_alu_op = (funct7 == 0x20) ? 9u : 7u; break;
                default: tx.exp_alu_op = 0u; break;
            }
            break;
        }
        case 0x13: { // I-type ALU
            tx.exp_reg_write = true;
            tx.exp_alu_src   = true;
            tx.exp_imm       = imm::i_type(instr);
            switch (funct3) {
                case 0: tx.exp_alu_op = 0u;  break; // ADDI
                case 2: tx.exp_alu_op = 5u;  break; // SLTI
                case 3: tx.exp_alu_op = 8u;  break; // SLTIU
                case 4: tx.exp_alu_op = 4u;  break; // XORI
                case 6: tx.exp_alu_op = 3u;  break; // ORI
                case 7: tx.exp_alu_op = 2u;  break; // ANDI
                case 1: tx.exp_alu_op = 6u;  break; // SLLI
                case 5: tx.exp_alu_op = (funct7 == 0x20) ? 9u : 7u; break;
                default: tx.exp_alu_op = 0u; break;
            }
            break;
        }
        case 0x03: { // LW
            tx.exp_reg_write  = true;
            tx.exp_alu_src    = true;
            tx.exp_mem_read   = true;
            tx.exp_mem_to_reg = true;
            tx.exp_imm        = imm::i_type(instr);
            tx.exp_alu_op     = 0u;
            break;
        }
        case 0x23: { // SW
            tx.exp_alu_src  = true;
            tx.exp_mem_write= true;
            tx.exp_imm      = imm::s_type(instr);
            tx.exp_alu_op   = 0u;
            break;
        }
        case 0x63: { // BEQ / BNE
            tx.exp_branch      = true;
            tx.exp_branch_type = (funct3 & 1u) != 0;
            tx.exp_imm         = imm::b_type(instr);
            tx.exp_alu_op      = 1u;
            break;
        }
        case 0x6F: { // JAL
            tx.exp_jump      = true;
            tx.exp_reg_write = true;
            tx.exp_alu_src   = true;
            tx.exp_imm       = imm::j_type(instr);
            tx.exp_alu_op    = 0u;
            break;
        }
        default:
            break; // unknown opcode — all defaults (zero / false)
    }
}

// ---------------------------------------------------------------------------
// Classify the instruction class from a transaction (after golden_decode)
// ---------------------------------------------------------------------------
inline InstrClass classify_instr(const DecoderTransaction& tx) {
    const uint8_t opcode = tx.instr & 0x7Fu;
    const uint8_t funct3 = (tx.instr >> 12) & 0x7u;
    const uint8_t funct7 = (tx.instr >> 25) & 0x7Fu;

    switch (opcode) {
        case 0x33:
            switch (funct3) {
                case 0: return (funct7 == 0x20) ? InstrClass::R_SUB : InstrClass::R_ADD;
                case 7: return InstrClass::R_AND;
                case 6: return InstrClass::R_OR;
                case 4: return InstrClass::R_XOR;
                case 2: return InstrClass::R_SLT;
                case 3: return InstrClass::R_SLTU;
                case 1: return InstrClass::R_SLL;
                case 5: return (funct7 == 0x20) ? InstrClass::R_SRA : InstrClass::R_SRL;
                default: break;
            }
            break;
        case 0x13:
            switch (funct3) {
                case 0: return InstrClass::I_ADDI;
                case 2: return InstrClass::I_SLTI;
                case 3: return InstrClass::I_SLTIU;
                case 4: return InstrClass::I_XORI;
                case 6: return InstrClass::I_ORI;
                case 7: return InstrClass::I_ANDI;
                case 1: return InstrClass::I_SLLI;
                case 5: return (funct7 == 0x20) ? InstrClass::I_SRAI : InstrClass::I_SRLI;
                default: break;
            }
            break;
        case 0x03: return InstrClass::LOAD_LW;
        case 0x23: return InstrClass::STORE_SW;
        case 0x63: return (funct3 & 1u) ? InstrClass::BRANCH_BNE : InstrClass::BRANCH_BEQ;
        case 0x6F: return InstrClass::JUMP_JAL;
        default: break;
    }
    return InstrClass::NUM_CLASSES; // unknown
}

// ---------------------------------------------------------------------------
// Classify the immediate value of a transaction (for coverage)
// ---------------------------------------------------------------------------
inline ImmCategory classify_imm(InstrClass cls, uint32_t imm) {
    // R-type has no immediate
    if (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA)
        return ImmCategory::NOT_APPLICABLE;

    int32_t signed_imm = static_cast<int32_t>(imm);
    if (signed_imm == 0)      return ImmCategory::ZERO;
    if (signed_imm > 0) {
        return (signed_imm <= 127) ? ImmCategory::SMALL_POS : ImmCategory::LARGE_POS;
    }
    return (signed_imm >= -128) ? ImmCategory::SMALL_NEG : ImmCategory::LARGE_NEG;
}

// ---------------------------------------------------------------------------
// Biased random register (30% chance of x0)
// ---------------------------------------------------------------------------
inline uint8_t random_reg(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> p(0, 9);
    if (p(rng) < 3) return 0;  // 30% x0
    std::uniform_int_distribution<uint8_t> r(1, 31);
    return r(rng);
}

// ---------------------------------------------------------------------------
// Biased random 12-bit signed immediate
// ---------------------------------------------------------------------------
inline int32_t random_imm12(std::mt19937_64& rng) {
    static const std::array<int32_t, 6> kSpecial = {0, 1, -1, 2047, -2048, 127};
    std::uniform_int_distribution<int> p(0, 9);
    if (p(rng) < 4) {
        std::uniform_int_distribution<int> pick(0, int(kSpecial.size()) - 1);
        return kSpecial[pick(rng)];
    }
    std::uniform_int_distribution<int32_t> full(-2048, 2047);
    return full(rng);
}

// ---------------------------------------------------------------------------
// Biased random B-type offset (multiples of 2, range ±4096)
// ---------------------------------------------------------------------------
inline int32_t random_b_offset(std::mt19937_64& rng) {
    static const std::array<int32_t, 4> kSpecial = {0, 2, -2, 4094};
    std::uniform_int_distribution<int> p(0, 9);
    if (p(rng) < 3) {
        std::uniform_int_distribution<int> pick(0, int(kSpecial.size()) - 1);
        return kSpecial[pick(rng)];
    }
    std::uniform_int_distribution<int32_t> raw(-2048, 2047);
    return raw(rng) * 2;  // B-type offset must be even
}

// ---------------------------------------------------------------------------
// Biased random J-type offset (multiples of 2, range ±1M)
// ---------------------------------------------------------------------------
inline int32_t random_j_offset(std::mt19937_64& rng) {
    static const std::array<int32_t, 4> kSpecial = {0, 2, -2, 1048574};
    std::uniform_int_distribution<int> p(0, 9);
    if (p(rng) < 3) {
        std::uniform_int_distribution<int> pick(0, int(kSpecial.size()) - 1);
        return kSpecial[pick(rng)];
    }
    std::uniform_int_distribution<int32_t> raw(-524288, 524287);
    return raw(rng) * 2;
}
