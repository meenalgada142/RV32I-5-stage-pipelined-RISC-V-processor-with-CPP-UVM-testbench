#pragma once
// decoder_env_uvm.h — Top-level verification environment (uvm_env equivalent)
//
// run_coverage_driven_flow() — 6-phase closure:
//   Phase 0: DirectedSequence        — hand-crafted edge cases (every opcode/variant)
//   Phase 1: IllegalEncodingSequence — all 6 illegal encoding kinds (Group F)
//   Phase 2: BoundaryImmSequence     — NEG_MIN/NEG_ONE/ZERO/POS_ONE/POS_MAX per class
//   Phase 3: RegisterZeroSequence    — rs1=x0/rs2=x0/rd=x0 per class (Group E)
//   Phase 4: RTypeFunct7Sequence     — full funct3×funct7 cross (Groups A+B)
//   Phase 5: BiasedRandomSequence    — 80% ADD/ADDI/LW baseline (breaks random dependency)
//   Phase 6+: CoverageDrivenSequence — feedback loop targeting remaining weak bins

#include <iostream>
#include <string>
#include <vector>

#include "decoder.h"
#include "decoder_agent.h"
#include "decoder_config.h"
#include "decoder_coverage.h"
#include "decoder_scoreboard.h"
#include "decoder_sequence.h"

class DecoderEnvUVM {
public:
    explicit DecoderEnvUVM(Decoder& dut, const std::string& config_path = "env")
        : cfg_(DecoderConfigDB::get_or_default(config_path)),
          scoreboard_(),
          coverage_(),
          agent_(dut, scoreboard_, cfg_) {
        agent_.attach_coverage(&coverage_);
    }

    // Single-sequence run (used directly in tests)
    void run_sequence(const DecoderSequence& seq, const std::string& name = "") {
        if (!name.empty() && cfg_.verbosity >= 2)
            std::cout << "[env] running: " << name << "\n";
        agent_.run_sequence(seq);
    }

    // Full multi-phase coverage-driven flow
    void run_coverage_driven_flow() {
        // Phase 0: Directed — deterministic, hits every legal encoding + boundary
        phase("0: directed");
        DirectedSequence directed;
        agent_.run_sequence(directed);
        print_summary("after directed");

        // Phase 1: Illegal encodings — seeds all Group F illegal bins
        phase("1: illegal encodings");
        IllegalEncodingSequence illegal;
        agent_.run_sequence(illegal);
        print_summary("after illegal");

        // Phase 2: Boundary immediates — closes Group D bins deterministically
        phase("2: boundary immediates");
        BoundaryImmSequence boundary(5);
        agent_.run_sequence(boundary);
        print_summary("after boundary");

        // Phase 3: Register zero patterns — closes Group E bins
        phase("3: register-zero patterns");
        RegisterZeroSequence regzero(5);
        agent_.run_sequence(regzero);
        print_summary("after register-zero");

        // Phase 4: R-type funct3×funct7 sweep — closes Groups A+B
        phase("4: R-type funct3×funct7");
        RTypeFunct7Sequence rtype_f7(10);
        agent_.run_sequence(rtype_f7);
        print_summary("after R-type sweep");

        // Phase 5: Biased random — baseline that CANNOT close coverage alone
        //          (80% ADD/ADDI/LW; 21 other classes are under-stimulated)
        phase("5: biased random (" + std::to_string(cfg_.num_transactions) + " txns)");
        BiasedRandomSequence biased(cfg_.num_transactions);
        agent_.run_sequence(biased);
        print_summary("after biased random");

        if (!cfg_.enable_coverage_driven) return;

        // Phase 6+: Coverage-driven feedback loop
        for (int pass = 1; pass <= cfg_.max_driven_passes; ++pass) {
            if (is_coverage_closed()) {
                if (cfg_.verbosity >= 1)
                    std::cout << "[env] coverage closed after " << pass - 1
                              << " driven pass(es)\n";
                break;
            }
            phase("6+" + std::to_string(pass) + ": driven pass " + std::to_string(pass));
            CoverageDrivenSequence driven(coverage_, cfg_.num_transactions,
                                          cfg_.coverage_threshold);
            agent_.run_sequence(driven);
            print_summary("after driven pass " + std::to_string(pass));
        }
    }

    // Accessors
    int         error_count()             const { return scoreboard_.error_count(); }
    int         assertion_violations()    const { return agent_.assertion_violations(); }
    bool        is_coverage_closed()      const { return coverage_.validate_coverage(cfg_.coverage_threshold); }
    std::string coverage_summary()        const { return coverage_.summary(); }
    std::string coverage_report()         const { return coverage_.report(); }
    const DecoderCoverage& coverage()     const { return coverage_; }

private:
    void phase(const std::string& name) const {
        if (cfg_.verbosity >= 2)
            std::cout << "[env] --- phase " << name << " ---\n";
    }
    void print_summary(const std::string& tag) const {
        if (cfg_.verbosity >= 1)
            std::cout << "[env] coverage " << tag << ": " << coverage_.summary() << "\n";
    }

    DecoderConfig     cfg_;
    DecoderScoreboard scoreboard_;
    DecoderCoverage   coverage_;
    DecoderAgent      agent_;
};
