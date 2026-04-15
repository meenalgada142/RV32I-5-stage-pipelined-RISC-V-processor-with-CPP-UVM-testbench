#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "alu_transaction.h"
#include "alu_golden.h"

enum OperandCategory {
    ZERO = 0,
    MAX = 1,
    MIN = 2,
    RANDOM = 3,
    PATTERN = 4,
    NUM_OPERAND_CATEGORIES = 5
};

enum ShiftCategory {
    SHIFT_ZERO = 0,
    SHIFT_SMALL = 1,
    SHIFT_MID = 2,
    SHIFT_MAX = 3,
    SHIFT_OVERFLOW = 4,
    NUM_SHIFT_CATEGORIES = 5
};

enum SignCategory {
    POSITIVE = 0,
    NEGATIVE = 1,
    NUM_SIGN_CATEGORIES = 2
};

static OperandCategory get_operand_category(uint32_t value) {
    if (value == 0) return ZERO;
    if (value == 0xFFFFFFFF) return MAX;
    if (value == 0x80000000) return MIN;
    if (value == 0xAAAAAAAA || value == 0x55555555) return PATTERN;
    return RANDOM;
}

static ShiftCategory get_shift_category(uint32_t shift_amount) {
    // Classify the RAW operand value, not the ALU-masked effective shift.
    // SHIFT_OVERFLOW captures raw values >=32 that the ALU wraps via &0x1F.
    if (shift_amount >= 32) return SHIFT_OVERFLOW;
    if (shift_amount == 0)  return SHIFT_ZERO;
    if (shift_amount <= 3)  return SHIFT_SMALL;
    if (shift_amount <= 30) return SHIFT_MID;
    return SHIFT_MAX;  // == 31
}

static SignCategory get_sign_category(uint32_t a, uint32_t b) {
    bool a_negative = (a & 0x80000000) != 0;
    bool b_negative = (b & 0x80000000) != 0;
    return (a_negative || b_negative) ? NEGATIVE : POSITIVE;
}

static const char* operand_cat_name(OperandCategory cat) {
    static const char* names[] = {"ZERO", "MAX", "MIN", "RANDOM", "PATTERN"};
    return (static_cast<int>(cat) < NUM_OPERAND_CATEGORIES) ? names[cat] : "UNKNOWN";
}

// Returns false for 5-D bins that can NEVER be hit because the same b value
// simultaneously determines both B_category and shift_category, and because
// certain operand values have a fixed sign.
//
// Impossible combinations:
//   B=ZERO    → shift is always SHIFT_ZERO     (b=0)
//   B=MAX     → shift is always SHIFT_OVERFLOW  (0xFFFFFFFF >= 32)
//   B=MIN     → shift is always SHIFT_OVERFLOW  (0x80000000 >= 32)
//   B=PATTERN → shift is always SHIFT_OVERFLOW  (both patterns >= 32)
//   B=RANDOM  → shift is never  SHIFT_ZERO      (b=0 lands in ZERO, not RANDOM)
//   A=MAX/MIN → sign is always  NEGATIVE        (MSB always set)
//   B=MAX/MIN → sign is always  NEGATIVE        (MSB always set)
//   A=ZERO, B=ZERO → sign is always POSITIVE    (both zero)
static bool is_bin_reachable(OperandCategory cat_a, OperandCategory cat_b,
                              ShiftCategory   shift_cat, SignCategory sign_cat) {
    // B_category ↔ shift_category hard coupling
    if (cat_b == ZERO    && shift_cat != SHIFT_ZERO)     return false;
    if (cat_b == MAX     && shift_cat != SHIFT_OVERFLOW) return false;
    if (cat_b == MIN     && shift_cat != SHIFT_OVERFLOW) return false;
    if (cat_b == PATTERN && shift_cat != SHIFT_OVERFLOW) return false;
    if (cat_b == RANDOM  && shift_cat == SHIFT_ZERO)     return false;

    // Sign constraints: POSITIVE requires BOTH operands non-negative.
    // A=MAX/MIN always have MSB set → POSITIVE is impossible regardless of B.
    if (cat_a == MAX && sign_cat == POSITIVE) return false;
    if (cat_a == MIN && sign_cat == POSITIVE) return false;
    // B=MAX/MIN always have MSB set → POSITIVE is impossible regardless of A.
    if (cat_b == MAX && sign_cat == POSITIVE) return false;
    if (cat_b == MIN && sign_cat == POSITIVE) return false;
    // A=ZERO (0x0) and B=ZERO (0x0) are both non-negative → NEGATIVE impossible.
    if (cat_a == ZERO && cat_b == ZERO && sign_cat == NEGATIVE) return false;

    return true;
}

// Full 5-D bin descriptor with its current hit count.
// Used by the coverage-driven generator to identify and target weak bins.
struct BinKey {
    int             op;
    OperandCategory cat_a;
    OperandCategory cat_b;
    ShiftCategory   shift_cat;
    SignCategory    sign_cat;
    int             hits;
};

// Synthesize a concrete (a, b) operand pair that will land in the requested
// (cat_a, cat_b, shift_cat, sign_cat) coverage bin.
//
// B is synthesized first because it simultaneously determines both B_category
// and shift_category (they are coupled for non-RANDOM special values).
// A is synthesized second, with sign forcing applied only if B doesn't already
// satisfy the sign requirement.
//
// A 200-iteration retry cap prevents infinite loops on the RANDOM category;
// guaranteed non-special fallback values are used if the cap is hit.
static std::pair<uint32_t, uint32_t> synthesize_operands_for_bin(
        OperandCategory cat_a, OperandCategory cat_b,
        ShiftCategory   shift_cat, SignCategory sign_cat,
        std::mt19937_64& rng) {

    static const std::array<uint32_t, 5> kSpecial = {
        0u, 0xFFFFFFFFu, 0x80000000u, 0xAAAAAAAAu, 0x55555555u
    };
    auto is_special = [](uint32_t v) {
        for (uint32_t s : kSpecial) { if (v == s) return true; }
        return false;
    };
    // Generate a RANDOM-category value, optionally forcing MSB set/clear.
    auto random_nonspecial = [&](bool force_msb_set, bool force_msb_clear) -> uint32_t {
        std::uniform_int_distribution<uint32_t> d(0u, 0xFFFFFFFFu);
        for (int attempt = 0; attempt < 200; ++attempt) {
            uint32_t v = d(rng);
            if (force_msb_set)   v |= 0x80000000u;
            if (force_msb_clear) v &= 0x7FFFFFFFu;
            if (!is_special(v))  return v;
        }
        return force_msb_set ? 0x92345678u : 0x12345678u;  // guaranteed fallback
    };

    // ---- Synthesize B ----
    std::uniform_int_distribution<uint32_t> rand_small(1u, 3u);
    std::uniform_int_distribution<uint32_t> rand_mid(4u, 30u);
    uint32_t b;
    switch (cat_b) {
        case ZERO:    b = 0u;          break;
        case MAX:     b = 0xFFFFFFFFu; break;
        case MIN:     b = 0x80000000u; break;
        case PATTERN: b = (sign_cat == POSITIVE) ? 0x55555555u : 0xAAAAAAAAu; break;
        case RANDOM:
            switch (shift_cat) {
                case SHIFT_SMALL:    b = rand_small(rng); break;
                case SHIFT_MID:      b = rand_mid(rng);   break;
                case SHIFT_MAX:      b = 31u;             break;
                case SHIFT_OVERFLOW: b = random_nonspecial(/*msb_set=*/true, false); break;
                default:             b = random_nonspecial(false, false);            break;
            }
            break;
        default: b = 0u; break;
    }

    // ---- Synthesize A (sign-aware) ----
    // POSITIVE sign requires both MSBs clear.
    // NEGATIVE sign requires at least one MSB set — B may already provide it.
    const bool b_is_neg    = (b & 0x80000000u) != 0;
    const bool need_a_neg  = (sign_cat == NEGATIVE) && !b_is_neg;
    const bool need_a_pos  = (sign_cat == POSITIVE);

    uint32_t a;
    switch (cat_a) {
        case ZERO:    a = 0u;          break;
        case MAX:     a = 0xFFFFFFFFu; break;
        case MIN:     a = 0x80000000u; break;
        case PATTERN: a = need_a_pos ? 0x55555555u : 0xAAAAAAAAu; break;
        case RANDOM:  a = random_nonspecial(need_a_neg, need_a_pos); break;
        default:      a = 0u; break;
    }

    return {a, b};
}

// Represents a (op, A_category, B_category) bucket that failed the minimum
// hit-count threshold.
struct CoverageHole {
    int            op;
    OperandCategory cat_a;
    OperandCategory cat_b;
    int            hits;
};

class ALUCoverage {
public:
    void sample(const ALUTransaction& tx) {
        if (tx.op < kOpCounts.size()) {
            ++kOpCounts[tx.op];
        }
        if (tx.expected == 0u) {
            ++zero_count_;
        }
        if (tx.op == 6 || tx.op == 7 || tx.op == 9) {
            ++shift_count_;
        }
        if (tx.op == 5 || tx.op == 8) {
            ++compare_count_;
        }
        // Expanded cross coverage
        OperandCategory cat_a = get_operand_category(tx.a);
        OperandCategory cat_b = get_operand_category(tx.b);
        ShiftCategory shift_cat = get_shift_category(tx.b);
        SignCategory sign_cat = get_sign_category(tx.a, tx.b);
        if (tx.op < 10) {
            ++cross_coverage_[tx.op][cat_a][cat_b][shift_cat][sign_cat];
        }
    }

    // Single-line percentage summary: "X.X% (N/M bins hit)"
    // Used wherever the full verbose report would create too much noise.
    std::string summary() const {
        int total = 0, covered = 0;
        for (int op = 0; op < 10; ++op) {
            for (int ca = 0; ca < NUM_OPERAND_CATEGORIES; ++ca) {
                for (int cb = 0; cb < NUM_OPERAND_CATEGORIES; ++cb) {
                    for (int sc = 0; sc < NUM_SHIFT_CATEGORIES; ++sc) {
                        for (int sg = 0; sg < NUM_SIGN_CATEGORIES; ++sg) {
                            if (!is_bin_reachable(static_cast<OperandCategory>(ca),
                                                  static_cast<OperandCategory>(cb),
                                                  static_cast<ShiftCategory>(sc),
                                                  static_cast<SignCategory>(sg))) continue;
                            ++total;
                            if (cross_coverage_[op][ca][cb][sc][sg] > 0) ++covered;
                        }
                    }
                }
            }
        }
        std::ostringstream oss;
        oss << std::fixed;
        oss.precision(1);
        oss << (total ? covered * 100.0 / total : 0.0) << "% (" << covered << "/" << total << " bins hit)";
        return oss.str();
    }

    std::string report() const {
        std::ostringstream oss;
        oss << "Coverage: ";
        for (size_t i = 0; i < kOpCounts.size(); ++i) {
            oss << op_name(static_cast<uint8_t>(i)) << "=" << kOpCounts[i] << " ";
        }
        oss << "zeros=" << zero_count_ << " shifts=" << shift_count_ << " compares=" << compare_count_;
        // Cross coverage summary — only count reachable bins
        int total_bins = 0;
        int covered_bins = 0;
        std::vector<std::string> holes;
        std::vector<std::string> low_hits;
        const char* op_names[] = {"ADD", "SUB", "AND", "OR", "XOR", "SLT", "SLL", "SRL", "SLTU", "SRA"};
        const char* operand_names[] = {"ZERO", "MAX", "MIN", "RANDOM", "PATTERN"};
        const char* shift_names[] = {"SHIFT_ZERO", "SHIFT_SMALL", "SHIFT_MID", "SHIFT_MAX", "SHIFT_OVERFLOW"};
        const char* sign_names[] = {"POSITIVE", "NEGATIVE"};
        for (int op = 0; op < 10; ++op) {
            for (int cat_a = 0; cat_a < NUM_OPERAND_CATEGORIES; ++cat_a) {
                for (int cat_b = 0; cat_b < NUM_OPERAND_CATEGORIES; ++cat_b) {
                    for (int shift_cat = 0; shift_cat < NUM_SHIFT_CATEGORIES; ++shift_cat) {
                        for (int sign_cat = 0; sign_cat < NUM_SIGN_CATEGORIES; ++sign_cat) {
                            if (!is_bin_reachable(static_cast<OperandCategory>(cat_a),
                                                  static_cast<OperandCategory>(cat_b),
                                                  static_cast<ShiftCategory>(shift_cat),
                                                  static_cast<SignCategory>(sign_cat))) {
                                continue;  // phantom bin — skip entirely
                            }
                            ++total_bins;
                            int hits = cross_coverage_[op][cat_a][cat_b][shift_cat][sign_cat];
                            if (hits > 0) {
                                ++covered_bins;
                            } else {
                                holes.push_back(std::string(op_names[op]) + ", A=" + operand_names[cat_a] + ", B=" + operand_names[cat_b] + ", Shift=" + shift_names[shift_cat] + ", Sign=" + sign_names[sign_cat]);
                            }
                            if (hits > 0 && hits < 10) {
                                low_hits.push_back(std::string(op_names[op]) + ", A=" + operand_names[cat_a] + ", B=" + operand_names[cat_b] + ", Shift=" + shift_names[shift_cat] + ", Sign=" + sign_names[sign_cat] + " -> " + std::to_string(hits));
                            }
                        }
                    }
                }
            }
        }
        double percentage = (static_cast<double>(covered_bins) / total_bins) * 100.0;
        oss << "\nCoverage: " << percentage << "% (" << covered_bins << "/" << total_bins << " bins hit)";
        if (!holes.empty()) {
            oss << "\nCoverage Holes (0 hits):";
            for (const auto& hole : holes) {
                oss << "\n  " << hole;
            }
        }
        if (!low_hits.empty()) {
            oss << "\nLow Hits (<10):";
            for (const auto& low : low_hits) {
                oss << "\n  " << low;
            }
        }
        return oss.str();
    }

    bool validate_coverage() const {
        for (int op = 0; op < 10; ++op) {
            for (int cat_a = 0; cat_a < NUM_OPERAND_CATEGORIES; ++cat_a) {
                for (int cat_b = 0; cat_b < NUM_OPERAND_CATEGORIES; ++cat_b) {
                    for (int shift_cat = 0; shift_cat < NUM_SHIFT_CATEGORIES; ++shift_cat) {
                        for (int sign_cat = 0; sign_cat < NUM_SIGN_CATEGORIES; ++sign_cat) {
                            if (!is_bin_reachable(static_cast<OperandCategory>(cat_a),
                                                  static_cast<OperandCategory>(cat_b),
                                                  static_cast<ShiftCategory>(shift_cat),
                                                  static_cast<SignCategory>(sign_cat))) {
                                continue;
                            }
                            if (cross_coverage_[op][cat_a][cat_b][shift_cat][sign_cat] < 10) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    // Returns every (op × A_category × B_category) 3-D bin whose total hit
    // count (summed over all shift and sign sub-dimensions) is below min_hits.
    // Use this to drive EXPECT_GE coverage-closure checks in tests.
    std::vector<CoverageHole> get_3d_coverage_holes(int min_hits = 10) const {
        std::vector<CoverageHole> holes;
        for (int op = 0; op < 10; ++op) {
            for (int cat_a = 0; cat_a < NUM_OPERAND_CATEGORIES; ++cat_a) {
                for (int cat_b = 0; cat_b < NUM_OPERAND_CATEGORIES; ++cat_b) {
                    int total = 0;
                    for (int sc = 0; sc < NUM_SHIFT_CATEGORIES; ++sc) {
                        for (int sg = 0; sg < NUM_SIGN_CATEGORIES; ++sg) {
                            total += cross_coverage_[op][cat_a][cat_b][sc][sg];
                        }
                    }
                    if (total < min_hits) {
                        holes.push_back({op,
                                         static_cast<OperandCategory>(cat_a),
                                         static_cast<OperandCategory>(cat_b),
                                         total});
                    }
                }
            }
        }
        return holes;
    }

    // Returns all reachable 5-D bins whose hit count is below threshold,
    // sorted ascending by hit count (weakest first).
    // This is the feedback signal consumed by CoverageDrivenSequence.
    std::vector<BinKey> get_sorted_weak_bins(int threshold = 10) const {
        std::vector<BinKey> weak;
        for (int op = 0; op < 10; ++op) {
            for (int ca = 0; ca < NUM_OPERAND_CATEGORIES; ++ca) {
                for (int cb = 0; cb < NUM_OPERAND_CATEGORIES; ++cb) {
                    for (int sc = 0; sc < NUM_SHIFT_CATEGORIES; ++sc) {
                        for (int sg = 0; sg < NUM_SIGN_CATEGORIES; ++sg) {
                            if (!is_bin_reachable(static_cast<OperandCategory>(ca),
                                                  static_cast<OperandCategory>(cb),
                                                  static_cast<ShiftCategory>(sc),
                                                  static_cast<SignCategory>(sg))) {
                                continue;
                            }
                            int hits = cross_coverage_[op][ca][cb][sc][sg];
                            if (hits < threshold) {
                                weak.push_back({op,
                                                static_cast<OperandCategory>(ca),
                                                static_cast<OperandCategory>(cb),
                                                static_cast<ShiftCategory>(sc),
                                                static_cast<SignCategory>(sg),
                                                hits});
                            }
                        }
                    }
                }
            }
        }
        std::sort(weak.begin(), weak.end(),
                  [](const BinKey& x, const BinKey& y) { return x.hits < y.hits; });
        return weak;
    }

private:
    std::array<int, 10> kOpCounts = {};
    int zero_count_ = 0;
    int shift_count_ = 0;
    int compare_count_ = 0;
    std::array<std::array<std::array<std::array<std::array<int, NUM_SIGN_CATEGORIES>, NUM_SHIFT_CATEGORIES>, NUM_OPERAND_CATEGORIES>, NUM_OPERAND_CATEGORIES>, 10> cross_coverage_ = {};
};
