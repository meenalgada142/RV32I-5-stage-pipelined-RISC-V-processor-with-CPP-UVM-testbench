#pragma once
// decoder_coverage.h — Deep functional coverage model (v2)
//
// Why v1 was misleading:
//   - InstrClass bins were 1:1 with pre-classified valid instructions.
//     The random generator always produced valid instructions, so closure
//     was trivial.  No illegal encodings.  No funct3/funct7 cross.
//     No immediate boundary stress.  No control-signal combination check.
//
// What v2 adds:
//   Group A — opcode × funct3 cross (per legal opcode)
//   Group B — R-type funct3 × funct7_class (catches wrong SUB/SRA decode)
//   Group C — I-type shift funct7_class (SRLI vs SRAI distinction)
//   Group D — Immediate boundary per instruction class (NEG_MIN/NEG_ONE/ZERO/POS_ONE/POS_MAX)
//   Group E — Register-zero cross per instruction class (rs1, rs2, rd each = x0)
//   Group F — Illegal encoding bins (illegal opcode, wrong funct7, wrong funct3)
//   Group G — Control-signal output combination (self-checking cross)
//
// Closure rule:
//   Groups A–E: every reachable bin ≥ threshold hits  (mandatory)
//   Group F: every illegal bucket ≥ 1 hit             (mandatory — must test defaults)
//   Group G: every valid control pattern ≥ 1 hit      (mandatory)

#include <array>
#include <iomanip>
#include <map>
#include <sstream>
#include <bitset>
#include <string>
#include <vector>
#include <algorithm>

#include "decoder_golden.h"
#include "decoder_transaction.h"

// ============================================================
//  Funct7 class — only the one bit that matters for decode
// ============================================================
enum class Funct7Class : int { ZERO = 0, ALT_20 = 1, OTHER = 2 };
static const int kNumF7Classes = 3;

static Funct7Class classify_funct7(uint8_t f7) {
    if (f7 == 0x00) return Funct7Class::ZERO;
    if (f7 == 0x20) return Funct7Class::ALT_20;
    return Funct7Class::OTHER;
}
static const char* f7_class_name(Funct7Class c) {
    static const char* n[] = {"f7=0x00", "f7=0x20", "f7=OTHER"};
    return n[static_cast<int>(c)];
}

// ============================================================
//  Immediate boundary bins (finer than v1)
// ============================================================
enum class ImmBoundary : int {
    NEG_MIN = 0,   // exactly the minimum value  (-2048 / -4096 / etc.)
    NEG_GENERAL,   // any other negative except -1
    NEG_ONE,       // exactly -1  (0xFFFFFFFF)
    ZERO,          // exactly 0
    POS_ONE,       // exactly +1
    POS_GENERAL,   // any positive except max
    POS_MAX,       // exactly the maximum value (+2047 / +4094 / etc.)
    NUM_IMM_BOUNDS
};
static const int kNumImmBounds = static_cast<int>(ImmBoundary::NUM_IMM_BOUNDS);
static const char* imm_bound_name(ImmBoundary b) {
    static const char* n[] = {"NEG_MIN","NEG_GENERAL","NEG_ONE","ZERO","POS_ONE","POS_GENERAL","POS_MAX"};
    return n[static_cast<int>(b)];
}

static ImmBoundary classify_imm_boundary(int32_t signed_imm, int32_t imm_min, int32_t imm_max) {
    if (signed_imm == imm_min) return ImmBoundary::NEG_MIN;
    if (signed_imm == -1)      return ImmBoundary::NEG_ONE;
    if (signed_imm < 0)        return ImmBoundary::NEG_GENERAL;
    if (signed_imm == 0)       return ImmBoundary::ZERO;
    if (signed_imm == 1)       return ImmBoundary::POS_ONE;
    if (signed_imm == imm_max) return ImmBoundary::POS_MAX;
    return ImmBoundary::POS_GENERAL;
}

// Immediate range per instruction class
static std::pair<int32_t,int32_t> imm_range(InstrClass cls) {
    if (cls == InstrClass::BRANCH_BEQ || cls == InstrClass::BRANCH_BNE)
        return {-4096, 4094};  // 13-bit signed, multiples of 2
    if (cls == InstrClass::JUMP_JAL)
        return {-1048576, 1048574};  // 21-bit signed, multiples of 2
    if (cls == InstrClass::I_SLLI || cls == InstrClass::I_SRLI)
        return {0, 31};   // shamt only
    if (cls == InstrClass::I_SRAI)
        return {1024, 1055};  // 0x400 | shamt — not a "boundary" in human terms
    return {-2048, 2047};  // standard 12-bit signed
}

// ============================================================
//  Illegal encoding categories (Group F)
// ============================================================
enum class IllegalKind : int {
    UNKNOWN_OPCODE = 0,   // opcode ∉ {0x33,0x13,0x03,0x23,0x63,0x6F}
    RTYPE_BAD_F7,         // opcode=0x33 with funct7 ∉ {0x00, 0x20}
    RTYPE_RESERVED_F3F7,  // opcode=0x33 but (funct3,funct7) combo has no defined meaning
    ISHIFT_BAD_F7,        // opcode=0x13, funct3=1 or 5, funct7[6:2] ≠ 0
    LOAD_BAD_F3,          // opcode=0x03, funct3 ≠ 2
    BRANCH_BAD_F3,        // opcode=0x63, funct3 ∉ {0,1}
    NUM_ILLEGAL_KINDS
};
static const int kNumIllegalKinds = static_cast<int>(IllegalKind::NUM_ILLEGAL_KINDS);
static const char* illegal_kind_name(IllegalKind k) {
    static const char* n[] = {
        "UNKNOWN_OPCODE","RTYPE_BAD_F7","RTYPE_RESERVED_F3F7",
        "ISHIFT_BAD_F7","LOAD_BAD_F3","BRANCH_BAD_F3"
    };
    return n[static_cast<int>(k)];
}

// ============================================================
//  Control-signal output bitmask
//  bit0=reg_write  bit1=alu_src  bit2=mem_read  bit3=mem_write
//  bit4=mem_to_reg bit5=branch   bit6=jump
// ============================================================
static uint8_t ctrl_sig_mask(const DecoderTransaction& tx) {
    return uint8_t(
        (tx.exp_reg_write  ? 0x01 : 0) |
        (tx.exp_alu_src    ? 0x02 : 0) |
        (tx.exp_mem_read   ? 0x04 : 0) |
        (tx.exp_mem_write  ? 0x08 : 0) |
        (tx.exp_mem_to_reg ? 0x10 : 0) |
        (tx.exp_branch     ? 0x20 : 0) |
        (tx.exp_jump       ? 0x40 : 0));
}
// Expected valid control patterns (one per instruction class, derived from RTL)
//   bit0=reg_write  bit1=alu_src  bit2=mem_read  bit3=mem_write
//   bit4=mem_to_reg bit5=branch   bit6=jump
static const std::array<uint8_t,6> kValidCtrlPatterns = {
    0x01, // R-type:     reg_write=1, alu_src=0
    0x03, // I-type ALU: reg_write=1, alu_src=1
    0x17, // LW:         reg_write=1, alu_src=1, mem_read=1, mem_to_reg=1
    0x0A, // SW:         alu_src=1, mem_write=1
    0x20, // BEQ/BNE:    branch=1
    0x43, // JAL:        reg_write=1, alu_src=1, jump=1
};

// ============================================================
//  Main coverage class
// ============================================================
class DecoderCoverage {
public:

    void sample(const DecoderTransaction& tx) {
        const uint32_t instr  = tx.instr;
        const uint8_t  opcode = instr & 0x7Fu;
        const uint8_t  funct3 = (instr >> 12) & 0x7u;
        const uint8_t  funct7 = (instr >> 25) & 0x7Fu;
        const Funct7Class f7c = classify_funct7(funct7);

        InstrClass cls = classify_instr(tx);

        // ---- Group A: opcode × funct3 ----
        switch (opcode) {
            case 0x33: rtype_f3_[funct3]++;  break;
            case 0x13: itype_f3_[funct3]++;  break;
            case 0x03: load_f3_[funct3]++;   break;
            case 0x23: store_f3_[funct3]++;  break;
            case 0x63: branch_f3_[funct3]++; break;
            case 0x6F: jal_hit_++;           break;
            default:   illegal_hits_[static_cast<int>(IllegalKind::UNKNOWN_OPCODE)]++; break;
        }

        // ---- Group B: R-type funct3 × funct7_class ----
        if (opcode == 0x33) {
            rtype_f3_f7_[funct3][static_cast<int>(f7c)]++;
            if (f7c == Funct7Class::OTHER)
                illegal_hits_[static_cast<int>(IllegalKind::RTYPE_BAD_F7)]++;
            // Check specifically reserved (funct3, funct7) combos
            bool reserved = false;
            if (f7c == Funct7Class::ALT_20 && funct3 != 0 && funct3 != 5) reserved = true;
            if (reserved)
                illegal_hits_[static_cast<int>(IllegalKind::RTYPE_RESERVED_F3F7)]++;
        }

        // ---- Group C: I-type shift funct7_class ----
        if (opcode == 0x13 && (funct3 == 1 || funct3 == 5)) {
            int shift_idx = (funct3 == 5) ? 1 : 0;
            itype_shift_f7_[shift_idx][static_cast<int>(f7c)]++;
            // funct7[6:2] must be 0 for valid SRLI/SRAI; only funct7[5] distinguishes them
            if ((funct7 & 0x7Du) != 0)  // bits 6,4,3,2,1,0 must be 0
                illegal_hits_[static_cast<int>(IllegalKind::ISHIFT_BAD_F7)]++;
        }

        // ---- Group F (more illegal kinds) ----
        if (opcode == 0x03 && funct3 != 2)
            illegal_hits_[static_cast<int>(IllegalKind::LOAD_BAD_F3)]++;
        if (opcode == 0x63 && funct3 > 1)
            illegal_hits_[static_cast<int>(IllegalKind::BRANCH_BAD_F3)]++;

        // ---- Group D: immediate boundary ----
        if (cls != InstrClass::NUM_CLASSES) {
            int ci = static_cast<int>(cls);
            auto [imin, imax] = imm_range(cls);
            bool has_imm = !(cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA);
            if (has_imm) {
                int32_t sv = static_cast<int32_t>(tx.exp_imm);
                ImmBoundary ib = classify_imm_boundary(sv, imin, imax);
                imm_boundary_[ci][static_cast<int>(ib)]++;
            }
        }

        // ---- Group E: register zero cross ----
        if (cls != InstrClass::NUM_CLASSES) {
            int ci = static_cast<int>(cls);
            if (tx.exp_rs1 == 0) rs1_zero_[ci]++;
            if (tx.exp_rs2 == 0) rs2_zero_[ci]++;
            if (tx.exp_rd  == 0) rd_zero_[ci]++;
        }

        // ---- Group G: control signal combination ----
        ctrl_combos_[ctrl_sig_mask(tx)]++;
    }

    // ---- Closure check ----
    bool validate_coverage(int threshold = 10) const {
        // Group A — opcode × funct3 (legal bins only)
        for (int f3 = 0; f3 < 8; ++f3) {
            if (rtype_f3_[f3]   < threshold) return false;
            if (itype_f3_[f3]   < threshold) return false;
            if (branch_f3_[f3 < 2 ? f3 : 0] < threshold && f3 < 2) return false;
        }
        if (load_f3_[2]   < threshold) return false;
        if (store_f3_[2]  < threshold) return false;
        if (branch_f3_[0] < threshold) return false;
        if (branch_f3_[1] < threshold) return false;
        if (jal_hit_      < threshold) return false;

        // Group B — R-type funct3 × funct7 (legal combos only)
        for (int f3 = 0; f3 < 8; ++f3)
            if (is_rtype_legal(f3, Funct7Class::ZERO) &&
                rtype_f3_f7_[f3][static_cast<int>(Funct7Class::ZERO)] < threshold)
                return false;
        // SUB and SRA require ALT_20
        if (rtype_f3_f7_[0][static_cast<int>(Funct7Class::ALT_20)] < threshold) return false;
        if (rtype_f3_f7_[5][static_cast<int>(Funct7Class::ALT_20)] < threshold) return false;

        // Group C — I-type shift funct7 (SRLI and SRAI)
        if (itype_shift_f7_[1][static_cast<int>(Funct7Class::ZERO)]    < threshold) return false; // SRLI
        if (itype_shift_f7_[1][static_cast<int>(Funct7Class::ALT_20)]  < threshold) return false; // SRAI

        // Group D — immediate boundary (mandatory: NEG_MIN, NEG_ONE, ZERO, POS_ONE, POS_MAX)
        for (int ci = 0; ci < kNumInstrClasses; ++ci) {
            auto cls = static_cast<InstrClass>(ci);
            if (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA) continue;
            if (cls == InstrClass::I_SRAI) continue; // imm not user-controlled
            for (auto ib : {ImmBoundary::ZERO, ImmBoundary::POS_ONE, ImmBoundary::NEG_ONE}) {
                if (!is_imm_boundary_reachable(cls, ib)) continue;
                if (imm_boundary_[ci][static_cast<int>(ib)] < threshold) return false;
            }
            for (auto ib : {ImmBoundary::NEG_MIN, ImmBoundary::POS_MAX}) {
                if (!is_imm_boundary_reachable(cls, ib)) continue;
                if (imm_boundary_[ci][static_cast<int>(ib)] < 1) return false; // boundary: 1 hit sufficient
            }
        }

        // Group E — register zero (each rs1=x0 / rs2=x0 / rd=x0 ≥ 1 hit)
        for (int ci = 0; ci < kNumInstrClasses; ++ci) {
            auto cls = static_cast<InstrClass>(ci);
            if (cls == InstrClass::NUM_CLASSES) continue;
            if (rs1_zero_[ci] < 1) return false;
            bool has_rs2 = (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA)
                        || cls == InstrClass::STORE_SW
                        || cls == InstrClass::BRANCH_BEQ
                        || cls == InstrClass::BRANCH_BNE;
            if (has_rs2 && rs2_zero_[ci] < 1) return false;
            bool has_rd = !(cls == InstrClass::STORE_SW ||
                            cls == InstrClass::BRANCH_BEQ ||
                            cls == InstrClass::BRANCH_BNE);
            if (has_rd && rd_zero_[ci] < 1) return false;
        }

        // Group F — illegal bins (at least 1 hit each — prove default is tested)
        for (int k = 0; k < kNumIllegalKinds; ++k)
            if (illegal_hits_[k] < 1) return false;

        // Group G — all valid control patterns seen
        for (uint8_t p : kValidCtrlPatterns)
            if (ctrl_combos_.count(p) == 0 || ctrl_combos_.at(p) < 1) return false;

        return true;
    }

    // ---- Summary ----
    std::string summary() const {
        int total = 0, hit = 0;
        count_bins(total, hit, 10);
        float pct = total ? 100.f * hit / total : 0.f;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << pct
            << "% (" << hit << "/" << total << " bins)";
        return oss.str();
    }

    // ---- Detailed report ----
    std::string report() const {
        std::ostringstream oss;
        oss << "=== Decoder Coverage Report (v2) ===\n";

        oss << "\n[A] opcode × funct3\n";
        oss << "  R-type  funct3: ";
        for (int f=0;f<8;++f) oss << "f3=" << f << ":" << rtype_f3_[f] << " ";
        oss << "\n  I-type  funct3: ";
        for (int f=0;f<8;++f) oss << "f3=" << f << ":" << itype_f3_[f] << " ";
        oss << "\n  Load    f3=2=" << load_f3_[2];
        oss << "  Store   f3=2=" << store_f3_[2];
        oss << "  Branch  f3=0=" << branch_f3_[0] << " f3=1=" << branch_f3_[1];
        oss << "  JAL=" << jal_hit_ << "\n";

        oss << "\n[B] R-type funct3 × funct7\n";
        for (int f3=0;f3<8;++f3) {
            oss << "  f3=" << f3 << ": ";
            for (int fc=0;fc<kNumF7Classes;++fc)
                oss << f7_class_name(static_cast<Funct7Class>(fc))
                    << "=" << rtype_f3_f7_[f3][fc] << " ";
            oss << "\n";
        }

        oss << "\n[C] I-type shift funct7\n";
        for (int si=0;si<2;++si) {
            oss << "  f3=" << (si==0?1:5) << "(";
            oss << (si==0?"SLLI/SLLI":"SRLI/SRAI") << "): ";
            for (int fc=0;fc<kNumF7Classes;++fc)
                oss << f7_class_name(static_cast<Funct7Class>(fc))
                    << "=" << itype_shift_f7_[si][fc] << " ";
            oss << "\n";
        }

        oss << "\n[D] Immediate boundary\n";
        for (int ci=0;ci<kNumInstrClasses;++ci) {
            auto cls = static_cast<InstrClass>(ci);
            if (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA) continue;
            oss << "  " << std::left << std::setw(12) << instr_class_name(cls);
            for (int ib=0;ib<kNumImmBounds;++ib) {
                if (!is_imm_boundary_reachable(cls, static_cast<ImmBoundary>(ib))) continue;
                oss << imm_bound_name(static_cast<ImmBoundary>(ib))
                    << "=" << imm_boundary_[ci][ib] << " ";
            }
            oss << "\n";
        }

        oss << "\n[E] Register zero patterns\n";
        for (int ci=0;ci<kNumInstrClasses;++ci) {
            auto cls = static_cast<InstrClass>(ci);
            oss << "  " << std::left << std::setw(12) << instr_class_name(cls)
                << " rs1_zero=" << rs1_zero_[ci]
                << " rs2_zero=" << rs2_zero_[ci]
                << " rd_zero="  << rd_zero_[ci] << "\n";
        }

        oss << "\n[F] Illegal encoding hits\n";
        for (int k=0;k<kNumIllegalKinds;++k)
            oss << "  " << std::left << std::setw(24)
                << illegal_kind_name(static_cast<IllegalKind>(k))
                << " = " << illegal_hits_[k] << "\n";

        oss << "\n[G] Control signal combos seen: " << ctrl_combos_.size() << "\n";
        for (auto& [mask, cnt] : ctrl_combos_)
            oss << "  0b" << std::bitset<7>(mask) << " = " << cnt << "\n";

        oss << "\nSummary: " << summary() << "\n";
        return oss.str();
    }

    // Weak bins for coverage-driven engine
    struct WeakBin {
        enum class Kind { RTYPE_F3, RTYPE_F3_F7, ITYPE_F3, ITYPE_SHIFT_F7,
                          LOAD_F3, STORE_F3, BRANCH_F3, JAL,
                          IMM_BOUNDARY, REG_ZERO, ILLEGAL } kind;
        InstrClass  cls  = InstrClass::NUM_CLASSES;
        int         f3   = -1;
        Funct7Class f7c  = Funct7Class::ZERO;
        ImmBoundary imm_b= ImmBoundary::ZERO;
        int         hits = 0;
    };

    std::vector<WeakBin> get_weak_bins(int threshold = 10) const {
        std::vector<WeakBin> out;

        // Group A: opcode × funct3
        for (int f3=0;f3<8;++f3) {
            if (rtype_f3_[f3]<threshold)
                out.push_back({WeakBin::Kind::RTYPE_F3,
                    static_cast<InstrClass>(static_cast<int>(InstrClass::R_ADD)+f3),
                    f3,Funct7Class::ZERO,ImmBoundary::ZERO,rtype_f3_[f3]});
            if (itype_f3_[f3]<threshold)
                out.push_back({WeakBin::Kind::ITYPE_F3,
                    InstrClass::I_ADDI,f3,Funct7Class::ZERO,ImmBoundary::ZERO,itype_f3_[f3]});
        }
        if (branch_f3_[0]<threshold) out.push_back({WeakBin::Kind::BRANCH_F3,InstrClass::BRANCH_BEQ,0,Funct7Class::ZERO,ImmBoundary::ZERO,branch_f3_[0]});
        if (branch_f3_[1]<threshold) out.push_back({WeakBin::Kind::BRANCH_F3,InstrClass::BRANCH_BNE,1,Funct7Class::ZERO,ImmBoundary::ZERO,branch_f3_[1]});
        if (load_f3_[2] <threshold)  out.push_back({WeakBin::Kind::LOAD_F3, InstrClass::LOAD_LW,  2,Funct7Class::ZERO,ImmBoundary::ZERO,load_f3_[2]});
        if (store_f3_[2]<threshold)  out.push_back({WeakBin::Kind::STORE_F3,InstrClass::STORE_SW, 2,Funct7Class::ZERO,ImmBoundary::ZERO,store_f3_[2]});
        if (jal_hit_    <threshold)  out.push_back({WeakBin::Kind::JAL,     InstrClass::JUMP_JAL,-1,Funct7Class::ZERO,ImmBoundary::ZERO,jal_hit_});

        // Group B: R-type funct3 × funct7
        for (int f3=0;f3<8;++f3)
            for (int fc=0;fc<kNumF7Classes;++fc) {
                auto f7c = static_cast<Funct7Class>(fc);
                if (!is_rtype_legal(f3,f7c)) continue;
                if (rtype_f3_f7_[f3][fc]<threshold)
                    out.push_back({WeakBin::Kind::RTYPE_F3_F7,InstrClass::NUM_CLASSES,f3,f7c,ImmBoundary::ZERO,rtype_f3_f7_[f3][fc]});
            }

        // Group C: shift funct7
        if (itype_shift_f7_[1][static_cast<int>(Funct7Class::ZERO)]   <threshold)
            out.push_back({WeakBin::Kind::ITYPE_SHIFT_F7,InstrClass::I_SRLI,-1,Funct7Class::ZERO,ImmBoundary::ZERO,itype_shift_f7_[1][0]});
        if (itype_shift_f7_[1][static_cast<int>(Funct7Class::ALT_20)] <threshold)
            out.push_back({WeakBin::Kind::ITYPE_SHIFT_F7,InstrClass::I_SRAI,-1,Funct7Class::ALT_20,ImmBoundary::ZERO,itype_shift_f7_[1][1]});

        // Group D: imm boundary
        for (int ci=0;ci<kNumInstrClasses;++ci) {
            auto cls = static_cast<InstrClass>(ci);
            if (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA) continue;
            if (cls == InstrClass::I_SRAI) continue;
            for (int ib=0;ib<kNumImmBounds;++ib) {
                auto ibc = static_cast<ImmBoundary>(ib);
                if (!is_imm_boundary_reachable(cls,ibc)) continue;
                int need = (ibc==ImmBoundary::NEG_MIN||ibc==ImmBoundary::POS_MAX) ? 1 : threshold;
                if (imm_boundary_[ci][ib]<need)
                    out.push_back({WeakBin::Kind::IMM_BOUNDARY,cls,-1,Funct7Class::ZERO,ibc,imm_boundary_[ci][ib]});
            }
        }

        std::sort(out.begin(),out.end(),[](const WeakBin&a,const WeakBin&b){return a.hits<b.hits;});
        return out;
    }

    const std::array<int,kNumIllegalKinds>& illegal_hits() const { return illegal_hits_; }

private:
    static bool is_rtype_legal(int f3, Funct7Class f7c) {
        if (f7c == Funct7Class::OTHER) return false;
        if (f7c == Funct7Class::ALT_20 && f3 != 0 && f3 != 5) return false;
        return true;
    }

    static bool is_imm_boundary_reachable(InstrClass cls, ImmBoundary ib) {
        if (cls >= InstrClass::R_ADD && cls <= InstrClass::R_SRA) return false;

        // Shift immediate is a shamt (0..31).
        //   shamt=0 → classify_imm_boundary(0, imin=0, imax=31) returns NEG_MIN (not ZERO),
        //   so the ZERO bin is structurally unreachable.
        //   Negative shamts are impossible, so NEG_GENERAL and NEG_ONE are also unreachable.
        //   NEG_MIN (=shamt 0), POS_ONE (=shamt 1), POS_GENERAL, POS_MAX are all reachable.
        if (cls == InstrClass::I_SLLI || cls == InstrClass::I_SRLI) {
            if (ib == ImmBoundary::ZERO)        return false;
            if (ib == ImmBoundary::NEG_GENERAL) return false;
            if (ib == ImmBoundary::NEG_ONE)     return false;
            return true;
        }

        // Branch and JAL immediates always have bit[0] = 0 (alignment constraint).
        // ±1 can never appear as a decoded immediate value.
        if (cls == InstrClass::BRANCH_BEQ || cls == InstrClass::BRANCH_BNE ||
            cls == InstrClass::JUMP_JAL) {
            if (ib == ImmBoundary::NEG_ONE) return false;
            if (ib == ImmBoundary::POS_ONE) return false;
        }

        return true;
    }

    void count_bins(int& total, int& hit, int threshold) const {
        total=0; hit=0;
        // A: opcode × funct3 (legal bins only)
        for (int f3=0;f3<8;++f3) { total++; if(rtype_f3_[f3]>=threshold) hit++; }
        for (int f3=0;f3<8;++f3) { total++; if(itype_f3_[f3]>=threshold) hit++; }
        { total++; if(load_f3_[2]>=threshold)  hit++; }
        { total++; if(store_f3_[2]>=threshold) hit++; }
        { total++; if(branch_f3_[0]>=threshold)hit++; }
        { total++; if(branch_f3_[1]>=threshold)hit++; }
        { total++; if(jal_hit_>=threshold)      hit++; }
        // B: R-type funct3 × funct7 legal combos
        for (int f3=0;f3<8;++f3)
            for (int fc=0;fc<kNumF7Classes;++fc) {
                auto f7c=static_cast<Funct7Class>(fc);
                if (!is_rtype_legal(f3,f7c)) continue;
                total++; if(rtype_f3_f7_[f3][fc]>=threshold) hit++;
            }
        // C: shift f7
        { total++; if(itype_shift_f7_[1][0]>=threshold) hit++; } // SRLI
        { total++; if(itype_shift_f7_[1][1]>=threshold) hit++; } // SRAI
        // D: imm boundary
        for (int ci=0;ci<kNumInstrClasses;++ci) {
            auto cls=static_cast<InstrClass>(ci);
            if (cls>=InstrClass::R_ADD&&cls<=InstrClass::R_SRA) continue;
            if (cls==InstrClass::I_SRAI) continue;
            for (int ib=0;ib<kNumImmBounds;++ib) {
                auto ibc=static_cast<ImmBoundary>(ib);
                if (!is_imm_boundary_reachable(cls,ibc)) continue;
                int need=(ibc==ImmBoundary::NEG_MIN||ibc==ImmBoundary::POS_MAX)?1:threshold;
                total++; if(imm_boundary_[ci][ib]>=need) hit++;
            }
        }
        // E: reg zero (one bin per field per class)
        for (int ci=0;ci<kNumInstrClasses;++ci) {
            total++;if(rs1_zero_[ci]>=1)hit++;
            total++;if(rd_zero_[ci]>=1)hit++;
        }
        // F: illegal bins
        for (int k=0;k<kNumIllegalKinds;++k){total++;if(illegal_hits_[k]>=1)hit++;}
        // G: control signal combos
        for (uint8_t p : kValidCtrlPatterns){total++;if(ctrl_combos_.count(p)&&ctrl_combos_.at(p)>=1)hit++;}
    }

    // Group A
    std::array<int,8> rtype_f3_   = {};
    std::array<int,8> itype_f3_   = {};
    std::array<int,8> load_f3_    = {};
    std::array<int,8> store_f3_   = {};
    std::array<int,8> branch_f3_  = {};
    int               jal_hit_    = 0;
    // Group B: [funct3][funct7_class]
    std::array<std::array<int,kNumF7Classes>,8> rtype_f3_f7_ = {};
    // Group C: [shift_idx: 0=f3=1, 1=f3=5][funct7_class]
    std::array<std::array<int,kNumF7Classes>,2> itype_shift_f7_ = {};
    // Group D: [instr_class][imm_boundary]
    std::array<std::array<int,kNumImmBounds>,kNumInstrClasses> imm_boundary_ = {};
    // Group E: per instr_class
    std::array<int,kNumInstrClasses> rs1_zero_ = {};
    std::array<int,kNumInstrClasses> rs2_zero_ = {};
    std::array<int,kNumInstrClasses> rd_zero_  = {};
    // Group F
    std::array<int,kNumIllegalKinds> illegal_hits_ = {};
    // Group G
    std::map<uint8_t,int> ctrl_combos_;
};

// Include bitset for report()
#include <bitset>
