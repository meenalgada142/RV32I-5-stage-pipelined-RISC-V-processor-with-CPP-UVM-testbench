#pragma once
// alu_agent.h — Verification agent (uvm_agent equivalent)
//
// In SystemVerilog UVM:
//   class alu_agent extends uvm_agent;
//     alu_driver    driver;
//     alu_monitor   monitor;
//     alu_sequencer sequencer;
//     alu_config    cfg;
//     function void build_phase(uvm_phase phase);
//       if (cfg.is_active == UVM_ACTIVE) begin
//         driver    = alu_driver::type_id::create("driver", this);
//         sequencer = alu_sequencer::type_id::create("sequencer", this);
//       end
//       monitor = alu_monitor::type_id::create("monitor", this);
//     endfunction
//   endclass
//
// ALUAgent owns a sequencer, a driver (active mode only), and a monitor.
// In passive mode only the monitor runs — used for checking without driving.
//
// The agent exposes run_sequence() which wires the sequencer → driver → DUT
// in one call, mirroring UVM's start() / run_phase() flow.

#include "alu_if.h"
#include "alu_config.h"
#include "alu_driver.h"
#include "alu_monitor.h"
#include "alu_scoreboard.h"
#include "alu_sequencer.h"
#include "alu_coverage.h"

class ALUAgent {
public:
    // Construct from a virtual interface handle, scoreboard, and config.
    // The agent does NOT own the scoreboard — it is shared with the env.
    ALUAgent(ALU& dut, ALUScoreboard& scoreboard, const ALUConfig& cfg)
        : cfg_(cfg),
          monitor_([this](const ALUTransaction& tx){ on_observe(tx); }),
          driver_(dut, scoreboard, monitor_),
          sequencer_(cfg) {}

    // Run a sequence through the sequencer → driver pipeline.
    // Returns number of transactions driven.
    int run_sequence(const ALUSequence& seq) {
        if (!cfg_.is_active) return 0;   // passive agent: do not drive
        return sequencer_.run(seq, [this](ALUTransaction& tx){
            driver_.drive(tx);
        });
    }

    // Sample coverage after every transaction (call from env if needed).
    void attach_coverage(ALUCoverage* cov) { coverage_ = cov; }

    const ALUConfig& config() const { return cfg_; }

private:
    void on_observe(const ALUTransaction& tx) {
        if (coverage_) coverage_->sample(tx);
    }

    ALUConfig     cfg_;
    ALUMonitor    monitor_;
    ALUDriver     driver_;
    ALUSequencer  sequencer_;
    ALUCoverage*  coverage_ = nullptr;
};
