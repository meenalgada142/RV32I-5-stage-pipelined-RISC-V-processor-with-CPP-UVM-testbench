#include "decoder.h"

Decoder::Decoder() {}

Decoder::~Decoder() {}

uint32_t Decoder::sign_extend(uint32_t value, int bits) {
    uint32_t sign_bit = (value >> (bits - 1)) & 1;
    if (sign_bit) {
        return value | (~0U << bits);
    }
    return value;
}

DecodedInstruction Decoder::decode(uint32_t instr) {
    DecodedInstruction decoded = {};

    uint8_t opcode = instr & 0x7F;
    uint8_t funct3 = (instr >> 12) & 0x7;
    uint8_t funct7 = (instr >> 25) & 0x7F;

    decoded.rs1 = (instr >> 15) & 0x1F;
    decoded.rs2 = (instr >> 20) & 0x1F;
    decoded.rd = (instr >> 7) & 0x1F;

    // Defaults
    decoded.imm = 0;
    decoded.alu_op = 0;
    decoded.reg_write = false;
    decoded.mem_read = false;
    decoded.mem_write = false;
    decoded.mem_to_reg = false;
    decoded.alu_src = false;
    decoded.branch = false;
    decoded.branch_type = false;
    decoded.jump = false;

    switch (opcode) {
        case 0x33: { // R-type
            decoded.alu_src = false;
            decoded.mem_to_reg = false;
            decoded.mem_read = false;
            decoded.mem_write = false;
            decoded.branch = false;
            decoded.jump = false;
            decoded.reg_write = true;
            switch (funct3) {
                case 0: decoded.alu_op = (funct7 == 0x20) ? 1 : 0; break; // SUB/ADD
                case 7: decoded.alu_op = 2; break; // AND
                case 6: decoded.alu_op = 3; break; // OR
                case 4: decoded.alu_op = 4; break; // XOR
                case 2: decoded.alu_op = 5; break; // SLT
                case 3: decoded.alu_op = 8; break; // SLTU
                case 1: decoded.alu_op = 6; break; // SLL
                case 5: decoded.alu_op = (funct7 == 0x20) ? 9 : 7; break; // SRA/SRL
            }
            break;
        }
        case 0x13: { // I-type
            decoded.alu_src = true;
            decoded.mem_to_reg = false;
            decoded.mem_read = false;
            decoded.mem_write = false;
            decoded.branch = false;
            decoded.jump = false;
            decoded.reg_write = true;
            decoded.imm = sign_extend((instr >> 20) & 0xFFF, 12);
            switch (funct3) {
                case 0: decoded.alu_op = 0; break; // ADDI
                case 2: decoded.alu_op = 5; break; // SLTI
                case 3: decoded.alu_op = 8; break; // SLTIU
                case 4: decoded.alu_op = 4; break; // XORI
                case 6: decoded.alu_op = 3; break; // ORI
                case 7: decoded.alu_op = 2; break; // ANDI
                case 1: decoded.alu_op = 6; break; // SLLI
                case 5: decoded.alu_op = ((instr >> 30) & 1) ? 9 : 7; break; // SRAI/SRLI
            }
            break;
        }
        case 0x03: { // LW
            decoded.alu_src = true;
            decoded.mem_to_reg = true;
            decoded.mem_read = true;
            decoded.mem_write = false;
            decoded.branch = false;
            decoded.jump = false;
            decoded.reg_write = true;
            decoded.imm = sign_extend((instr >> 20) & 0xFFF, 12);
            decoded.alu_op = 0;
            break;
        }
        case 0x23: { // SW
            decoded.alu_src = true;
            decoded.mem_to_reg = false;
            decoded.mem_read = false;
            decoded.mem_write = true;
            decoded.branch = false;
            decoded.jump = false;
            decoded.reg_write = false;
            uint32_t imm_s = (((instr >> 25) & 0x7F) << 5) | ((instr >> 7) & 0x1F);
            decoded.imm = sign_extend(imm_s, 12);
            decoded.alu_op = 0;
            break;
        }
        case 0x63: { // BEQ/BNE
            decoded.alu_src = false;
            decoded.mem_to_reg = false;
            decoded.mem_read = false;
            decoded.mem_write = false;
            decoded.branch = true;
            decoded.branch_type = (funct3 & 1);
            decoded.jump = false;
            decoded.reg_write = false;
            uint32_t imm_b = (((instr >> 31) & 1u) << 12) |
                             (((instr >>  7) & 1u) << 11) |
                             (((instr >> 25) & 0x3Fu) << 5) |
                             (((instr >>  8) & 0xFu) << 1);
            decoded.imm = sign_extend(imm_b, 13);
            decoded.alu_op = 1;
            break;
        }
        case 0x6F: { // JAL
            decoded.alu_src = true;
            decoded.mem_to_reg = false;
            decoded.mem_read = false;
            decoded.mem_write = false;
            decoded.branch = false;
            decoded.jump = true;
            decoded.reg_write = true;
            uint32_t imm_j = (((instr >> 31) & 1u)     << 20) |
                             (((instr >> 12) & 0xFFu)  << 12) |
                             (((instr >> 20) & 1u)     << 11) |
                             (((instr >> 21) & 0x3FFu) << 1);
            decoded.imm = sign_extend(imm_j, 21);
            decoded.alu_op = 0;
            break;
        }
    }

    return decoded;
}