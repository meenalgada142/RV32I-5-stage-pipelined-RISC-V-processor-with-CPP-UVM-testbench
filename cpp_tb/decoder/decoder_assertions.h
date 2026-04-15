#pragma once
// decoder_assertions.h — Self-checking property assertions on decoder outputs
//
// Each assert_* function checks one structural invariant.
// Violations call ADD_FAILURE() so they integrate with GoogleTest.
// Call check_all() after every transaction for full coverage.

#include <gtest/gtest.h>
#include <sstream>
#include <iomanip>
#include "decoder_transaction.h"
#include "decoder_golden.h"

class DecoderAssertions {
public:
    // Run all assertions on one transaction. Returns false if any fail.
    bool check_all(const DecoderTransaction& tx) {
        bool ok = true;
        ok &= assert_mutual_exclusion(tx);
        ok &= assert_reg_write_consistency(tx);
        ok &= assert_mem_consistency(tx);
        ok &= assert_alu_src_consistency(tx);
        ok &= assert_branch_jump_exclusive(tx);
        ok &= assert_illegal_opcode_default(tx);
        ok &= assert_rs_fields_preserved(tx);
        return ok;
    }

    int violation_count() const { return violations_; }

private:
    int violations_ = 0;

    std::string ctx(const DecoderTransaction& tx) const {
        std::ostringstream o;
        o << "instr=0x" << std::hex << std::setw(8) << std::setfill('0')
          << tx.instr << " (" << tx.label << ")";
        return o.str();
    }

    void fail(const DecoderTransaction& tx, const char* msg) {
        ADD_FAILURE() << "[ASSERT] " << msg << " — " << ctx(tx);
        ++violations_;
    }

    // 1. mem_read and mem_write cannot both be true simultaneously
    bool assert_mutual_exclusion(const DecoderTransaction& tx) {
        if (tx.act_mem_read && tx.act_mem_write) {
            fail(tx, "mem_read AND mem_write both asserted");
            return false;
        }
        return true;
    }

    // 2. reg_write must be false for stores and branches
    bool assert_reg_write_consistency(const DecoderTransaction& tx) {
        const uint8_t op = tx.instr & 0x7Fu;
        bool is_store  = (op == 0x23);
        bool is_branch = (op == 0x63);
        if ((is_store || is_branch) && tx.act_reg_write) {
            fail(tx, "reg_write asserted for store/branch");
            return false;
        }
        return true;
    }

    // 3. mem_to_reg can only be true when mem_read is also true
    bool assert_mem_consistency(const DecoderTransaction& tx) {
        if (tx.act_mem_to_reg && !tx.act_mem_read) {
            fail(tx, "mem_to_reg set without mem_read");
            return false;
        }
        return true;
    }

    // 4. alu_src must be false for R-type (both operands are registers)
    bool assert_alu_src_consistency(const DecoderTransaction& tx) {
        const uint8_t op = tx.instr & 0x7Fu;
        if (op == 0x33 && tx.act_alu_src) {
            fail(tx, "alu_src asserted for R-type instruction");
            return false;
        }
        return true;
    }

    // 5. branch and jump cannot both be true
    bool assert_branch_jump_exclusive(const DecoderTransaction& tx) {
        if (tx.act_branch && tx.act_jump) {
            fail(tx, "branch AND jump both asserted");
            return false;
        }
        return true;
    }

    // 6. Unknown opcode must produce all-zero control outputs
    bool assert_illegal_opcode_default(const DecoderTransaction& tx) {
        const uint8_t op = tx.instr & 0x7Fu;
        const bool known = (op==0x33||op==0x13||op==0x03||op==0x23||op==0x63||op==0x6F);
        if (known) return true;
        bool any_set = tx.act_reg_write || tx.act_mem_read || tx.act_mem_write ||
                       tx.act_mem_to_reg || tx.act_alu_src || tx.act_branch || tx.act_jump;
        if (any_set) {
            fail(tx, "illegal opcode produced non-zero control signals");
            return false;
        }
        return true;
    }

    // 7. rs1/rs2/rd register fields must be preserved verbatim from instruction bits
    bool assert_rs_fields_preserved(const DecoderTransaction& tx) {
        bool ok = true;
        uint8_t exp_rs1 = (tx.instr >> 15) & 0x1F;
        uint8_t exp_rs2 = (tx.instr >> 20) & 0x1F;
        uint8_t exp_rd  = (tx.instr >>  7) & 0x1F;
        if (tx.act_rs1 != exp_rs1) { fail(tx, "rs1 field corrupted"); ok = false; }
        if (tx.act_rs2 != exp_rs2) { fail(tx, "rs2 field corrupted"); ok = false; }
        if (tx.act_rd  != exp_rd)  { fail(tx, "rd field corrupted");  ok = false; }
        return ok;
    }
};
