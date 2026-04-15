#pragma once
// pipe_env.h — Top-level verification environment (uvm_env equivalent)
//
// run_sequence():
//   1. Build reference model commit list.
//   2. Load scoreboard with expected commits.
//   3. Sample coverage from reference trace.
//   4. Drive DUT via the agent.
//   5. Verify DUT commit count == reference commit count.
//   6. Optionally append one CSV row to the results log.
//
// Commit-count check (new):
//   After every sequence the env compares:
//     dut_commits (from monitor per-sequence counters)
//   vs
//     ref_commits (from reference model commit list size)
//   A mismatch means either flushed instructions leaked into WB (dut > ref)
//   or commits were silently dropped (dut < ref). Both are fatal bugs.
//
// CSV logging (new):
//   Call set_log_file("path/to/file.csv") once before running sequences.
//   Each sequence appends one row:
//     seed,name,ref_commits,dut_commits,commit_delta,matched,errors,
//     coverage,cycles

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "obj_dir/Vrv32i_pipe5_with_branches.h"
#include "pipe_agent.h"
#include "pipe_coverage.h"
#include "pipe_scoreboard.h"
#include "pipe_sequence.h"
#include "reference_model.h"

class PipeEnvUVM {
public:
    explicit PipeEnvUVM(Vrv32i_pipe5_with_branches* dut,
                        int verbosity = 0,
                        uint32_t seed_tag = 0)
        : agent_(dut, sb_, verbosity),
          verbosity_(verbosity),
          seed_tag_(seed_tag) {}

    // -----------------------------------------------------------------------
    // Optional: set a CSV file path. Opens (or creates) the file and writes
    // the header row if the file is empty. All subsequent run_sequence calls
    // append one row.
    void set_log_file(const std::string& path) {
        log_path_ = path;
        // Write header if file doesn't exist / is empty
        std::ifstream chk(path);
        bool need_header = !chk.good() || chk.peek() == std::ifstream::traits_type::eof();
        chk.close();
        if (need_header) {
            std::ofstream f(path, std::ios::app);
            f << "seed,name,ref_commits,dut_commits,commit_delta,"
              << "matched,errors,coverage,cycles\n";
        }
    }

    // -----------------------------------------------------------------------
    void run_sequence(const PipeSequence& seq) {
        // 1. Reference model
        ReferenceModel ref;
        ref.load_program(seq.program);
        for (const auto& p : seq.preloads)
            ref.preload_dmem(p.word_addr, p.data);

        const auto& steps   = ref.run();
        const auto  commits = ref.commit_list();
        const int   ref_cnt = (int)commits.size();

        if (verbosity_ >= 2) {
            std::cout << "[env] ref: " << steps.size()
                      << " instrs, " << ref_cnt << " commits\n";
        }

        // 2. Load scoreboard
        sb_.load_expected(commits);

        // 3. Coverage
        cov_.sample_trace(steps);

        // 4. Drive DUT
        const uint64_t cycles = agent_.run_sequence(seq, sb_);

        // 5. Commit-count check
        const int dut_cnt   = agent_.monitor().seq_total_commits();
        const int delta     = dut_cnt - ref_cnt;
        if (delta != 0) {
            ++total_errors_;
            std::cerr << "[env] COMMIT COUNT MISMATCH for \"" << seq.name
                      << "\": DUT=" << dut_cnt << " REF=" << ref_cnt;
            if (delta > 0)
                std::cerr << " (+" << delta << " phantom commits — flushed instr leaked to WB?)";
            else
                std::cerr << " (" << delta << " missing commits — WB suppressed?)";
            std::cerr << "\n";
        }

        // 6. Accumulate totals
        const int seq_errors = sb_.error_count() + (delta != 0 ? 1 : 0);
        total_errors_  += sb_.error_count();
        total_matched_ += sb_.matched_count();

        // 7. CSV logging
        if (!log_path_.empty()) {
            std::ofstream f(log_path_, std::ios::app);
            f << seed_tag_       << ","
              << seq.name        << ","
              << ref_cnt         << ","
              << dut_cnt         << ","
              << delta           << ","
              << sb_.matched_count() << ","
              << seq_errors      << ","
              << cov_.summary()  << ","
              << cycles          << "\n";
        }
    }

    // -----------------------------------------------------------------------
    int         error_count()        const { return total_errors_; }
    int         matched_count()      const { return total_matched_; }
    bool        is_coverage_closed() const { return cov_.validate_coverage(1); }
    std::string coverage_summary()   const { return cov_.summary(); }
    std::string coverage_report()    const { return cov_.report(); }

private:
    PipeScoreboard sb_;
    PipeCoverage   cov_;
    PipeAgent      agent_;
    int            verbosity_;
    uint32_t       seed_tag_      = 0;
    int            total_errors_  = 0;
    int            total_matched_ = 0;
    std::string    log_path_;
};
