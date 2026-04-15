#pragma once

#include <gtest/gtest.h>
#include <random>
#include <string>

#include "alu.h"
#include "alu_driver.h"
#include "alu_monitor.h"
#include "alu_scoreboard.h"
#include "alu_sequence.h"
#include "alu_coverage.h"

class ALUEnvironment {
public:
    explicit ALUEnvironment(ALU& dut)
        : dut_(dut), scoreboard_(), monitor_([this](const ALUTransaction& tx) { observe(tx); }), driver_(dut_, scoreboard_, monitor_), coverage_() {}

    void run_sequence(const ALUSequence& seq, const std::string& name) {
        std::random_device rd;
        std::mt19937_64 rng(rd());
        auto transactions = seq.generate(rng);

        SCOPED_TRACE("ALUEnvironment sequence=" + name);
        for (auto& tx : transactions) {
            driver_.drive(tx);
            coverage_.sample(tx);
        }
    }

    int error_count() const {
        return scoreboard_.error_count();
    }

    std::string coverage_report() const { return coverage_.report(); }
    std::string coverage_summary() const { return coverage_.summary(); }

    std::vector<CoverageHole> get_3d_coverage_holes(int min_hits = 10) const {
        return coverage_.get_3d_coverage_holes(min_hits);
    }

    // Read-only access to the coverage model — used by CoverageDrivenSequence
    // to snapshot weak bins at the start of each new pass.
    const ALUCoverage& coverage() const { return coverage_; }

    // True once every reachable 5-D bin has reached ≥ 10 hits.
    bool is_coverage_closed() const { return coverage_.validate_coverage(); }

private:
    void observe(const ALUTransaction& tx) const {
        (void)tx;
    }

    ALU& dut_;
    ALUScoreboard scoreboard_;
    ALUMonitor monitor_;
    ALUDriver driver_;
    ALUCoverage coverage_;
};
