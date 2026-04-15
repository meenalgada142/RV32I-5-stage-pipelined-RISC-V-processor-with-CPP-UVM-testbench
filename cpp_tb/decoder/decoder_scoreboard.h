#pragma once
// decoder_scoreboard.h — Field-by-field comparison of decoded vs expected

#include <gtest/gtest.h>
#include <sstream>
#include <iomanip>
#include "decoder_transaction.h"
#include "decoder_golden.h"

class DecoderScoreboard {
public:
    void compare(const DecoderTransaction& tx) {
        bool ok = true;
        std::ostringstream oss;
        oss << "[SCOREBOARD] instr=0x" << std::hex << std::setw(8) << std::setfill('0')
            << tx.instr << " (" << tx.label << ")\n";

        auto check_field = [&](const char* name, uint32_t exp, uint32_t act) {
            if (exp != act) {
                oss << "  MISMATCH " << name
                    << " exp=0x" << std::hex << exp
                    << " act=0x" << std::hex << act << "\n";
                ok = false;
            }
        };
        auto check_bool = [&](const char* name, bool exp, bool act) {
            if (exp != act) {
                oss << "  MISMATCH " << name
                    << " exp=" << exp << " act=" << act << "\n";
                ok = false;
            }
        };

        check_field("rs1",        tx.exp_rs1,        tx.act_rs1);
        check_field("rs2",        tx.exp_rs2,        tx.act_rs2);
        check_field("rd",         tx.exp_rd,         tx.act_rd);
        check_field("imm",        tx.exp_imm,        tx.act_imm);
        check_field("alu_op",     tx.exp_alu_op,     tx.act_alu_op);
        check_bool ("reg_write",  tx.exp_reg_write,  tx.act_reg_write);
        check_bool ("mem_read",   tx.exp_mem_read,   tx.act_mem_read);
        check_bool ("mem_write",  tx.exp_mem_write,  tx.act_mem_write);
        check_bool ("mem_to_reg", tx.exp_mem_to_reg, tx.act_mem_to_reg);
        check_bool ("alu_src",    tx.exp_alu_src,    tx.act_alu_src);
        check_bool ("branch",     tx.exp_branch,     tx.act_branch);
        check_bool ("branch_type",tx.exp_branch_type,tx.act_branch_type);
        check_bool ("jump",       tx.exp_jump,       tx.act_jump);

        if (!ok) {
            ADD_FAILURE() << oss.str();
            ++error_count_;
        }
    }

    int error_count() const { return error_count_; }

private:
    int error_count_ = 0;
};
