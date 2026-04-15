#pragma once

#include <random>
#include <vector>

#include "alu_golden.h"
#include "alu_coverage.h"

class ALUSequence {
public:
    virtual ~ALUSequence() = default;
    virtual std::vector<ALUTransaction> generate(std::mt19937_64& rng) const = 0;
};

class BasicSequence : public ALUSequence {
public:
    std::vector<ALUTransaction> generate(std::mt19937_64&) const override {
        return {
            {10u, 5u, 0u, "Basic ADD"},
            {0u, 0u, 1u, "Zero subtract"},
            {0x80000000u, 1u, 9u, "Basic SRA negative"}
        };
    }
};

class EdgeCaseSequence : public ALUSequence {
public:
    std::vector<ALUTransaction> generate(std::mt19937_64&) const override {
        return {
            {0xAAAAAAAAu, 0u, 6u, "SLL shift by 0"},
            {0xAAAAAAAAu, 1u, 6u, "SLL shift by 1"},
            {0xAAAAAAAAu, 31u, 6u, "SLL shift by 31"},
            {0xAAAAAAAAu, 32u, 6u, "SLL shift by 32 uses lower 5 bits"},
            {0xAAAAAAAAu, 63u, 6u, "SLL shift by 63 uses lower 5 bits"},
            {0xAAAAAAAAu, 0x12345678u, 6u, "SLL random large number"},
            {0x80000000u, 1u, 9u, "SRA negative number preserves sign"},
            {0xFFFFFFFFu, 1u, 9u, "SRA -1 remains -1"},
            {0xF0000000u, 4u, 9u, "SRA sign extension large negative"},
            {0x80000000u, 1u, 7u, "SRL negative number logical"},
            {0xFFFFFFFFu, 1u, 7u, "SRL negative number logical"},
            {0x7FFFFFFFu, 1u, 0u, "INT_MAX + 1 wraps to INT_MIN"},
            {0x80000000u, 1u, 1u, "INT_MIN - 1 wraps to INT_MAX"},
            {0x7FFFFFFFu, 0x7FFFFFFFu, 0u, "Large addition overflow"},
            {0x80000000u, 0xFFFFFFFFu, 1u, "Large subtraction underflow"},
            {0xFFFFFFFFu, 1u, 5u, "SLT negative vs positive"},
            {1u, 0xFFFFFFFFu, 5u, "SLT positive vs negative"},
            {0xFFFFFFFFu, 0u, 8u, "SLTU max vs zero false"},
            {0u, 0xFFFFFFFFu, 8u, "SLTU zero vs max true"},
            {0u, 0u, 0u, "ADD zero-zero identity"},
            {0u, 0u, 1u, "SUB zero-zero identity"},
            {42u, 42u, 1u, "SUB identical operands zero"},
            {0u, 0xCAFEBABEu, 2u, "AND with zero returns zero"},
            {0u, 0xCAFEBABEu, 3u, "OR with zero returns operand"},
            {0u, 0xCAFEBABEu, 4u, "XOR with zero returns operand"}
        };
    }
};

class RandomStressSequence : public ALUSequence {
public:
    explicit RandomStressSequence(int iterations = 10000)
        : iterations_(iterations) {}

    std::vector<ALUTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<ALUTransaction> items;
        items.reserve(iterations_);
        std::uniform_int_distribution<uint32_t> rand32(0u, 0xFFFFFFFFu);
        std::uniform_int_distribution<uint32_t> rand_mid_shift(4u, 30u);
        std::uniform_int_distribution<uint32_t> rand_small_shift(1u, 3u);
        for (int i = 0; i < iterations_; ++i) {
            // Every 20 iterations inject one directed stimulus to reach
            // under-sampled bins (max/mid/small shift, negative operands,
            // overflow).  All other slots use the existing biased generator.
            const int phase = i % 20;
            if (phase == 1) {
                // Force negative A + max shift 31 → hits SHIFT_MAX × NEGATIVE
                uint32_t neg_a = 0x80000000u | (rand32(rng) & 0x7FFFFFFFu);
                items.emplace_back(neg_a, 31u, random_op(rng), "Directed: neg-A max-shift");
            } else if (phase == 3) {
                // Force ADD overflow: 0x7FFFFFFF + positive
                items.emplace_back(0x7FFFFFFFu, rand32(rng) & 0x7FFFFFFFu, 0u, "Directed: ADD overflow");
            } else if (phase == 5) {
                // Force mid-range shift (4–30)
                items.emplace_back(biased_operand(rng), rand_mid_shift(rng), random_op(rng), "Directed: mid shift");
            } else if (phase == 7) {
                // Force both operands negative
                uint32_t neg_a = 0x80000000u | (rand32(rng) & 0x7FFFFFFFu);
                uint32_t neg_b = 0x80000000u | (rand32(rng) & 0x7FFFFFFFu);
                items.emplace_back(neg_a, neg_b, random_op(rng), "Directed: both negative");
            } else if (phase == 9) {
                // Force small shift (1–3)
                items.emplace_back(biased_operand(rng), rand_small_shift(rng), random_op(rng), "Directed: small shift");
            } else {
                items.emplace_back(biased_operand(rng), biased_operand(rng), random_op(rng), "Random stress");
            }
        }
        return items;
    }

private:
    int iterations_;
};

// Exhaustively exercises every combination of:
//   special_A × special_B × op  (each repeated `repeats_per_combo` times)
//
// This is the directed "corner-case" layer that guarantees the rare
// (special × special × op) 3-D bins all reach the minimum hit count,
// independently of random sampling statistics.
class SpecialValueSweepSequence : public ALUSequence {
public:
    explicit SpecialValueSweepSequence(int repeats_per_combo = 15)
        : repeats_per_combo_(repeats_per_combo) {}

    std::vector<ALUTransaction> generate(std::mt19937_64&) const override {
        // Covers all five OperandCategories:
        //   ZERO=0, MAX=0xFFFFFFFF, MIN=0x80000000,
        //   PATTERN×2={0xAAAAAAAA,0x55555555}, plus 0x7FFFFFFF for POSITIVE sign
        static const std::array<uint32_t, 6> kSpecialValues = {
            0u, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0xAAAAAAAAu, 0x55555555u
        };
        std::vector<ALUTransaction> items;
        items.reserve(kSpecialValues.size() * kSpecialValues.size() * 10 * repeats_per_combo_);
        for (uint32_t a : kSpecialValues) {
            for (uint32_t b : kSpecialValues) {
                for (uint8_t op = 0; op < 10; ++op) {
                    for (int rep = 0; rep < repeats_per_combo_; ++rep) {
                        items.emplace_back(a, b, op, "Special sweep");
                    }
                }
            }
        }
        return items;
    }

private:
    int repeats_per_combo_;
};

// Coverage-driven sequence — mimics a UVM sequencer with a priority queue.
//
// At generation time it reads the current coverage model, builds a sorted list
// of weak bins (hits < threshold), then generates transactions with a 70/30
// split:
//   30% targeted  — pick uniformly from the 10 weakest bins, synthesize
//                   concrete (a, b, op) values that land exactly in that bin.
//   70% random    — existing biased_operand() generator.
//
// Running multiple passes (each pass constructing a new CoverageDrivenSequence
// over the same ALUEnvironment) progressively closes coverage because each
// pass sees the updated hit counts from all prior passes.
class CoverageDrivenSequence : public ALUSequence {
public:
    CoverageDrivenSequence(const ALUCoverage& cov,
                           int iterations = 10000,
                           int threshold  = 10)
        : coverage_(cov), iterations_(iterations), threshold_(threshold) {}

    std::vector<ALUTransaction> generate(std::mt19937_64& rng) const override {
        // Snapshot weak bins once at the start (feedback from prior passes).
        const auto weak_bins = coverage_.get_sorted_weak_bins(threshold_);
        // Candidate pool: up to the 10 weakest bins to prevent over-focusing
        // on a single bin and to maintain intra-pass variety.
        const size_t pool_size = std::min(weak_bins.size(), size_t(10));

        std::vector<ALUTransaction> items;
        items.reserve(iterations_);
        std::uniform_int_distribution<int> percent(0, 99);

        for (int i = 0; i < iterations_; ++i) {
            if (!weak_bins.empty() && percent(rng) < 30) {
                // ---- 30%: targeted toward a weak bin ----
                std::uniform_int_distribution<size_t> pick(0, pool_size - 1);
                const BinKey& target = weak_bins[pick(rng)];
                auto [a, b] = synthesize_operands_for_bin(
                    target.cat_a, target.cat_b,
                    target.shift_cat, target.sign_cat, rng);
                items.emplace_back(a, b, static_cast<uint8_t>(target.op),
                                   "CovDriven: targeted");
            } else {
                // ---- 70%: biased random (existing generator) ----
                items.emplace_back(biased_operand(rng), biased_operand(rng),
                                   random_op(rng), "CovDriven: random");
            }
        }
        return items;
    }

private:
    const ALUCoverage& coverage_;
    int iterations_;
    int threshold_;
};

class OperationSweepSequence : public ALUSequence {
public:
    explicit OperationSweepSequence(int iterations_per_op = 1000)
        : iterations_per_op_(iterations_per_op) {}

    std::vector<ALUTransaction> generate(std::mt19937_64& rng) const override {
        std::vector<ALUTransaction> items;
        items.reserve(iterations_per_op_ * kAluOpNames.size());
        for (uint8_t op = 0; op < kAluOpNames.size(); ++op) {
            for (int i = 0; i < iterations_per_op_; ++i) {
                items.emplace_back(biased_operand(rng), biased_operand(rng), op, "Operation sweep");
            }
        }
        return items;
    }

private:
    int iterations_per_op_;
};
