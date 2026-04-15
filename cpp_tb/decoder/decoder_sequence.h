#pragma once
// decoder_sequence.h — Sequence library (v2)
//
// New sequences added to break random dependency and enable true coverage closure:
//
//   BiasedRandomSequence      — 80% ADD/ADDI/LW only; proves random alone cannot close
//   IllegalEncodingSequence   — explicitly generates all illegal encoding kinds
//   BoundaryImmSequence       — sweeps every boundary value per instruction class
//   RegisterZeroSequence      — ensures every rs1/rs2/rd=x0 combination is hit
//   RTypeFunct7Sequence       — exhausts the funct3×funct7 cross for R-type
//   CoverageDrivenSequence    — feedback loop targeting the v2 WeakBin list

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "decoder_golden.h"
#include "decoder_coverage.h"
#include "decoder_transaction.h"

// ---------------------------------------------------------------------------
// Base
// ---------------------------------------------------------------------------
class DecoderSequence {
public:
    virtual ~DecoderSequence() = default;
    virtual std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const = 0;
};

// ---------------------------------------------------------------------------
// Helpers (carried over from v1 + new ones)
// ---------------------------------------------------------------------------
namespace detail {

inline DecoderTransaction make_r(std::mt19937_64& rng, InstrClass cls) {
    uint8_t rs1=random_reg(rng), rs2=random_reg(rng), rd=random_reg(rng);
    uint32_t instr=0;
    switch(cls){
        case InstrClass::R_ADD:  instr=encode::r(0x00,rs2,rs1,0,rd); break;
        case InstrClass::R_SUB:  instr=encode::r(0x20,rs2,rs1,0,rd); break;
        case InstrClass::R_AND:  instr=encode::r(0x00,rs2,rs1,7,rd); break;
        case InstrClass::R_OR:   instr=encode::r(0x00,rs2,rs1,6,rd); break;
        case InstrClass::R_XOR:  instr=encode::r(0x00,rs2,rs1,4,rd); break;
        case InstrClass::R_SLT:  instr=encode::r(0x00,rs2,rs1,2,rd); break;
        case InstrClass::R_SLTU: instr=encode::r(0x00,rs2,rs1,3,rd); break;
        case InstrClass::R_SLL:  instr=encode::r(0x00,rs2,rs1,1,rd); break;
        case InstrClass::R_SRL:  instr=encode::r(0x00,rs2,rs1,5,rd); break;
        case InstrClass::R_SRA:  instr=encode::r(0x20,rs2,rs1,5,rd); break;
        default: break;
    }
    return DecoderTransaction(instr, instr_class_name(cls));
}

inline DecoderTransaction make_i_with_imm(InstrClass cls, uint8_t rd, uint8_t rs1, int32_t imm) {
    uint32_t instr=0;
    switch(cls){
        case InstrClass::I_ADDI:  instr=encode::i(0x13,0,rd,rs1,imm); break;
        case InstrClass::I_SLTI:  instr=encode::i(0x13,2,rd,rs1,imm); break;
        case InstrClass::I_SLTIU: instr=encode::i(0x13,3,rd,rs1,imm); break;
        case InstrClass::I_XORI:  instr=encode::i(0x13,4,rd,rs1,imm); break;
        case InstrClass::I_ORI:   instr=encode::i(0x13,6,rd,rs1,imm); break;
        case InstrClass::I_ANDI:  instr=encode::i(0x13,7,rd,rs1,imm); break;
        default: instr=encode::i(0x13,0,rd,rs1,imm); break;
    }
    return DecoderTransaction(instr, instr_class_name(cls));
}

inline DecoderTransaction make_i(std::mt19937_64& rng, InstrClass cls) {
    uint8_t rs1=random_reg(rng), rd=random_reg(rng);
    int32_t imm=random_imm12(rng);
    if(cls==InstrClass::I_SLLI){uint8_t sh=std::uniform_int_distribution<uint8_t>(0,31)(rng);return DecoderTransaction(encode::i_shift(0x00,1,rd,rs1,sh),"I_SLLI");}
    if(cls==InstrClass::I_SRLI){uint8_t sh=std::uniform_int_distribution<uint8_t>(0,31)(rng);return DecoderTransaction(encode::i_shift(0x00,5,rd,rs1,sh),"I_SRLI");}
    if(cls==InstrClass::I_SRAI){uint8_t sh=std::uniform_int_distribution<uint8_t>(0,31)(rng);return DecoderTransaction(encode::i_shift(0x20,5,rd,rs1,sh),"I_SRAI");}
    return make_i_with_imm(cls,rd,rs1,imm);
}

inline DecoderTransaction make_load(std::mt19937_64& rng){
    return DecoderTransaction(encode::i(0x03,2,random_reg(rng),random_reg(rng),random_imm12(rng)),"LW");
}
inline DecoderTransaction make_store(std::mt19937_64& rng){
    return DecoderTransaction(encode::s(random_reg(rng),random_reg(rng),random_imm12(rng)),"SW");
}
inline DecoderTransaction make_beq(std::mt19937_64& rng){
    return DecoderTransaction(encode::b(random_reg(rng),random_reg(rng),0,random_b_offset(rng)),"BEQ");
}
inline DecoderTransaction make_bne(std::mt19937_64& rng){
    return DecoderTransaction(encode::b(random_reg(rng),random_reg(rng),1,random_b_offset(rng)),"BNE");
}
inline DecoderTransaction make_jal(std::mt19937_64& rng){
    return DecoderTransaction(encode::j(random_reg(rng),random_j_offset(rng)),"JAL");
}
inline DecoderTransaction make_random(std::mt19937_64& rng){
    std::uniform_int_distribution<int> pick(0,kNumInstrClasses-1);
    auto cls=static_cast<InstrClass>(pick(rng));
    if(cls>=InstrClass::R_ADD&&cls<=InstrClass::R_SRA) return make_r(rng,cls);
    if(cls>=InstrClass::I_ADDI&&cls<=InstrClass::I_SRAI) return make_i(rng,cls);
    if(cls==InstrClass::LOAD_LW)    return make_load(rng);
    if(cls==InstrClass::STORE_SW)   return make_store(rng);
    if(cls==InstrClass::BRANCH_BEQ) return make_beq(rng);
    if(cls==InstrClass::BRANCH_BNE) return make_bne(rng);
    return make_jal(rng);
}

} // namespace detail

// ---------------------------------------------------------------------------
// DirectedSequence — hand-crafted, hits every legal encoding deterministically
// ---------------------------------------------------------------------------
class DirectedSequence : public DecoderSequence {
public:
    std::vector<DecoderTransaction> generate(std::mt19937_64&) const override {
        std::vector<DecoderTransaction> v;
        auto add=[&](uint32_t i,const char* l){v.emplace_back(i,l);};

        // R-type: all 10 ops with x0 operands (hits rd_zero, rs1_zero, rs2_zero)
        add(encode::r(0x00, 0, 0,0, 0),"ADD  x0,x0,x0");
        add(encode::r(0x20, 0, 0,0, 0),"SUB  x0,x0,x0");
        add(encode::r(0x00, 0, 0,7, 0),"AND  x0,x0,x0");
        add(encode::r(0x00, 0, 0,6, 0),"OR   x0,x0,x0");
        add(encode::r(0x00, 0, 0,4, 0),"XOR  x0,x0,x0");
        add(encode::r(0x00, 0, 0,2, 0),"SLT  x0,x0,x0");
        add(encode::r(0x00, 0, 0,3, 0),"SLTU x0,x0,x0");
        add(encode::r(0x00, 0, 0,1, 0),"SLL  x0,x0,x0");
        add(encode::r(0x00, 0, 0,5, 0),"SRL  x0,x0,x0");
        add(encode::r(0x20, 0, 0,5, 0),"SRA  x0,x0,x0");
        // R-type with non-zero regs
        add(encode::r(0x00,11,10,0,10),"ADD  x10,x10,x11");
        add(encode::r(0x20,11,10,0, 5),"SUB  x5,x10,x11");
        add(encode::r(0x00,11,10,5, 5),"SRL  x5,x10,x11");
        add(encode::r(0x20,11,10,5, 5),"SRA  x5,x10,x11");

        // I-type: all 9 ops × boundary immediates
        for (auto [cls,f3] : std::initializer_list<std::pair<InstrClass,uint8_t>>{
                {InstrClass::I_ADDI,0},{InstrClass::I_SLTI,2},{InstrClass::I_SLTIU,3},
                {InstrClass::I_XORI,4},{InstrClass::I_ORI,6},{InstrClass::I_ANDI,7}}) {
            add(encode::i(0x13,f3,10,11, 0),   instr_class_name(cls));  // imm=0
            add(encode::i(0x13,f3,10,11, 1),   instr_class_name(cls));  // imm=+1
            add(encode::i(0x13,f3,10,11,-1),   instr_class_name(cls));  // imm=-1
            add(encode::i(0x13,f3,10,11, 2047),instr_class_name(cls));  // imm=max
            add(encode::i(0x13,f3,10,11,-2048),instr_class_name(cls));  // imm=min
            add(encode::i(0x13,f3, 0, 0, 0),   instr_class_name(cls));  // rs1=rd=x0
        }
        // Shifts
        add(encode::i_shift(0x00,1,10,11, 0),"SLLI shamt=0");
        add(encode::i_shift(0x00,1,10,11, 1),"SLLI shamt=1");
        add(encode::i_shift(0x00,1,10,11,31),"SLLI shamt=31");
        add(encode::i_shift(0x00,5,10,11, 0),"SRLI shamt=0");
        add(encode::i_shift(0x00,5,10,11, 1),"SRLI shamt=1");
        add(encode::i_shift(0x00,5,10,11,31),"SRLI shamt=31");
        add(encode::i_shift(0x20,5,10,11, 0),"SRAI shamt=0");
        add(encode::i_shift(0x20,5,10,11, 1),"SRAI shamt=1");
        add(encode::i_shift(0x20,5,10,11,31),"SRAI shamt=31");

        // Load/Store boundary immediates
        add(encode::i(0x03,2,10,11,   0),"LW imm=0");
        add(encode::i(0x03,2,10,11,   1),"LW imm=1");
        add(encode::i(0x03,2,10,11,  -1),"LW imm=-1");
        add(encode::i(0x03,2,10,11,2047),"LW imm=max");
        add(encode::i(0x03,2,10,11,-2048),"LW imm=min");
        add(encode::i(0x03,2, 0, 0,   0),"LW rs1=rd=x0");
        add(encode::s(11,10,   0),"SW imm=0");
        add(encode::s(11,10,   1),"SW imm=1");
        add(encode::s(11,10,  -1),"SW imm=-1");
        add(encode::s(11,10,2047),"SW imm=max");
        add(encode::s(11,10,-2048),"SW imm=min");
        add(encode::s( 0, 0,   0),"SW rs1=rs2=x0");

        // Branch boundary offsets
        add(encode::b(10,11,0,   0),"BEQ off=0");
        add(encode::b(10,11,0,   2),"BEQ off=+2");
        add(encode::b(10,11,0,  -2),"BEQ off=-2");
        add(encode::b(10,11,0,4094),"BEQ off=max");
        add(encode::b(10,11,0,-4096),"BEQ off=min");
        add(encode::b(10,11,1,   2),"BNE off=+2");
        add(encode::b(10,11,1,  -2),"BNE off=-2");
        add(encode::b( 0, 0,0,   2),"BEQ rs1=rs2=x0");

        // JAL
        add(encode::j( 1,      2),"JAL rd=x1 off=+2");
        add(encode::j( 1,     -2),"JAL rd=x1 off=-2");
        add(encode::j( 0,      2),"JAL rd=x0 off=+2");
        add(encode::j( 1,1048574),"JAL off=max");
        add(encode::j( 1,-1048576),"JAL off=min");

        return v;
    }
};

// ---------------------------------------------------------------------------
// BiasedRandomSequence — 80% ADD+ADDI+LW, 20% uniform
// Proves random alone cannot close the v2 coverage model.
// ---------------------------------------------------------------------------
class BiasedRandomSequence : public DecoderSequence {
public:
    explicit BiasedRandomSequence(int n=10000) : n_(n) {}

    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        v.reserve(n_);
        std::uniform_int_distribution<int> pct(0,99);
        for (int i=0;i<n_;++i) {
            int r=pct(rng);
            if      (r<35) v.push_back(detail::make_r(rng,InstrClass::R_ADD));
            else if (r<60) v.push_back(detail::make_i(rng,InstrClass::I_ADDI));
            else if (r<80) v.push_back(detail::make_load(rng));
            else           v.push_back(detail::make_random(rng));
        }
        return v;
    }
private:
    int n_;
};

// ---------------------------------------------------------------------------
// IllegalEncodingSequence — explicitly targets every illegal bin in Group F
// ---------------------------------------------------------------------------
class IllegalEncodingSequence : public DecoderSequence {
public:
    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        std::uniform_int_distribution<uint8_t> r5(1,31);

        // 1. Unknown opcodes — pick several invalid opcodes
        for (uint8_t op : {0x00u,0x05u,0x10u,0x17u,0x1Fu,0x37u,0x3Fu,0x7Fu}) {
            uint32_t instr = uint32_t(op) | (uint32_t(r5(rng))<<7)
                           | (uint32_t(r5(rng))<<15) | (uint32_t(r5(rng))<<20);
            v.emplace_back(instr, "ILLEGAL_OPCODE");
        }

        // 2. R-type with funct7 ∉ {0x00, 0x20}
        for (uint8_t f7 : {0x01u,0x02u,0x04u,0x08u,0x40u,0x7Fu}) {
            for (int f3=0;f3<8;++f3)
                v.emplace_back(encode::r(f7,r5(rng),r5(rng),f3,r5(rng)),"RTYPE_BAD_F7");
        }

        // 3. R-type: ALT_20 with funct3 ≠ 0 and ≠ 5 (reserved combo)
        for (int f3=1;f3<=7;++f3) {
            if (f3==5) continue;
            v.emplace_back(encode::r(0x20,r5(rng),r5(rng),f3,r5(rng)),"RTYPE_RESERVED_F3F7");
        }

        // 4. I-type shift with illegal funct7 (funct7[6:2] ≠ 0)
        for (uint8_t bad_f7 : {0x02u,0x04u,0x08u,0x10u,0x40u}) {
            // SLLI (f3=1) with bad funct7
            v.emplace_back(
                (uint32_t(bad_f7)<<25)|(uint32_t(r5(rng)&0x1F)<<20)
                |(uint32_t(r5(rng))<<15)|(1u<<12)|(uint32_t(r5(rng))<<7)|0x13u,
                "ISHIFT_BAD_F7_SLLI");
            // SRLI/SRAI (f3=5) with bad funct7
            v.emplace_back(
                (uint32_t(bad_f7)<<25)|(uint32_t(r5(rng)&0x1F)<<20)
                |(uint32_t(r5(rng))<<15)|(5u<<12)|(uint32_t(r5(rng))<<7)|0x13u,
                "ISHIFT_BAD_F7_SRI");
        }

        // 5. Load with funct3 ≠ 2 (LB=0, LH=1, LBU=4, LHU=5 — not implemented)
        for (uint8_t f3 : {0u,1u,3u,4u,5u,6u,7u})
            v.emplace_back(encode::i(0x03,f3,r5(rng),r5(rng),0),"LOAD_BAD_F3");

        // 6. Branch with funct3 > 1 (BLT=4,BGE=5,BLTU=6,BGEU=7 — not decoded)
        for (uint8_t f3 : {2u,3u,4u,5u,6u,7u})
            v.emplace_back(encode::b(r5(rng),r5(rng),f3,8),"BRANCH_BAD_F3");

        return v;
    }
};

// ---------------------------------------------------------------------------
// BoundaryImmSequence — sweeps NEG_MIN / NEG_ONE / ZERO / POS_ONE / POS_MAX
// for every instruction class that has an immediate.
// ---------------------------------------------------------------------------
class BoundaryImmSequence : public DecoderSequence {
public:
    explicit BoundaryImmSequence(int repeats=5) : repeats_(repeats) {}

    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        std::uniform_int_distribution<uint8_t> r5(1,31);

        auto add_i = [&](InstrClass cls, uint8_t f3, int32_t imm){
            v.emplace_back(encode::i(0x13,f3,r5(rng),r5(rng),imm), instr_class_name(cls));
        };

        for (int rep=0;rep<repeats_;++rep) {
            // I-type non-shift
            for (auto [cls,f3] : std::initializer_list<std::pair<InstrClass,uint8_t>>{
                    {InstrClass::I_ADDI,0},{InstrClass::I_SLTI,2},{InstrClass::I_SLTIU,3},
                    {InstrClass::I_XORI,4},{InstrClass::I_ORI,6},{InstrClass::I_ANDI,7}}) {
                for (int32_t imm : {-2048,-1,0,1,2047})
                    add_i(cls,f3,imm);
            }
            // Shifts — shamt boundaries
            for (uint8_t sh : {0u,1u,31u}) {
                v.emplace_back(encode::i_shift(0x00,1,r5(rng),r5(rng),sh),"SLLI");
                v.emplace_back(encode::i_shift(0x00,5,r5(rng),r5(rng),sh),"SRLI");
                v.emplace_back(encode::i_shift(0x20,5,r5(rng),r5(rng),sh),"SRAI");
            }
            // LW
            for (int32_t imm : {-2048,-1,0,1,2047})
                v.emplace_back(encode::i(0x03,2,r5(rng),r5(rng),imm),"LW");
            // SW
            for (int32_t imm : {-2048,-1,0,1,2047})
                v.emplace_back(encode::s(r5(rng),r5(rng),imm),"SW");
            // BEQ / BNE
            for (int32_t off : {-4096,-2,0,2,4094}) {
                v.emplace_back(encode::b(r5(rng),r5(rng),0,off),"BEQ");
                v.emplace_back(encode::b(r5(rng),r5(rng),1,off),"BNE");
            }
            // JAL
            for (int32_t off : {-1048576,-2,0,2,1048574})
                v.emplace_back(encode::j(r5(rng),off),"JAL");
        }
        return v;
    }
private:
    int repeats_;
};

// ---------------------------------------------------------------------------
// RegisterZeroSequence — ensures rs1=x0, rs2=x0, rd=x0 for every instr class
// ---------------------------------------------------------------------------
class RegisterZeroSequence : public DecoderSequence {
public:
    explicit RegisterZeroSequence(int repeats=5) : repeats_(repeats) {}

    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        std::uniform_int_distribution<uint8_t> r5(1,31);

        for (int rep=0;rep<repeats_;++rep) {
            // R-type: vary which register is x0
            for (auto cls : {InstrClass::R_ADD,InstrClass::R_SUB,InstrClass::R_AND,
                             InstrClass::R_OR,InstrClass::R_XOR,InstrClass::R_SLT,
                             InstrClass::R_SLTU,InstrClass::R_SLL,InstrClass::R_SRL,InstrClass::R_SRA}) {
                v.push_back(detail::make_r(rng,cls)); // random (baseline)
                // rd=x0
                auto tx=detail::make_r(rng,cls); tx.instr &= ~(0x1Fu<<7); v.push_back(tx);
                // rs1=x0
                auto tx2=detail::make_r(rng,cls); tx2.instr &= ~(0x1Fu<<15); v.push_back(tx2);
                // rs2=x0
                auto tx3=detail::make_r(rng,cls); tx3.instr &= ~(0x1Fu<<20); v.push_back(tx3);
            }
            // I-type: rd=x0, rs1=x0
            for (auto cls : {InstrClass::I_ADDI,InstrClass::I_SLTI,InstrClass::I_SLTIU,
                             InstrClass::I_XORI,InstrClass::I_ORI,InstrClass::I_ANDI,
                             InstrClass::I_SLLI,InstrClass::I_SRLI,InstrClass::I_SRAI}) {
                v.push_back(detail::make_i(rng,cls));
                auto tx=detail::make_i(rng,cls); tx.instr &= ~(0x1Fu<<7);  v.push_back(tx); // rd=0
                auto tx2=detail::make_i(rng,cls); tx2.instr &= ~(0x1Fu<<15); v.push_back(tx2); // rs1=0
            }
            // LW
            v.emplace_back(encode::i(0x03,2, 0,r5(rng),0),"LW rd=x0");
            v.emplace_back(encode::i(0x03,2,r5(rng),0,0),"LW rs1=x0");
            // SW: rs1=x0, rs2=x0
            v.emplace_back(encode::s(0,r5(rng),0),"SW rs1=x0");
            v.emplace_back(encode::s(r5(rng),0,0),"SW rs2=x0");
            // BEQ/BNE
            v.emplace_back(encode::b(0,r5(rng),0,8),"BEQ rs1=x0");
            v.emplace_back(encode::b(r5(rng),0,0,8),"BEQ rs2=x0");
            v.emplace_back(encode::b(0,r5(rng),1,8),"BNE rs1=x0");
            // JAL
            v.emplace_back(encode::j(0,8),"JAL rd=x0");
        }
        return v;
    }
private:
    int repeats_;
};

// ---------------------------------------------------------------------------
// RTypeFunct7Sequence — exhausts the funct3 × funct7 cross
// ---------------------------------------------------------------------------
class RTypeFunct7Sequence : public DecoderSequence {
public:
    explicit RTypeFunct7Sequence(int repeats=10) : repeats_(repeats) {}

    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        std::uniform_int_distribution<uint8_t> r5(1,31);
        // Legal funct7 values
        for (int rep=0;rep<repeats_;++rep)
            for (int f3=0;f3<8;++f3) {
                v.emplace_back(encode::r(0x00,r5(rng),r5(rng),f3,r5(rng)),
                               "RTYPE_f7=0x00");
                v.emplace_back(encode::r(0x20,r5(rng),r5(rng),f3,r5(rng)),
                               "RTYPE_f7=0x20");
            }
        return v;
    }
private:
    int repeats_;
};

// ---------------------------------------------------------------------------
// RandomStressSequence — uniform random (unchanged, used as reference)
// ---------------------------------------------------------------------------
class RandomStressSequence : public DecoderSequence {
public:
    explicit RandomStressSequence(int n=10000) : n_(n) {}
    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        v.reserve(n_);
        for (int i=0;i<n_;++i) v.push_back(detail::make_random(rng));
        return v;
    }
private: int n_;
};

// ---------------------------------------------------------------------------
// FullSweepSequence — one of everything (kept for compatibility)
// ---------------------------------------------------------------------------
class FullSweepSequence : public DecoderSequence {
public:
    explicit FullSweepSequence(int repeats=15) : repeats_(repeats) {}
    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<DecoderTransaction> v;
        for (int rep=0;rep<repeats_;++rep)
            for (int ci=0;ci<kNumInstrClasses;++ci)
                v.push_back(detail::make_random(rng)); // just ensure class variety
        return v;
    }
private: int repeats_;
};

// ---------------------------------------------------------------------------
// CoverageDrivenSequence (v2) — uses the v2 WeakBin list
// ---------------------------------------------------------------------------
class CoverageDrivenSequence : public DecoderSequence {
public:
    CoverageDrivenSequence(const DecoderCoverage& cov, int n=10000, int threshold=10)
        : cov_(cov), n_(n), threshold_(threshold) {}

    std::vector<DecoderTransaction> generate(std::mt19937_64& rng) const override {
        auto weak = cov_.get_weak_bins(threshold_);
        const size_t pool = std::min(weak.size(), size_t(12));

        std::vector<DecoderTransaction> v;
        v.reserve(n_);
        std::uniform_int_distribution<int> pct(0,99);
        std::uniform_int_distribution<uint8_t> r5(1,31);

        for (int i=0;i<n_;++i) {
            if (!weak.empty() && pct(rng)<40) {
                std::uniform_int_distribution<size_t> pick(0,pool-1);
                const auto& wb = weak[pick(rng)];
                v.push_back(synthesize(wb,rng));
            } else {
                v.push_back(detail::make_random(rng));
                v.back().label="CovDriven:random";
            }
        }
        return v;
    }

private:
    DecoderTransaction synthesize(const DecoderCoverage::WeakBin& wb,
                                   std::mt19937_64& rng) const {
        std::uniform_int_distribution<uint8_t> r5(1,31);
        using K = DecoderCoverage::WeakBin::Kind;

        switch (wb.kind) {
            case K::RTYPE_F3:
            case K::RTYPE_F3_F7: {
                int f3 = (wb.f3>=0) ? wb.f3 : std::uniform_int_distribution<int>(0,7)(rng);
                uint8_t f7 = (wb.f7c==Funct7Class::ALT_20) ? 0x20u : 0x00u;
                return DecoderTransaction(encode::r(f7,r5(rng),r5(rng),f3,r5(rng)),"CovDriven:rtype");
            }
            case K::ITYPE_F3: {
                int f3 = (wb.f3>=0) ? wb.f3 : std::uniform_int_distribution<int>(0,7)(rng);
                if (f3==1||f3==5) {
                    uint8_t f7=(f3==5&&wb.f7c==Funct7Class::ALT_20)?0x20u:0x00u;
                    return DecoderTransaction(encode::i_shift(f7,f3,r5(rng),r5(rng),r5(rng)%32),"CovDriven:ishift");
                }
                return detail::make_i_with_imm(static_cast<InstrClass>(static_cast<int>(InstrClass::I_ADDI)+f3),r5(rng),r5(rng),0);
            }
            case K::ITYPE_SHIFT_F7: {
                uint8_t f7=(wb.f7c==Funct7Class::ALT_20)?0x20u:0x00u;
                return DecoderTransaction(encode::i_shift(f7,5,r5(rng),r5(rng),r5(rng)%32),"CovDriven:shift");
            }
            case K::LOAD_F3:
                return detail::make_load(rng);
            case K::STORE_F3:
                return detail::make_store(rng);
            case K::BRANCH_F3:
                return (wb.cls==InstrClass::BRANCH_BEQ) ? detail::make_beq(rng) : detail::make_bne(rng);
            case K::JAL:
                return detail::make_jal(rng);
            case K::IMM_BOUNDARY: {
                auto [imin,imax] = imm_range(wb.cls);
                int32_t imm=0;
                switch(wb.imm_b){
                    case ImmBoundary::NEG_MIN:     imm=imin; break;
                    case ImmBoundary::NEG_ONE:     imm=-1; break;
                    case ImmBoundary::NEG_GENERAL: imm=std::uniform_int_distribution<int32_t>(imin+1,-2)(rng); break;
                    case ImmBoundary::ZERO:        imm=0; break;
                    case ImmBoundary::POS_ONE:     imm=1; break;
                    case ImmBoundary::POS_GENERAL: imm=std::uniform_int_distribution<int32_t>(2,imax-1)(rng); break;
                    case ImmBoundary::POS_MAX:     imm=imax; break;
                    default: break;
                }
                return synthesize_with_imm(wb.cls,imm,rng);
            }
            case K::REG_ZERO:
                return detail::make_random(rng);
            case K::ILLEGAL:
                return detail::make_random(rng);
        }
        return detail::make_random(rng);
    }

    DecoderTransaction synthesize_with_imm(InstrClass cls, int32_t imm,
                                            std::mt19937_64& rng) const {
        std::uniform_int_distribution<uint8_t> r5(1,31);
        if (cls>=InstrClass::I_ADDI && cls<=InstrClass::I_ANDI)
            return detail::make_i_with_imm(cls,r5(rng),r5(rng),imm);
        if (cls==InstrClass::LOAD_LW)
            return DecoderTransaction(encode::i(0x03,2,r5(rng),r5(rng),imm),"LW");
        if (cls==InstrClass::STORE_SW)
            return DecoderTransaction(encode::s(r5(rng),r5(rng),imm),"SW");
        if (cls==InstrClass::BRANCH_BEQ)
            return DecoderTransaction(encode::b(r5(rng),r5(rng),0,imm),"BEQ");
        if (cls==InstrClass::BRANCH_BNE)
            return DecoderTransaction(encode::b(r5(rng),r5(rng),1,imm),"BNE");
        if (cls==InstrClass::JUMP_JAL)
            return DecoderTransaction(encode::j(r5(rng),imm),"JAL");
        return detail::make_random(rng);
    }

    const DecoderCoverage& cov_;
    int n_, threshold_;
};
