#pragma once
// decoder_config.h — Configuration object + static config database

#include <string>
#include <unordered_map>
#include <cstdint>

struct DecoderConfig {
    bool     is_active              = true;
    int      num_transactions       = 10000;
    int      coverage_threshold     = 10;
    int      max_driven_passes      = 5;
    int      verbosity              = 0;   // 0=quiet, 1=summary, 2=trace
    bool     enable_coverage_driven = true;
    uint64_t seed                   = 0;   // 0 = use random_device
};

class DecoderConfigDB {
public:
    static void set(const std::string& path, const DecoderConfig& cfg) {
        db_[path] = cfg;
    }
    static bool get(const std::string& path, DecoderConfig& out) {
        auto it = db_.find(path);
        if (it == db_.end()) return false;
        out = it->second;
        return true;
    }
    static DecoderConfig get_or_default(const std::string& path) {
        DecoderConfig cfg;
        get(path, cfg);
        return cfg;
    }
    static void clear() { db_.clear(); }

private:
    static std::unordered_map<std::string, DecoderConfig> db_;
};

inline std::unordered_map<std::string, DecoderConfig> DecoderConfigDB::db_;
