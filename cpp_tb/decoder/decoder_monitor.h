#pragma once
// decoder_monitor.h — Passive observer (uvm_monitor equivalent)

#include <functional>
#include "decoder_transaction.h"

class DecoderMonitor {
public:
    using callback_t = std::function<void(const DecoderTransaction&)>;

    explicit DecoderMonitor(callback_t cb = nullptr)
        : callback_(std::move(cb)) {}

    void observe(const DecoderTransaction& tx) const {
        if (callback_) callback_(tx);
    }

private:
    callback_t callback_;
};
