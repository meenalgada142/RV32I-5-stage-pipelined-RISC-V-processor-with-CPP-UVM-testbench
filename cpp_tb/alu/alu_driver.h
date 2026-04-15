#pragma once

#include "alu.h"
#include "alu_transaction.h"
#include "alu_golden.h"
#include "alu_scoreboard.h"
#include "alu_monitor.h"

class ALUDriver {
public:
    ALUDriver(ALU& dut, ALUScoreboard& scoreboard, const ALUMonitor& monitor)
        : dut_(dut), scoreboard_(scoreboard), monitor_(monitor) {}

    void drive(ALUTransaction& tx) {
        tx.expected = golden_alu(tx.a, tx.b, tx.op);
        tx.expected_zero = (tx.expected == 0u);

        dut_.set_inputs(tx.a, tx.b, tx.op);
        tx.actual = dut_.get_result();
        tx.actual_zero = dut_.get_zero();

        monitor_.observe(tx);
        scoreboard_.compare(tx);
    }

private:
    ALU& dut_;
    ALUScoreboard& scoreboard_;
    const ALUMonitor& monitor_;
};
