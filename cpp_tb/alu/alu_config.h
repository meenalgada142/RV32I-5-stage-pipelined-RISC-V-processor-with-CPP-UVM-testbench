#pragma once
// alu_config.h — Configuration object + static config database
//
// In SystemVerilog UVM:
//   class alu_config extends uvm_object;
//     bit is_active = 1;
//     int num_transactions = 10000;
//     ...
//   endclass
//
//   uvm_config_db #(alu_config)::set(this, "env.agent", "cfg", my_cfg);
//   uvm_config_db #(alu_config)::get(this, "", "cfg", cfg);
//
// ALUConfig is the plain-data equivalent of the uvm_config_object.
// ALUConfigDB is the static key-value store that replaces the UVM resource
// database — same set/get API, scoped by a string path.

#include <string>
#include <unordered_map>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Configuration object — holds all knobs for one test run
// ---------------------------------------------------------------------------
struct ALUConfig {
    // Agent mode: true = drive + monitor (active), false = monitor-only (passive)
    bool is_active = true;

    // Base transaction count for random sequences
    int num_transactions = 10000;

    // Coverage-closure threshold: every reachable bin must reach this many hits
    int coverage_threshold = 10;

    // Max coverage-driven passes before giving up
    int max_driven_passes = 5;

    // Log verbosity: 0=silent, 1=summary, 2=per-test, 3=full trace
    int verbosity = 1;

    // Enable the feedback-loop coverage-driven phase
    bool enable_coverage_driven = true;

    // RNG seed (0 = use std::random_device)
    uint64_t seed = 0;
};

// ---------------------------------------------------------------------------
// Static configuration database — uvm_config_db #(ALUConfig) equivalent
// ---------------------------------------------------------------------------
class ALUConfigDB {
public:
    // Store a config under the given path (e.g. "env", "env.agent").
    // If a config already exists at that path it is overwritten.
    static void set(const std::string& path, const ALUConfig& cfg) {
        db_[path] = cfg;
    }

    // Retrieve config stored at `path`.  Returns true and populates `out` on
    // success; returns false if no entry exists at that path.
    static bool get(const std::string& path, ALUConfig& out) {
        auto it = db_.find(path);
        if (it == db_.end()) return false;
        out = it->second;
        return true;
    }

    // Convenience: get or throw — mirrors the "fatal if not found" UVM pattern.
    static ALUConfig get_or_default(const std::string& path) {
        ALUConfig cfg;
        get(path, cfg);   // returns default-constructed cfg if not found
        return cfg;
    }

    // Clear all entries — useful between independent test runs.
    static void clear() { db_.clear(); }

private:
    static std::unordered_map<std::string, ALUConfig> db_;
};

// Out-of-line definition of the static member
inline std::unordered_map<std::string, ALUConfig> ALUConfigDB::db_;
