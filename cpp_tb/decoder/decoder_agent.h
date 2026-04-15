#pragma once
// decoder_agent.h — Verification agent (uvm_agent equivalent)
//
// Composes: sequencer + driver + monitor.
// Passive mode (is_active=false) skips driving entirely.
// Coverage is sampled through the monitor callback.

#include "decoder.h"
#include "decoder_config.h"
#include "decoder_coverage.h"
#include "decoder_driver.h"
#include "decoder_monitor.h"
#include "decoder_scoreboard.h"
#include "decoder_sequencer.h"

class DecoderAgent {
public:
    DecoderAgent(Decoder& dut, DecoderScoreboard& sb, const DecoderConfig& cfg)
        : cfg_(cfg),
          monitor_([this](const DecoderTransaction& tx){ on_observe(tx); }),
          driver_(dut, sb, monitor_),
          sequencer_(cfg) {}

    int run_sequence(const DecoderSequence& seq) {
        if (!cfg_.is_active) return 0;
        return sequencer_.run(seq, [this](DecoderTransaction& tx){
            driver_.drive(tx);
        });
    }

    void attach_coverage(DecoderCoverage* cov) { coverage_ = cov; }

    int assertion_violations() const { return driver_.assertion_violations(); }

private:
    void on_observe(const DecoderTransaction& tx) {
        if (coverage_) coverage_->sample(tx);
    }

    DecoderConfig     cfg_;
    DecoderMonitor    monitor_;
    DecoderDriver     driver_;
    DecoderSequencer  sequencer_;
    DecoderCoverage*  coverage_ = nullptr;
};
