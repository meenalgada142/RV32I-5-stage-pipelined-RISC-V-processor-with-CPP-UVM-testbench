#pragma once
// pipe_monitor.h — Per-cycle DUT observer (uvm_monitor equivalent)
//
// Sampling strategy:
//   pre_posedge()  — snapshot pipeline register outputs BEFORE clk rises.
//                    These are the values that always_ff blocks will latch
//                    (non-blocking RHS evaluated before posedge).
//   post_posedge() — report the snapshots to the scoreboard AFTER clk rises.
//
// Per-sequence commit accounting:
//   Call reset_sequence_counts() before each sequence run.
//   seq_reg_commits() / seq_mem_commits() return counts for that sequence only.
//   This lets pipe_env compare DUT commit count vs reference commit count.
//
// Note: The RTL does not carry PC through to MEM/WB, so the DUT-side
// CommitRecord has pc=0. Mismatches are identified by commit index in the log.

#include <cstdint>

#include "obj_dir/Vrv32i_pipe5_with_branches.h"
#include "obj_dir/Vrv32i_pipe5_with_branches___024root.h"
#include "pipe_scoreboard.h"

// Convenience macro: access an internal DUT signal via the rootp pointer.
#define PIPE_SIG(dut, sig) \
    ((dut)->rootp->rv32i_pipe5_with_branches__DOT__##sig)

class PipeMonitor {
public:
    PipeMonitor(Vrv32i_pipe5_with_branches* dut, PipeScoreboard& sb)
        : dut_(dut), sb_(sb) {}

    // -----------------------------------------------------------------------
    // Call BEFORE clk=1 / eval() — snapshot what is about to commit.
    void pre_posedge() {
        // WB stage: register-file write
        wb_rd_        = PIPE_SIG(dut_, mem_wb_rd)        & 0x1Fu;
        wb_regwrite_  = PIPE_SIG(dut_, mem_wb_reg_write) & 1u;
        wb_data_      = PIPE_SIG(dut_, wb_write_data);

        // MEM stage: data-memory store
        mem_write_    = PIPE_SIG(dut_, ex_mem_mem_write) & 1u;
        mem_addr_     = PIPE_SIG(dut_, ex_mem_alu_result) >> 2;  // byte→word
        mem_data_     = PIPE_SIG(dut_, ex_mem_rs2_data);

        snapped_ = true;
    }

    // -----------------------------------------------------------------------
    // Call AFTER clk=1 / eval() — report the snapped commits to scoreboard.
    void post_posedge() {
        if (!snapped_) return;
        snapped_ = false;

        if (wb_regwrite_ && wb_rd_ != 0) {
            sb_.actual_reg(wb_rd_, wb_data_);
            ++reg_commits_;
            ++seq_reg_commits_;
        }
        if (mem_write_) {
            sb_.actual_mem(mem_addr_, mem_data_);
            ++mem_commits_;
            ++seq_mem_commits_;
        }
        ++cycles_;
    }

    // -----------------------------------------------------------------------
    // Reset per-sequence counters. Call before each sequence run.
    void reset_sequence_counts() {
        seq_reg_commits_ = 0;
        seq_mem_commits_ = 0;
    }

    // -----------------------------------------------------------------------
    // Cumulative counters (across all sequences since construction).
    uint64_t cycle_count()      const { return cycles_; }
    int      reg_commit_count() const { return reg_commits_; }
    int      mem_commit_count() const { return mem_commits_; }

    // Per-sequence counters (reset by reset_sequence_counts()).
    int      seq_reg_commits()  const { return seq_reg_commits_; }
    int      seq_mem_commits()  const { return seq_mem_commits_; }
    int      seq_total_commits()const { return seq_reg_commits_ + seq_mem_commits_; }

    // Current IF-stage PC (for loop termination heuristics).
    uint32_t current_pc() const { return PIPE_SIG(dut_, pc); }

private:
    Vrv32i_pipe5_with_branches* dut_;
    PipeScoreboard&             sb_;

    // Pre-posedge snapshot
    bool     snapped_         = false;
    uint8_t  wb_rd_           = 0;
    bool     wb_regwrite_     = false;
    uint32_t wb_data_         = 0;
    bool     mem_write_       = false;
    uint32_t mem_addr_        = 0;
    uint32_t mem_data_        = 0;

    // Cumulative counters
    uint64_t cycles_          = 0;
    int      reg_commits_     = 0;
    int      mem_commits_     = 0;

    // Per-sequence counters
    int      seq_reg_commits_ = 0;
    int      seq_mem_commits_ = 0;
};
