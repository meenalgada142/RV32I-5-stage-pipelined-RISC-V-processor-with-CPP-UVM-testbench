#pragma once

#include <gtest/gtest.h>
#include "alu_transaction.h"
#include "alu_golden.h"

class ALUScoreboard {
public:
    void compare(const ALUTransaction& tx) {
        if (tx.actual != tx.expected) {
            ADD_FAILURE() << format_alu_message(tx)
                          << " actual=0x" << std::hex << tx.actual
                          << " expected=0x" << std::hex << tx.expected;
            ++error_count_;
        }
        if (tx.actual_zero != tx.expected_zero) {
            ADD_FAILURE() << format_alu_message(tx)
                          << " actual_zero=" << std::boolalpha << tx.actual_zero
                          << " expected_zero=" << std::boolalpha << tx.expected_zero;
            ++error_count_;
        }
    }

    int error_count() const { return error_count_; }

private:
    int error_count_ = 0;
};
