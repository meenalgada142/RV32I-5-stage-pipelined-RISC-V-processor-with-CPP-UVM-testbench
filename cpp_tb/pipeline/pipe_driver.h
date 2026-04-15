#pragma once
// pipe_driver.h — Program loader + clock controller (uvm_driver equivalent)
//
// Responsibilities:
//   1. load_program()  — write instruction words directly into DUT's instr_mem[]
//   2. preload_dmem()  — write initial data words into DUT's data_mem[]
//   3. reset()         — assert rst for N cycles, then deassert
//   4. tick()          — advance one full clock cycle (negedge→pre_snap→posedge→post_snap)
//
// The monitor is wired in and called automatically on every tick().
//
// Inline assertions (checked every posedge):
//   A1 — jump_taken and branch_taken are mutually exclusive
//   A2 — jump_taken only when EX opcode is JAL (0x6F)
//   A3 — branch_taken only when EX opcode is BEQ/BNE (0x63)
//   A4 — PC after jump equals id_ex_pc + id_ex_imm
//   A5 — PC after taken branch equals id_ex_pc + id_ex_imm
//   A6 — PC=0 only when rst was active or a control transfer targeted 0
//   A7 — load-use bubble must have all control signals cleared
//   A8 — branch-flush bubble must have all control signals cleared

#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "obj_dir/Vrv32i_pipe5_with_branches.h"
#include "obj_dir/Vrv32i_pipe5_with_branches___024root.h"
#include "pipe_monitor.h"

class PipeDriver {
public:
    PipeDriver(Vrv32i_pipe5_with_branches* dut, PipeMonitor& monitor)
        : dut_(dut), monitor_(monitor) {}

    // -----------------------------------------------------------------------
    // Write a program into DUT instruction memory (word-addressed).
    // Also clears data_mem so each test starts from a clean memory state.
    // Must be called before reset().
    void load_program(const std::vector<uint32_t>& prog) {
        auto& imem = PIPE_SIG(dut_, instr_mem);
        auto& dmem = PIPE_SIG(dut_, data_mem);
        // Clear both memories so state never leaks between tests
        for (int i = 0; i < 256; ++i) { imem[i] = 0u; dmem[i] = 0u; }
        for (size_t i = 0; i < prog.size() && i < 256; ++i)
            imem[i] = prog[i];
    }

    // -----------------------------------------------------------------------
    // Pre-load data memory (useful for LW tests).
    void preload_dmem(uint32_t word_addr, uint32_t data) {
        if (word_addr < 256)
            PIPE_SIG(dut_, data_mem)[word_addr] = data;
    }

    // -----------------------------------------------------------------------
    // Assert reset for rst_cycles half-periods, then release.
    void reset(int rst_cycles = 5) {
        dut_->rst = 1;
        for (int i = 0; i < rst_cycles; ++i) {
            dut_->clk = 0; dut_->eval();
            dut_->clk = 1; dut_->eval();
        }
        dut_->rst = 0;
        dut_->eval();
        cycle_ = 0;
    }

    // -----------------------------------------------------------------------
    // Advance one full clock cycle.
    // Returns false if max_cycles reached (caller should stop).
    bool tick(uint64_t max_cycles = 0) {
        if (max_cycles > 0 && cycle_ >= max_cycles) return false;

        // Negedge — combinational settles from previous posedge
        dut_->clk = 0;
        dut_->eval();

        // ------------------------------------------------------------------
        // Pre-posedge: snapshot combinational values BEFORE the flip-flops
        // latch. These are the values assertions should check — they reflect
        // what the pipeline is *about* to commit this cycle.
        // ------------------------------------------------------------------
        snap_pre_posedge();

        // Snapshot signals that are about to commit
        monitor_.pre_posedge();

        // Posedge — flip-flops latch new values
        dut_->clk = 1;
        dut_->eval();

        // ------------------------------------------------------------------
        // Post-posedge: check invariants using the pre-posedge snapshot.
        // PC has now been updated; check it against expected value.
        // ------------------------------------------------------------------
        check_assertions();

        // Report what just committed
        monitor_.post_posedge();

        ++cycle_;
        return true;
    }

    uint64_t cycle() const { return cycle_; }

private:
    Vrv32i_pipe5_with_branches* dut_;
    PipeMonitor&                monitor_;
    uint64_t                    cycle_ = 0;

    // Pre-posedge snapshot used by assertions
    uint32_t snap_pc_          = 0;
    uint32_t snap_id_ex_pc_    = 0;
    uint32_t snap_id_ex_imm_   = 0;
    uint32_t snap_id_ex_instr_ = 0;
    bool     snap_jump_taken_  = false;
    bool     snap_branch_taken_= false;
    bool     snap_insert_bubble_ = false;
    bool     snap_flush_id_ex_   = false;
    bool     snap_rst_was_active_ = false;

    // ------------------------------------------------------------------
    // Capture combinational signals before posedge
    // ------------------------------------------------------------------
    void snap_pre_posedge() {
        snap_pc_            = PIPE_SIG(dut_, pc);
        snap_id_ex_pc_      = PIPE_SIG(dut_, id_ex_pc);
        snap_id_ex_imm_     = PIPE_SIG(dut_, id_ex_imm);
        snap_id_ex_instr_   = PIPE_SIG(dut_, id_ex_instr);  // instr currently in EX
        snap_jump_taken_    = (bool)(PIPE_SIG(dut_, jump_taken)   & 1u);
        snap_branch_taken_  = (bool)(PIPE_SIG(dut_, branch_taken) & 1u);
        snap_insert_bubble_ = (bool)(PIPE_SIG(dut_, insert_bubble)& 1u);
        snap_flush_id_ex_   = (bool)(PIPE_SIG(dut_, flush_id_ex)  & 1u);
        snap_rst_was_active_= (bool)(dut_->rst & 1u);
    }

    // ------------------------------------------------------------------
    // Assertion helper: print context and abort
    // ------------------------------------------------------------------
    void fail_assert(const char* name, const char* msg) const {
        std::fprintf(stderr,
            "\n[ASSERT FAIL] cycle=%" PRIu64 " %s: %s\n"
            "  PC=0x%08X  id_ex_pc=0x%08X  id_ex_imm=0x%08X\n"
            "  jump_taken=%d  branch_taken=%d\n"
            "  insert_bubble=%d  flush_id_ex=%d\n",
            cycle_, name, msg,
            snap_pc_, snap_id_ex_pc_, snap_id_ex_imm_,
            (int)snap_jump_taken_, (int)snap_branch_taken_,
            (int)snap_insert_bubble_, (int)snap_flush_id_ex_);
        std::abort();
    }

    // ------------------------------------------------------------------
    // All assertions — called after posedge so PC$next is observable
    // ------------------------------------------------------------------
    void check_assertions() {
        if (snap_rst_was_active_) return;  // don't check during reset

        const uint32_t new_pc = PIPE_SIG(dut_, pc);

        // A1 — jump_taken and branch_taken are mutually exclusive.
        //      Both require an instruction in EX; a single instruction
        //      cannot be both a JAL and a BEQ/BNE.
        if (snap_jump_taken_ && snap_branch_taken_)
            fail_assert("A1",
                "jump_taken && branch_taken simultaneously — impossible for one instruction");

        // A2 — jump_taken only fires on a JAL opcode (7'h6F).
        //      Uses snap_id_ex_instr_ (pre-posedge) because flush_id_ex fires in
        //      the same cycle as jump_taken, zeroing id_ex_instr at posedge.
        //      Reading id_ex_instr post-posedge would always see 0 — wrong.
        if (snap_jump_taken_) {
            const uint32_t id_ex_op = snap_id_ex_instr_ & 0x7Fu;
            if (id_ex_op != 0x6Fu)
                fail_assert("A2",
                    "jump_taken=1 but id_ex opcode is not JAL (0x6F) — "
                    "flush/bubble did not zero id_ex_jump");
        }

        // A3 — branch_taken only fires on a BEQ/BNE opcode (7'h63).
        //      Same pre-posedge snapshot reasoning as A2.
        if (snap_branch_taken_) {
            const uint32_t id_ex_op = snap_id_ex_instr_ & 0x7Fu;
            if (id_ex_op != 0x63u)
                fail_assert("A3",
                    "branch_taken=1 but id_ex opcode is not branch (0x63) — "
                    "flush/bubble did not zero id_ex_branch");
        }

        // A4 — After a jump, PC must equal id_ex_pc + id_ex_imm (JAL target).
        if (snap_jump_taken_) {
            const uint32_t expected_target =
                snap_id_ex_pc_ + snap_id_ex_imm_;
            if (new_pc != expected_target)
                fail_assert("A4",
                    "PC after JAL does not equal id_ex_pc + id_ex_imm — "
                    "pc_next mux or jump_target computation wrong");
        }

        // A5 — After a taken branch, PC must equal id_ex_pc + id_ex_imm.
        if (snap_branch_taken_) {
            const uint32_t expected_target =
                snap_id_ex_pc_ + snap_id_ex_imm_;
            if (new_pc != expected_target)
                fail_assert("A5",
                    "PC after taken branch does not equal id_ex_pc + id_ex_imm — "
                    "branch_pc_next mux or imm_b extraction wrong");
        }

        // A6 — PC=0 is only legal when:
        //   (a) rst was just deasserted (reset to 0 is architectural)
        //   (b) a control transfer arithmetically targeted address 0
        //      (snap_id_ex_pc_ + snap_id_ex_imm_ == 0)
        //   Any other PC=0 is a pipeline corruption.
        if (new_pc == 0u) {
            const bool ctrl_targeted_zero =
                (snap_jump_taken_ || snap_branch_taken_) &&
                ((snap_id_ex_pc_ + snap_id_ex_imm_) == 0u);
            if (!ctrl_targeted_zero && cycle_ > 0)
                fail_assert("A6",
                    "PC=0 without rst or a control transfer to address 0 — "
                    "spurious reset, stale PC, or mux select fault");
        }

        // A7 — After a load-use bubble, ID/EX control signals must all be 0.
        //      A stale id_ex_reg_write, id_ex_mem_read, or id_ex_jump in a
        //      bubble would cause phantom commits or spurious redirects.
        if (snap_insert_bubble_) {
            const bool reg_w  = (bool)(PIPE_SIG(dut_, id_ex_reg_write)  & 1u);
            const bool mem_r  = (bool)(PIPE_SIG(dut_, id_ex_mem_read)   & 1u);
            const bool mem_w  = (bool)(PIPE_SIG(dut_, id_ex_mem_write)  & 1u);
            const bool jmp    = (bool)(PIPE_SIG(dut_, id_ex_jump)       & 1u);
            const bool br     = (bool)(PIPE_SIG(dut_, id_ex_branch)     & 1u);
            if (reg_w || mem_r || mem_w || jmp || br)
                fail_assert("A7",
                    "load-use bubble has non-zero control signals — "
                    "insert_bubble did not zero id_ex control fields");
        }

        // A8 — After a branch/jump flush, ID/EX control signals must all be 0.
        if (snap_flush_id_ex_) {
            const bool reg_w  = (bool)(PIPE_SIG(dut_, id_ex_reg_write)  & 1u);
            const bool mem_r  = (bool)(PIPE_SIG(dut_, id_ex_mem_read)   & 1u);
            const bool mem_w  = (bool)(PIPE_SIG(dut_, id_ex_mem_write)  & 1u);
            const bool jmp    = (bool)(PIPE_SIG(dut_, id_ex_jump)       & 1u);
            const bool br     = (bool)(PIPE_SIG(dut_, id_ex_branch)     & 1u);
            if (reg_w || mem_r || mem_w || jmp || br)
                fail_assert("A8",
                    "branch/jump flush did not zero id_ex control signals — "
                    "flushed instruction can still commit or redirect PC");
        }
    }
};
