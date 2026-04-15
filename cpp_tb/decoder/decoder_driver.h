#pragma once
// decoder_driver.h — Drives the Decoder DUT and populates tx.act_* fields

#include "decoder.h"
#include "decoder_transaction.h"
#include "decoder_golden.h"
#include "decoder_scoreboard.h"
#include "decoder_monitor.h"
#include "decoder_assertions.h"

class DecoderDriver {
public:
    DecoderDriver(Decoder& dut, DecoderScoreboard& sb, const DecoderMonitor& mon)
        : dut_(dut), scoreboard_(sb), monitor_(mon) {}

    void drive(DecoderTransaction& tx) {
        // 1. Fill expected fields from golden model
        golden_decode(tx);

        // 2. Apply stimulus to DUT and capture actual outputs
        DecodedInstruction act = dut_.decode(tx.instr);
        tx.act_rs1         = act.rs1;
        tx.act_rs2         = act.rs2;
        tx.act_rd          = act.rd;
        tx.act_imm         = act.imm;
        tx.act_alu_op      = act.alu_op;
        tx.act_reg_write   = act.reg_write;
        tx.act_mem_read    = act.mem_read;
        tx.act_mem_write   = act.mem_write;
        tx.act_mem_to_reg  = act.mem_to_reg;
        tx.act_alu_src     = act.alu_src;
        tx.act_branch      = act.branch;
        tx.act_branch_type = act.branch_type;
        tx.act_jump        = act.jump;

        // 3. Structural assertions on DUT outputs
        assertions_.check_all(tx);

        // 4. Broadcast completed transaction
        monitor_.observe(tx);

        // 5. Score expected vs actual
        scoreboard_.compare(tx);
    }

    int assertion_violations() const { return assertions_.violation_count(); }

private:
    Decoder&              dut_;
    DecoderScoreboard&    scoreboard_;
    const DecoderMonitor& monitor_;
    DecoderAssertions     assertions_;
};
