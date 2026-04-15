#pragma once
// reference_model.h — C++ ISS (Instruction Set Simulator) for RV32I subset
//
// Supported instructions (matching rtl/rv32i_decoder.sv):
//   R-type:  ADD SUB AND OR XOR SLT SLTU SLL SRL SRA
//   I-type:  ADDI SLTI SLTIU XORI ORI ANDI SLLI SRLI SRAI
//   Load:    LW
//   Store:   SW
//   Branch:  BEQ BNE
//   Jump:    JAL
//
// Execution is in program order (in-order reference for in-order pipeline).
// run() returns annotated ExecStep list; each step with has_commit=true
// contributes a CommitRecord to the expected queue.
//
// Hazard annotation (annotate_hazards) checks 1-back and 2-back steps to
// cover both EX/MEM forwarding paths:
//   1-back = instruction in MEM stage when current is in EX  (EX→EX fwd)
//   2-back = instruction in WB  stage when current is in EX  (MEM→EX fwd)
//
// Termination: JAL x0,0 (0x0000006F) OR max_instrs limit.

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "pipe_transaction.h"

class ReferenceModel {
public:
    static constexpr uint32_t MEM_WORDS = 256u;

    ReferenceModel() { reset(); }

    void reset() {
        regs_.fill(0);
        imem_.fill(0);
        dmem_.clear();
        pc_ = 0;
        steps_.clear();
        step_ok_  = true;
        raw_cnt_  = 0; ldu_cnt_ = 0;
        btn_cnt_  = 0; ctl_cnt_ = 0;
    }

    // -----------------------------------------------------------------------
    void load_program(const std::vector<uint32_t>& prog) {
        for (size_t i = 0; i < prog.size() && i < MEM_WORDS; ++i)
            imem_[i] = prog[i];
    }

    void preload_dmem(uint32_t word_addr, uint32_t val) {
        if (word_addr < MEM_WORDS) dmem_[word_addr] = val;
    }

    // -----------------------------------------------------------------------
    const std::vector<ExecStep>& run(uint32_t max_instrs = 8000) {
        steps_.clear();
        for (uint32_t i = 0; i < max_instrs; ++i) {
            uint32_t waddr = pc_ >> 2;
            if (waddr >= MEM_WORDS) break;
            uint32_t instr = imem_[waddr];
            if (instr == 0x0000006Fu) break;  // JAL x0,0 halt

            ExecStep step = execute_one(instr);
            annotate_hazards(step);
            steps_.push_back(step);

            if (!step_ok_) break;
        }
        return steps_;
    }

    std::vector<CommitRecord> commit_list() const {
        std::vector<CommitRecord> out;
        for (const auto& s : steps_)
            if (s.has_commit) out.push_back(s.commit);
        return out;
    }

    uint32_t reg(int i)           const { return regs_[i]; }
    uint32_t dmem(uint32_t waddr) const {
        auto it = dmem_.find(waddr);
        return (it != dmem_.end()) ? it->second : 0u;
    }
    uint32_t pc()                 const { return pc_; }

    int raw_hazard_count()   const { return raw_cnt_; }
    int load_use_count()     const { return ldu_cnt_; }
    int branch_taken_count() const { return btn_cnt_; }
    int control_hazard_count() const { return ctl_cnt_; }

private:
    bool     step_ok_ = true;
    int      raw_cnt_ = 0, ldu_cnt_ = 0, btn_cnt_ = 0, ctl_cnt_ = 0;

    std::array<uint32_t, 32>       regs_;
    std::array<uint32_t, MEM_WORDS> imem_;
    std::unordered_map<uint32_t, uint32_t> dmem_;
    uint32_t pc_ = 0;
    std::vector<ExecStep> steps_;

    // -----------------------------------------------------------------------
    // Immediate extractors
    // -----------------------------------------------------------------------
    static int32_t sext12(uint32_t instr) { return (int32_t)instr >> 20; }
    static int32_t imm_s(uint32_t instr) {
        return (int32_t)(instr & 0xFE000000u) >> 20
             | (int32_t)((instr >> 7) & 0x1Fu);
    }
    static int32_t imm_b(uint32_t instr) {
        return (int32_t)(instr & 0x80000000u) >> 19
             | (int32_t)((instr << 4)  & 0x800u)
             | (int32_t)((instr >> 20) & 0x7E0u)
             | (int32_t)((instr >> 7)  & 0x1Eu);
    }
    static int32_t imm_j(uint32_t instr) {
        return (int32_t)(instr & 0x80000000u) >> 11
             | (int32_t)(instr & 0xFF000u)
             | (int32_t)((instr >> 9)  & 0x800u)
             | (int32_t)((instr >> 20) & 0x7FEu);
    }

    // -----------------------------------------------------------------------
    ExecStep execute_one(uint32_t instr) {
        ExecStep step;
        step.pc    = pc_;
        step.instr = instr;
        step.kind  = classify_instr(instr);

        const uint32_t op     = instr & 0x7Fu;
        const uint8_t  rd     = (instr >> 7)  & 0x1Fu;
        const uint32_t funct3 = (instr >> 12) & 0x7u;
        const uint8_t  rs1    = (instr >> 15) & 0x1Fu;
        const uint8_t  rs2    = (instr >> 20) & 0x1Fu;
        const uint32_t funct7 = (instr >> 25) & 0x7Fu;

        const uint32_t rv1 = regs_[rs1];
        const uint32_t rv2 = regs_[rs2];
        const uint32_t cur_pc = pc_;

        // Capture operand values for coverage
        step.rs1_val = rv1;
        step.rs2_val = rv2;

        auto reg_write = [&](uint8_t dst, uint32_t val) {
            if (dst != 0) {
                regs_[dst] = val;
                step.commit.kind  = CommitRecord::Kind::REG;
                step.commit.pc    = cur_pc;
                step.commit.rd    = dst;
                step.commit.data  = val;
                step.has_commit   = true;
            }
        };

        switch (op) {
            // ---- R-type -------------------------------------------------------
            case 0x33: {
                step.imm_val = 0;
                const uint32_t sh = rv2 & 0x1Fu;
                uint32_t res = 0;
                switch (funct3) {
                    case 0: res = (funct7 & 0x20u) ? rv1 - rv2 : rv1 + rv2; break;
                    case 7: res = rv1 & rv2;  break;
                    case 6: res = rv1 | rv2;  break;
                    case 4: res = rv1 ^ rv2;  break;
                    case 2: res = ((int32_t)rv1 < (int32_t)rv2) ? 1u : 0u; break;
                    case 3: res = (rv1 < rv2) ? 1u : 0u; break;
                    case 1: res = rv1 << sh;  break;
                    case 5: res = (funct7 & 0x20u)
                                  ? (uint32_t)((int32_t)rv1 >> sh)
                                  : rv1 >> sh; break;
                    default: step_ok_ = false; pc_ += 4; return step;
                }
                reg_write(rd, res);
                pc_ += 4;
                break;
            }
            // ---- I-type ALU ---------------------------------------------------
            case 0x13: {
                const int32_t  imm = sext12(instr);
                const uint32_t sh  = (instr >> 20) & 0x1Fu;
                step.imm_val = imm;
                uint32_t res = 0;
                switch (funct3) {
                    case 0: res = rv1 + (uint32_t)imm; break;
                    case 2: res = ((int32_t)rv1 < imm) ? 1u : 0u; break;
                    case 3: res = (rv1 < (uint32_t)imm) ? 1u : 0u; break;
                    case 4: res = rv1 ^ (uint32_t)imm; break;
                    case 6: res = rv1 | (uint32_t)imm; break;
                    case 7: res = rv1 & (uint32_t)imm; break;
                    case 1: res = rv1 << sh; break;
                    case 5: res = (funct7 & 0x20u)
                                  ? (uint32_t)((int32_t)rv1 >> sh)
                                  : rv1 >> sh; break;
                    default: step_ok_ = false; pc_ += 4; return step;
                }
                reg_write(rd, res);
                pc_ += 4;
                break;
            }
            // ---- LW -----------------------------------------------------------
            case 0x03: {
                const int32_t imm = sext12(instr);
                step.imm_val = imm;
                uint32_t ea    = rv1 + (uint32_t)imm;
                // Match DUT's address truncation: data_mem[alu_result[9:2]]
                // = (byte_addr >> 2) & (MEM_WORDS-1).  Without this mask the
                // reference model returns 0 for ea >= 1024 while the DUT wraps
                // to a real (potentially non-zero) location, causing cascading
                // mismatches on all subsequent instructions using the loaded reg.
                uint32_t waddr = (ea >> 2) & (MEM_WORDS - 1u);
                uint32_t val   = dmem(waddr);
                reg_write(rd, val);
                pc_ += 4;
                break;
            }
            // ---- SW -----------------------------------------------------------
            case 0x23: {
                const int32_t imm = imm_s(instr);
                step.imm_val = imm;
                uint32_t ea    = rv1 + (uint32_t)imm;
                // Same wrap-around addressing as LW — match DUT's [9:2] select.
                uint32_t waddr = (ea >> 2) & (MEM_WORDS - 1u);
                dmem_[waddr] = rv2;
                step.commit.kind      = CommitRecord::Kind::MEM;
                step.commit.pc        = cur_pc;
                // commit.word_addr stores the FULL (unmasked) >> 2 value so that
                // the scoreboard comparison matches the monitor's report of
                // ex_mem_alu_result >> 2 (also unmasked).  The physical storage
                // location is waddr (masked), but identity of the commit is the
                // architectural address.
                step.commit.word_addr = ea >> 2;
                step.commit.data      = rv2;
                step.has_commit       = true;
                pc_ += 4;
                break;
            }
            // ---- BEQ / BNE ----------------------------------------------------
            case 0x63: {
                const int32_t imm = imm_b(instr);
                step.imm_val = imm;
                bool taken = (funct3 == 0) ? (rv1 == rv2) : (rv1 != rv2);
                step.branch_taken = taken;
                if (taken) { pc_ = cur_pc + (uint32_t)imm; ++btn_cnt_; ++ctl_cnt_; }
                else        { pc_ += 4; }
                break;
            }
            // ---- JAL ----------------------------------------------------------
            case 0x6F: {
                const int32_t imm = imm_j(instr);
                step.imm_val = imm;
                reg_write(rd, cur_pc + 4u);
                pc_ = cur_pc + (uint32_t)imm;
                ++ctl_cnt_;
                break;
            }
            // ---- NOP / unsupported --------------------------------------------
            default:
                step.imm_val = 0;
                pc_ += 4;
                break;
        }
        return step;
    }

    // -----------------------------------------------------------------------
    // Annotate hazards for the just-computed step.
    //
    // Checks two previous instructions to cover both forwarding paths:
    //   1-back  → EX/MEM register (forward_a = 2'b10, EX→EX path)
    //   2-back  → MEM/WB register (forward_a = 2'b01, MEM→EX path)
    //
    // EX/MEM path (1-back) has priority: if both paths reference the same
    // destination register, the closer one wins (matches RTL forwarding unit).
    // -----------------------------------------------------------------------
    void annotate_hazards(ExecStep& step) {
        if (steps_.empty()) return;

        const uint32_t cur_rs1 = (step.instr >> 15) & 0x1Fu;
        const uint32_t cur_rs2 = (step.instr >> 20) & 0x1Fu;

        auto is_reg_writer = [](const ExecStep& s, uint32_t& out_rd) -> bool {
            if (!s.has_commit || s.commit.kind != CommitRecord::Kind::REG) return false;
            out_rd = s.commit.rd;
            return out_rd != 0;
        };

        // ---- 1-back (EX/MEM forwarding path) ---
        const ExecStep& p1 = steps_.back();
        uint32_t p1_rd = 0;
        if (is_reg_writer(p1, p1_rd)) {
            bool uses_p1 = (cur_rs1 == p1_rd) || (cur_rs2 == p1_rd);
            if (uses_p1) {
                if (p1.kind == InstrKind::LW) {
                    step.load_use_next = true;
                    ++ldu_cnt_;
                    return;  // load-use dominates; no need to check 2-back
                } else {
                    step.raw_hazard = true;
                    ++raw_cnt_;
                    return;  // 1-back RAW dominates
                }
            }
        }

        // ---- 2-back (MEM/WB forwarding path) ---
        if (steps_.size() < 2) return;
        const ExecStep& p2 = steps_[steps_.size() - 2];
        uint32_t p2_rd = 0;
        if (!is_reg_writer(p2, p2_rd)) return;

        // Only annotate if p1 didn't already write to the same register.
        // If p1_rd == p2_rd the closer (p1) path wins and was already handled.
        if (p1_rd == p2_rd) return;

        bool uses_p2 = (cur_rs1 == p2_rd) || (cur_rs2 == p2_rd);
        if (uses_p2) {
            // LW 2-back: the load-use stall was already inserted for the 1-back
            // consumer; this 2-back consumer gets a regular MEM→EX forward.
            step.raw_hazard = true;
            ++raw_cnt_;
        }
    }
};
