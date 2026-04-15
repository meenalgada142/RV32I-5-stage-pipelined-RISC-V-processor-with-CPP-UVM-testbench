#pragma once
// pipe_coverage.h — Functional coverage model (24 required bins)
//
// Coverage is sampled from the reference model's ExecStep trace.
//
// Required bins (24 total — all must be hit for validate_coverage() == true):
//
//   Group A  — Instruction class (7): R_TYPE I_ALU LW SW BEQ BNE JAL
//   Group B  — Branch outcomes   (2): taken not_taken
//   Group C  — Hazard types      (3): RAW load_use control_transfer
//
//   Group G  — Cross: instruction × hazard (4 selected bins):
//     G1  R_TYPE   × RAW       — R-type consuming forwarded result
//     G2  BEQ      × RAW       — branch whose operand is forwarded
//     G3  BNE      × RAW       — branch whose operand is forwarded
//     G4  R_TYPE   × load_use  — R-type consumer after LW stall
//
//   Group H  — Cross: branch_outcome × hazard (2 selected bins):
//     H1  taken    × RAW       — taken branch on forwarded operands
//     H2  not_taken× RAW       — not-taken branch on forwarded operands
//
//   Group I  — Operand / immediate value classes (6):
//     I1  rs1 = 0x00000000 (ZERO)
//     I2  rs1 = 0xFFFFFFFF (NEG_ONE / all-ones)
//     I3  imm = 0          (zero immediate, I-type)
//     I4  imm = -1         (sign-extended all-ones immediate)
//     I5  imm = +2047      (maximum 12-bit signed positive)
//     I6  imm = -2048      (minimum 12-bit signed)
//
// Informational bins (tracked but do NOT gate validate_coverage):
//   Group D  — rd=x0 writes (architectural no-op)
//   Group E  — rs1=x0 or rs2=x0 source reads
//   Group F  — back-to-back same instruction class (pipeline stress)
//   Group GX — full 7×3 cross instr×hazard matrix (super-set of G)
//   Group HX — full 2×3 cross branch×hazard matrix (super-set of H)

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "pipe_transaction.h"
#include "reference_model.h"

class PipeCoverage {
public:
    // -----------------------------------------------------------------------
    void sample_trace(const std::vector<ExecStep>& steps) {
        for (const auto& s : steps) sample_step(s);
    }

    // -----------------------------------------------------------------------
    void sample_step(const ExecStep& s) {
        const int cls    = std::min((int)s.kind, (int)InstrKind::UNKNOWN);
        const int hslot  = s.load_use_next ? 2 : (s.raw_hazard ? 1 : 0);

        // Group A — instruction class
        if (cls >= 0 && cls < (int)g_instr_.size()) ++g_instr_[cls];

        // Group B — branch outcomes
        if (s.kind == InstrKind::BEQ || s.kind == InstrKind::BNE) {
            if (s.branch_taken) ++g_branch_taken_;
            else                ++g_branch_not_taken_;
        }

        // Group C — hazard types
        if (s.raw_hazard)    ++g_raw_;
        if (s.load_use_next) ++g_ldu_;
        if (s.kind == InstrKind::BEQ || s.kind == InstrKind::BNE ||
            s.kind == InstrKind::JAL)
            ++g_control_;

        // Group G/GX — cross instruction × hazard (full 9×3 matrix)
        if (cls < (int)g_cross_instr_hazard_.size())
            ++g_cross_instr_hazard_[cls][hslot];

        // Group H/HX — cross branch_outcome × hazard
        if (s.kind == InstrKind::BEQ || s.kind == InstrKind::BNE) {
            int bidx = s.branch_taken ? 1 : 0;
            ++g_cross_branch_hazard_[bidx][hslot];
        }

        // Group I — operand / immediate value classes
        sample_value_classes(s);

        // Group D — rd=x0
        const uint8_t rd = (s.instr >> 7) & 0x1Fu;
        if (rd == 0 && s.kind != InstrKind::NOP) ++g_rd_x0_;

        // Group E — rs1/rs2=x0
        if (((s.instr >> 15) & 0x1Fu) == 0) ++g_rs1_x0_;
        if (((s.instr >> 20) & 0x1Fu) == 0) ++g_rs2_x0_;

        // Group F — back-to-back same class
        if (prev_kind_ == s.kind && s.kind != InstrKind::NOP &&
            s.kind != InstrKind::UNKNOWN)
            ++g_b2b_same_;
        prev_kind_ = s.kind;
    }

    // -----------------------------------------------------------------------
    void reset() {
        g_instr_.fill(0);
        g_branch_taken_ = g_branch_not_taken_ = 0;
        g_raw_ = g_ldu_ = g_control_ = 0;
        for (auto& row : g_cross_instr_hazard_)  row.fill(0);
        for (auto& row : g_cross_branch_hazard_) row.fill(0);
        g_rs1_zero_ = g_rs1_neg1_ = 0;
        g_imm_zero_ = g_imm_neg1_ = g_imm_maxpos_ = g_imm_maxneg_ = 0;
        g_rd_x0_ = g_rs1_x0_ = g_rs2_x0_ = g_b2b_same_ = 0;
        prev_kind_ = InstrKind::UNKNOWN;
    }

    // -----------------------------------------------------------------------
    // Returns true when all 24 required bins have at least `threshold` hits.
    bool validate_coverage(int threshold = 1) const {
        // Group A — all 7 instruction classes
        for (int i = 0; i <= (int)InstrKind::JAL; ++i)
            if (g_instr_[i] < threshold) return false;

        // Group B — both branch outcomes
        if (g_branch_taken_     < threshold) return false;
        if (g_branch_not_taken_ < threshold) return false;

        // Group C — all 3 hazard types
        if (g_raw_     < threshold) return false;
        if (g_ldu_     < threshold) return false;
        if (g_control_ < threshold) return false;

        // Group G — 4 selected cross bins
        // G1: R_TYPE × RAW
        if (g_cross_instr_hazard_[(int)InstrKind::R_TYPE][1] < threshold) return false;
        // G2: BEQ × RAW
        if (g_cross_instr_hazard_[(int)InstrKind::BEQ][1] < threshold) return false;
        // G3: BNE × RAW
        if (g_cross_instr_hazard_[(int)InstrKind::BNE][1] < threshold) return false;
        // G4: R_TYPE × load_use
        if (g_cross_instr_hazard_[(int)InstrKind::R_TYPE][2] < threshold) return false;

        // Group H — 2 selected cross bins
        // H1: taken × RAW
        if (g_cross_branch_hazard_[1][1] < threshold) return false;
        // H2: not_taken × RAW
        if (g_cross_branch_hazard_[0][1] < threshold) return false;

        // Group I — 6 operand/immediate value bins
        if (g_rs1_zero_   < threshold) return false;
        if (g_rs1_neg1_   < threshold) return false;
        if (g_imm_zero_   < threshold) return false;
        if (g_imm_neg1_   < threshold) return false;
        if (g_imm_maxpos_ < threshold) return false;
        if (g_imm_maxneg_ < threshold) return false;

        return true;
    }

    // -----------------------------------------------------------------------
    std::string summary() const {
        int hit = 0, total = 0;
        auto chk = [&](int cnt, int thr = 1) { ++total; if (cnt >= thr) ++hit; };

        // A
        for (int i = 0; i <= (int)InstrKind::JAL; ++i) chk(g_instr_[i]);
        // B
        chk(g_branch_taken_);   chk(g_branch_not_taken_);
        // C
        chk(g_raw_); chk(g_ldu_); chk(g_control_);
        // G
        chk(g_cross_instr_hazard_[(int)InstrKind::R_TYPE][1]);
        chk(g_cross_instr_hazard_[(int)InstrKind::BEQ][1]);
        chk(g_cross_instr_hazard_[(int)InstrKind::BNE][1]);
        chk(g_cross_instr_hazard_[(int)InstrKind::R_TYPE][2]);
        // H
        chk(g_cross_branch_hazard_[1][1]);
        chk(g_cross_branch_hazard_[0][1]);
        // I
        chk(g_rs1_zero_); chk(g_rs1_neg1_);
        chk(g_imm_zero_); chk(g_imm_neg1_);
        chk(g_imm_maxpos_); chk(g_imm_maxneg_);

        std::ostringstream ss;
        ss << hit << "/" << total << " bins hit ("
           << std::fixed << std::setprecision(1)
           << (total ? 100.0 * hit / total : 0.0) << "%)";
        return ss.str();
    }

    // -----------------------------------------------------------------------
    std::string report() const {
        static const char* kKind[] = {
            "R-type","I-ALU","LW","SW","BEQ","BNE","JAL","NOP","UNKNOWN"
        };
        static const char* kHaz[] = { "none", "RAW", "load_use" };
        std::ostringstream ss;
        ss << "=== Pipeline Coverage Report ===\n";

        ss << "\nGroup A — Instruction classes:\n";
        for (int i = 0; i < 9; ++i)
            ss << "  " << std::setw(8) << kKind[i] << ": " << g_instr_[i]
               << (i <= (int)InstrKind::JAL && g_instr_[i] == 0 ? "  *** UNCOVERED ***" : "") << "\n";

        ss << "\nGroup B — Branch outcomes:\n";
        ss << "  taken:     " << g_branch_taken_     << (g_branch_taken_     == 0 ? "  *** UNCOVERED ***" : "") << "\n";
        ss << "  not_taken: " << g_branch_not_taken_ << (g_branch_not_taken_ == 0 ? "  *** UNCOVERED ***" : "") << "\n";

        ss << "\nGroup C — Hazard types:\n";
        ss << "  RAW fwd:      " << g_raw_     << (g_raw_     == 0 ? "  *** UNCOVERED ***" : "") << "\n";
        ss << "  load-use stall:" << g_ldu_    << (g_ldu_     == 0 ? "  *** UNCOVERED ***" : "") << "\n";
        ss << "  control xfer: " << g_control_ << (g_control_ == 0 ? "  *** UNCOVERED ***" : "") << "\n";

        ss << "\nGroup G — Cross: instruction × hazard (required bins marked *):\n";
        ss << "  " << std::setw(10) << "" ;
        for (int h = 0; h < 3; ++h) ss << " | " << std::setw(9) << kHaz[h];
        ss << "\n";
        for (int i = 0; i < 9; ++i) {
            ss << "  " << std::setw(10) << kKind[i];
            for (int h = 0; h < 3; ++h) {
                bool required = (i == (int)InstrKind::R_TYPE && (h == 1 || h == 2)) ||
                                (i == (int)InstrKind::BEQ    && h == 1) ||
                                (i == (int)InstrKind::BNE    && h == 1);
                ss << " | " << std::setw(8) << g_cross_instr_hazard_[i][h]
                   << (required ? "*" : " ");
            }
            ss << "\n";
        }

        ss << "\nGroup H — Cross: branch_outcome × hazard (required bins marked *):\n";
        ss << "  " << std::setw(10) << "";
        for (int h = 0; h < 3; ++h) ss << " | " << std::setw(9) << kHaz[h];
        ss << "\n";
        static const char* kOut[] = { "not_taken", "taken" };
        for (int b = 0; b < 2; ++b) {
            ss << "  " << std::setw(10) << kOut[b];
            for (int h = 0; h < 3; ++h) {
                bool required = (h == 1);  // × RAW required for both outcomes
                ss << " | " << std::setw(8) << g_cross_branch_hazard_[b][h]
                   << (required ? "*" : " ");
            }
            ss << "\n";
        }

        ss << "\nGroup I — Operand / immediate value classes (required bins marked *):\n";
        ss << "  rs1 = ZERO    (0x00000000): " << g_rs1_zero_   << "*\n";
        ss << "  rs1 = NEG_ONE (0xFFFFFFFF): " << g_rs1_neg1_   << "*\n";
        ss << "  imm = 0:                    " << g_imm_zero_   << "*\n";
        ss << "  imm = -1:                   " << g_imm_neg1_   << "*\n";
        ss << "  imm = +2047 (max pos):      " << g_imm_maxpos_ << "*\n";
        ss << "  imm = -2048 (min neg):      " << g_imm_maxneg_ << "*\n";

        ss << "\nGroup D/E/F — Informational:\n";
        ss << "  rd=x0 writes:          " << g_rd_x0_   << "\n";
        ss << "  rs1=x0 reads:          " << g_rs1_x0_  << "\n";
        ss << "  rs2=x0 reads:          " << g_rs2_x0_  << "\n";
        ss << "  back-to-back same cls: " << g_b2b_same_ << "\n";

        ss << "\nSummary: " << summary() << "\n";
        return ss.str();
    }

private:
    // Group A (9 entries, one per InstrKind)
    std::array<int, 9>            g_instr_              = {};
    // Group B
    int g_branch_taken_     = 0;
    int g_branch_not_taken_ = 0;
    // Group C
    int g_raw_     = 0;
    int g_ldu_     = 0;
    int g_control_ = 0;
    // Group GX: [instr_kind][hazard_slot]  hazard: 0=none,1=RAW,2=load_use
    std::array<std::array<int,3>, 9> g_cross_instr_hazard_  = {};
    // Group HX: [branch_outcome][hazard_slot]  outcome: 0=not_taken,1=taken
    std::array<std::array<int,3>, 2> g_cross_branch_hazard_ = {};
    // Group I
    int g_rs1_zero_   = 0;
    int g_rs1_neg1_   = 0;
    int g_imm_zero_   = 0;
    int g_imm_neg1_   = 0;
    int g_imm_maxpos_ = 0;
    int g_imm_maxneg_ = 0;
    // Groups D/E/F
    int g_rd_x0_    = 0;
    int g_rs1_x0_   = 0;
    int g_rs2_x0_   = 0;
    int g_b2b_same_ = 0;
    // State for Group F
    InstrKind prev_kind_ = InstrKind::UNKNOWN;

    // -----------------------------------------------------------------------
    void sample_value_classes(const ExecStep& s) {
        // rs1 value class (applies to all instruction types)
        if (s.rs1_val == 0x00000000u) ++g_rs1_zero_;
        if (s.rs1_val == 0xFFFFFFFFu) ++g_rs1_neg1_;

        // imm value class — only meaningful for I/S/B/J types
        if (s.kind == InstrKind::I_ALU || s.kind == InstrKind::LW ||
            s.kind == InstrKind::SW    || s.kind == InstrKind::BEQ ||
            s.kind == InstrKind::BNE   || s.kind == InstrKind::JAL) {
            if (s.imm_val ==     0) ++g_imm_zero_;
            if (s.imm_val ==    -1) ++g_imm_neg1_;
            if (s.imm_val ==  2047) ++g_imm_maxpos_;
            if (s.imm_val == -2048) ++g_imm_maxneg_;
        }
    }
};
