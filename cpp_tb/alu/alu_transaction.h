#pragma once

#include <cstdint>
#include <string>

struct ALUTransaction {
    uint32_t a = 0;
    uint32_t b = 0;
    uint8_t op = 0;
    std::string label;
    uint32_t expected = 0;
    uint32_t actual = 0;
    bool expected_zero = false;
    bool actual_zero = false;

    ALUTransaction() = default;
    ALUTransaction(uint32_t a_, uint32_t b_, uint8_t op_, std::string label_ = "")
        : a(a_), b(b_), op(op_), label(std::move(label_)) {}
};
