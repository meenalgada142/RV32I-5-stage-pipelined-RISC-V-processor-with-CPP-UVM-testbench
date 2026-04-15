#pragma once

#include <functional>
#include "alu_transaction.h"

class ALUMonitor {
public:
    using callback_t = std::function<void(const ALUTransaction&)>;

    explicit ALUMonitor(callback_t callback = nullptr)
        : callback_(std::move(callback)) {}

    void observe(const ALUTransaction& tx) const {
        if (callback_) {
            callback_(tx);
        }
    }

private:
    callback_t callback_;
};
