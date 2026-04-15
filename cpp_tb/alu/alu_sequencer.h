#pragma once
// alu_sequencer.h — Transaction sequencer (uvm_sequencer equivalent)
//
// In SystemVerilog UVM:
//   class alu_sequencer extends uvm_sequencer #(alu_tx);
//   endclass
//   // Sequence items are pulled from the sequencer by the driver via
//   // get_next_item() / item_done() TLM handshake.
//
// Here the sequencer owns the RNG, generates a complete transaction vector
// from a sequence object, and hands it to the driver one item at a time via
// the execute() call-back — preserving the same driver↔sequencer ownership
// boundary without TLM overhead.

#include <random>
#include <vector>

#include "alu_transaction.h"
#include "alu_sequence.h"
#include "alu_config.h"

class ALUSequencer {
public:
    explicit ALUSequencer(const ALUConfig& cfg = ALUConfig{})
        : cfg_(cfg) {
        if (cfg_.seed == 0) {
            std::random_device rd;
            rng_.seed(rd());
        } else {
            rng_.seed(cfg_.seed);
        }
    }

    // Run a sequence: generate all transactions then feed them one at a time
    // to the provided sink functor (the driver's drive() method).
    //
    //   sequencer.run(seq, [&](ALUTransaction& tx){ driver.drive(tx); });
    //
    // Returns the number of transactions dispatched.
    template <typename Sink>
    int run(const ALUSequence& seq, Sink&& sink) {
        auto transactions = seq.generate(rng_);
        for (auto& tx : transactions) {
            sink(tx);
        }
        return static_cast<int>(transactions.size());
    }

    // Direct access to the RNG for sequences that need it externally.
    std::mt19937_64& rng() { return rng_; }

    const ALUConfig& config() const { return cfg_; }

private:
    ALUConfig     cfg_;
    std::mt19937_64 rng_;
};
