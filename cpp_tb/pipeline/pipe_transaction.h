#pragma once
// pipe_transaction.h — Commit record types for the pipeline verification environment
//
// A CommitRecord represents one observable write event produced by the DUT:
//   REG — register-file write  (rd, data)
//   MEM — data-memory store    (word_addr, data)
//
// ExecStep now carries operand values sampled at execution time so the
// coverage model can bin rs1/rs2/imm into value classes without needing
// access to the register file.

#include <cstdint>
#include <sstream>
#include <string>

struct CommitRecord {
    enum class Kind { REG, MEM } kind = Kind::REG;

    uint32_t pc        = 0;   // PC of the originating instruction
    uint8_t  rd        = 0;   // Destination register  (REG only)
    uint32_t data      = 0;   // Written value (REG) or store data (MEM)
    uint32_t word_addr = 0;   // Word address = byte_addr >> 2  (MEM only)

    bool operator==(const CommitRecord& o) const {
        if (kind != o.kind) return false;
        if (kind == Kind::REG) return rd == o.rd && data == o.data;
        return word_addr == o.word_addr && data == o.data;
    }
    bool operator!=(const CommitRecord& o) const { return !(*this == o); }

    std::string to_string() const {
        std::ostringstream ss;
        if (kind == Kind::REG) {
            ss << "REG[pc=0x" << std::hex << pc
               << " x"        << std::dec << (int)rd
               << "=0x"       << std::hex << data << "]";
        } else {
            ss << "MEM[pc=0x" << std::hex << pc
               << " @w"       << std::dec << word_addr
               << "=0x"       << std::hex << data << "]";
        }
        return ss.str();
    }
};

// Instruction class tags — used by coverage model.
enum class InstrKind {
    R_TYPE, I_ALU, LW, SW, BEQ, BNE, JAL, NOP, UNKNOWN
};

inline InstrKind classify_instr(uint32_t instr) {
    const uint32_t op = instr & 0x7Fu;
    if (instr == 0x00000013u) return InstrKind::NOP;
    switch (op) {
        case 0x33: return InstrKind::R_TYPE;
        case 0x13: return InstrKind::I_ALU;
        case 0x03: return InstrKind::LW;
        case 0x23: return InstrKind::SW;
        case 0x63: return ((instr >> 12) & 7u) == 0 ? InstrKind::BEQ : InstrKind::BNE;
        case 0x6F: return InstrKind::JAL;
        default:   return InstrKind::UNKNOWN;
    }
}

// Coverage-annotated execution step from the reference model.
// rs1_val / rs2_val / imm_val are sampled at execution time so the coverage
// model can classify operand value classes without replaying the ISS.
struct ExecStep {
    uint32_t     pc           = 0;
    uint32_t     instr        = 0;
    InstrKind    kind         = InstrKind::UNKNOWN;

    // Hazard annotations (set by ReferenceModel::annotate_hazards)
    bool         branch_taken  = false;  // BEQ/BNE resolved taken
    bool         load_use_next = false;  // this instr stalls due to preceding LW
    bool         raw_hazard    = false;  // this instr uses forwarded result (non-LW)

    // Operand values at execution time (for coverage value-class binning)
    uint32_t     rs1_val      = 0;
    uint32_t     rs2_val      = 0;
    int32_t      imm_val      = 0;  // sign-extended immediate; 0 for R-type

    // Commit produced by this instruction (if any)
    CommitRecord commit;
    bool         has_commit   = false;
};
