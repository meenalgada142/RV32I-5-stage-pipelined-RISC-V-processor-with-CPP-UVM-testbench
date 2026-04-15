#pragma once
// pipe_agent.h — Verification agent (uvm_agent equivalent)
//
// Composes: PipeDriver + PipeMonitor.
// The agent is the single entry point used by PipeEnvUVM to run sequences.
//
// Changes vs original:
//   • Calls monitor.reset_sequence_counts() before each sequence so
//     pipe_env can compute per-sequence DUT commit counts accurately.

#include <iostream>
#include <string>

#include "obj_dir/Vrv32i_pipe5_with_branches.h"
#include "pipe_driver.h"
#include "pipe_monitor.h"
#include "pipe_scoreboard.h"
#include "pipe_sequence.h"

class PipeAgent {
public:
    PipeAgent(Vrv32i_pipe5_with_branches* dut,
              PipeScoreboard&             sb,
              int                         verbosity = 0)
        : dut_(dut),
          monitor_(dut, sb),
          driver_(dut, monitor_),
          verbosity_(verbosity) {}

    // -----------------------------------------------------------------------
    // Run one sequence:
    //   1. Reset per-sequence commit counters in the monitor.
    //   2. Load program into DUT imem (also clears dmem).
    //   3. Apply optional dmem preloads.
    //   4. Reset the pipeline.
    //   5. Clock until scoreboard all_matched() or timeout.
    //
    // Returns clock cycles consumed for this sequence.
    uint64_t run_sequence(const PipeSequence& seq, PipeScoreboard& sb) {
        if (verbosity_ >= 1)
            std::cout << "[agent] running: " << seq.name << "\n";

        // Reset per-sequence commit counters before this run
        monitor_.reset_sequence_counts();

        // Load program and optional data preloads
        driver_.load_program(seq.program);
        for (const auto& p : seq.preloads)
            driver_.preload_dmem(p.word_addr, p.data);

        // Reset pipeline
        driver_.reset(5);

        // Clock loop — drain 8 extra cycles after all_matched
        const uint64_t max = seq.max_cycles();
        while (driver_.tick(max)) {
            if (sb.all_matched()) {
                for (int drain = 0; drain < 8; ++drain) driver_.tick();
                break;
            }
        }

        if (verbosity_ >= 1) {
            std::cout << "[agent] done in " << driver_.cycle()
                      << " cycles: " << sb.summary() << "\n";
        }
        return driver_.cycle();
    }

    const PipeMonitor& monitor() const { return monitor_; }

private:
    Vrv32i_pipe5_with_branches* dut_;
    PipeMonitor  monitor_;
    PipeDriver   driver_;
    int          verbosity_;
};
