#pragma once
// decoder_sequencer.h — Owns the RNG and drives sequences to a sink functor

#include <random>
#include "decoder_config.h"
#include "decoder_sequence.h"

class DecoderSequencer {
public:
    explicit DecoderSequencer(const DecoderConfig& cfg = DecoderConfig{}) : cfg_(cfg) {
        if (cfg_.seed == 0) {
            std::random_device rd;
            rng_.seed(rd());
        } else {
            rng_.seed(cfg_.seed);
        }
    }

    template <typename Sink>
    int run(const DecoderSequence& seq, Sink&& sink) {
        auto txns = seq.generate(rng_);
        for (auto& tx : txns) sink(tx);
        return static_cast<int>(txns.size());
    }

    std::mt19937_64& rng() { return rng_; }

private:
    DecoderConfig   cfg_;
    std::mt19937_64 rng_;
};
