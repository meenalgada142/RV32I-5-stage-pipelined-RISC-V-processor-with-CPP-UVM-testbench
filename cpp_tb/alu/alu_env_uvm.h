#pragma once
// alu_env_uvm.h — Top-level verification environment (uvm_env equivalent)
//
// In SystemVerilog UVM:
//   class alu_env extends uvm_env;
//     alu_agent     agent;
//     alu_scoreboard scoreboard;
//     alu_coverage  coverage;
//   endclass
//
// ALUEnvUVM composes the agent, scoreboard, and coverage model.  It provides
// a high-level run_phase() equivalent — run_coverage_driven_flow() — which
// executes the full multi-pass coverage-closure methodology:
//
//   Phase 0 : Directed special-value sweep (guarantees special×special×op bins)
//   Phase 1 : Baseline random stress
//   Phase 2+ : Coverage-driven passes until closed (up to max_driven_passes)
//
// After each phase, a one-line summary is printed so progress is visible.

#include <iostream>
#include <string>
#include <vector>

#include "alu.h"
#include "alu_agent.h"
#include "alu_config.h"
#include "alu_coverage.h"
#include "alu_scoreboard.h"
#include "alu_sequence.h"

class ALUEnvUVM {
public:
    // Build from a DUT reference and a config path in ALUConfigDB.
    // Falls back to a default ALUConfig if the path is not registered.
    explicit ALUEnvUVM(ALU& dut, const std::string& config_path = "env")
        : cfg_(ALUConfigDB::get_or_default(config_path)),
          scoreboard_(),
          agent_(dut, scoreboard_, cfg_) {
        agent_.attach_coverage(&coverage_);
    }

    // ------------------------------------------------------------------
    // Explicit single-sequence run — mirrors env.run_sequence() from the
    // original ALUEnvironment, used directly in existing tests.
    // ------------------------------------------------------------------
    void run_sequence(const ALUSequence& seq, const std::string& name = "") {
        if (!name.empty() && cfg_.verbosity >= 2) {
            std::cout << "[env] running sequence: " << name << "\n";
        }
        agent_.run_sequence(seq);
    }

    // ------------------------------------------------------------------
    // Full coverage-driven flow — the "run_phase" equivalent.
    //
    //   1. Special-value sweep (15 repeats/combo)  → deterministic base
    //   2. Baseline random stress (cfg.num_transactions)
    //   3. Repeat up to cfg.max_driven_passes:
    //        CoverageDrivenSequence(threshold=cfg.coverage_threshold)
    //      until is_coverage_closed() or pass limit reached.
    // ------------------------------------------------------------------
    void run_coverage_driven_flow() {
        // Phase 0 — directed sweep
        print_phase("sweep");
        SpecialValueSweepSequence sweep(15);
        agent_.run_sequence(sweep);
        print_summary("after sweep");

        // Phase 1 — baseline random
        print_phase("baseline random (" + std::to_string(cfg_.num_transactions) + " txns)");
        RandomStressSequence baseline(cfg_.num_transactions);
        agent_.run_sequence(baseline);
        print_summary("after baseline");

        if (!cfg_.enable_coverage_driven) return;

        // Phase 2+ — coverage-driven passes
        for (int pass = 1; pass <= cfg_.max_driven_passes; ++pass) {
            if (is_coverage_closed()) {
                if (cfg_.verbosity >= 1)
                    std::cout << "[env] coverage closed after " << pass - 1
                              << " driven pass(es)\n";
                break;
            }
            print_phase("driven pass " + std::to_string(pass));
            CoverageDrivenSequence driven(coverage_, cfg_.num_transactions,
                                          cfg_.coverage_threshold);
            agent_.run_sequence(driven);
            print_summary("after driven pass " + std::to_string(pass));
        }
    }

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------
    int         error_count()          const { return scoreboard_.error_count(); }
    bool        is_coverage_closed()   const { return coverage_.validate_coverage(); }
    std::string coverage_summary()     const { return coverage_.summary(); }
    std::string coverage_report()      const { return coverage_.report(); }
    const ALUCoverage& coverage()      const { return coverage_; }

    std::vector<CoverageHole> get_3d_coverage_holes(int min_hits = 10) const {
        return coverage_.get_3d_coverage_holes(min_hits);
    }

private:
    void print_phase(const std::string& name) const {
        if (cfg_.verbosity >= 2)
            std::cout << "[env] --- phase: " << name << " ---\n";
    }

    void print_summary(const std::string& tag) const {
        if (cfg_.verbosity >= 1)
            std::cout << "[env] coverage " << tag << ": "
                      << coverage_.summary() << "\n";
    }

    ALUConfig    cfg_;
    ALUScoreboard scoreboard_;
    ALUCoverage  coverage_;
    ALUAgent     agent_;
};
