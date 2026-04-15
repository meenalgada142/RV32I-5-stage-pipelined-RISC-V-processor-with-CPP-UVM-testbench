
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>

#include "alu_transaction.h"

static const std::array<const char*, 10> kAluOpNames = {
    "ADD",
    "SUB",
    "AND",
    "OR",
    "XOR",
    "SLT",
    "SLL",
    "SRL",
    "SLTU",
    "SRA"
};

static std::string op_name(uint8_t op) {
    return op < kAluOpNames.size() ? kAluOpNames[op] : "UNKNOWN";
}

static uint32_t golden_alu(uint32_t a, uint32_t b, uint8_t op) {
    switch (op) {
        case 0: // ADD
            return a + b;
        case 1: // SUB
            return a - b;
        case 2: // AND
            return a & b;
        case 3: // OR
            return a | b;
        case 4: // XOR
            return a ^ b;
        case 5: // SLT
            return (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u;
        case 6: // SLL
            return a << (b & 0x1F);
        case 7: // SRL
            return a >> (b & 0x1F);
        case 8: // SLTU
            return (a < b) ? 1u : 0u;
        case 9: // SRA
            return static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1F));
        default:
            return 0u;
    }
}

static std::string format_alu_message(const ALUTransaction& tx) {
    std::ostringstream oss;
    oss << "op=" << op_name(tx.op) << "(" << static_cast<int>(tx.op) << ")"
        << " a=0x" << std::hex << tx.a
        << " b=0x" << std::hex << tx.b;
    if (!tx.label.empty()) {
        oss << " - " << tx.label;
    }
    return oss.str();
}

static uint32_t biased_operand(std::mt19937_64& rng) {
    static const std::array<uint32_t, 7> kWeightedValues = {
        0u,
        1u,
        0xFFFFFFFFu,
        0x7FFFFFFFu,
        0x80000000u,
        0xAAAAAAAAu,
        0x55555555u
    };

    std::uniform_int_distribution<uint32_t> random32(0, std::numeric_limits<uint32_t>::max());
    std::uniform_int_distribution<int> choice(0, 9);
    int index = choice(rng);
    if (index < 3) {  // 30% chance to pick from biased pool
        std::uniform_int_distribution<size_t> pool_choice(0, kWeightedValues.size() - 1);
        return kWeightedValues[pool_choice(rng)];
    }
    return random32(rng);
}

static uint8_t random_op(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> op_dist(0, 9);
    return static_cast<uint8_t>(op_dist(rng));
}
