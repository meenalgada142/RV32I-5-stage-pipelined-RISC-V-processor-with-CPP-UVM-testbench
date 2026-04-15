#pragma once
// pipe_scoreboard.h — Queue-based commit comparator with windowed lookahead
//
// Usage:
//   1. load_expected(commits)    — load reference commit list before DUT run.
//   2. actual_reg(rd, data)      — called by monitor on every WB reg-write.
//   3. actual_mem(word_addr, data) — called by monitor on every MEM store.
//   4. all_matched()             — returns true once all expected commits consumed.
//   5. error_count() / summary() — query results.
//
// Mismatch handling — windowed lookahead (LOOKAHEAD = 4):
//   On a mismatch at the head of the expected queue, the scoreboard scans
//   the next LOOKAHEAD entries:
//     • If actual matches entry at offset +k  → the DUT dropped k commits.
//       Those k entries are flagged as DROPPED errors and removed; the match
//       is recorded. This prevents cascade false-positives.
//     • If actual matches nothing in the window → genuine wrong value.
//       The front entry is flagged as WRONG and consumed to allow continuation.
//
//   This strategy means a single flush bug produces one clear error message
//   rather than N cascading mismatches.

#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "pipe_transaction.h"

class PipeScoreboard {
public:
    static constexpr int LOOKAHEAD = 4;

    // -----------------------------------------------------------------------
    void load_expected(const std::vector<CommitRecord>& expected) {
        expected_.clear();
        errors_ = 0;
        matched_ = 0;
        dropped_ = 0;
        extra_   = 0;
        for (const auto& c : expected)
            expected_.push_back(c);
        total_expected_ = (int)expected.size();
        log_.clear();
    }

    // -----------------------------------------------------------------------
    void actual_reg(uint8_t rd, uint32_t data) {
        CommitRecord actual;
        actual.kind = CommitRecord::Kind::REG;
        actual.rd   = rd;
        actual.data = data;
        compare(actual);
    }

    void actual_mem(uint32_t word_addr, uint32_t data) {
        CommitRecord actual;
        actual.kind      = CommitRecord::Kind::MEM;
        actual.word_addr = word_addr;
        actual.data      = data;
        compare(actual);
    }

    // -----------------------------------------------------------------------
    bool all_matched()   const { return expected_.empty(); }
    int  error_count()   const { return errors_; }
    int  matched_count() const { return matched_; }
    int  dropped_count() const { return dropped_; }
    int  extra_count()   const { return extra_; }
    int  total_expected()const { return total_expected_; }
    int  remaining()     const { return (int)expected_.size(); }

    std::string summary() const {
        std::ostringstream ss;
        ss << matched_ << "/" << total_expected_ << " matched";
        if (errors_ > 0)   ss << ", " << errors_ << " error(s)";
        if (dropped_ > 0)  ss << " [" << dropped_ << " dropped]";
        if (extra_ > 0)    ss << " [" << extra_   << " extra]";
        return ss.str();
    }

    const std::vector<std::string>& mismatch_log() const { return log_; }

private:
    std::deque<CommitRecord>  expected_;
    int  errors_         = 0;
    int  matched_        = 0;
    int  dropped_        = 0;
    int  extra_          = 0;
    int  total_expected_ = 0;
    std::vector<std::string> log_;

    // -----------------------------------------------------------------------
    void compare(const CommitRecord& actual) {
        // Case 1: no more expected commits — DUT emitted an extra one
        if (expected_.empty()) {
            ++errors_;
            ++extra_;
            std::ostringstream ss;
            ss << "[SB] EXTRA commit (queue empty): " << actual.to_string();
            log_.push_back(ss.str());
            std::cerr << log_.back() << "\n";
            return;
        }

        // Case 2: exact match at the front of the queue
        if (actual == expected_.front()) {
            expected_.pop_front();
            ++matched_;
            return;
        }

        // Case 3: mismatch — scan the lookahead window
        // If actual matches entry at offset k, the DUT dropped k entries.
        int match_offset = -1;
        int scan_limit   = std::min(LOOKAHEAD, (int)expected_.size());
        for (int k = 1; k < scan_limit; ++k) {
            if (actual == expected_[k]) { match_offset = k; break; }
        }

        if (match_offset > 0) {
            // DUT dropped 'match_offset' expected commits
            for (int k = 0; k < match_offset; ++k) {
                ++errors_;
                ++dropped_;
                std::ostringstream ss;
                ss << "[SB] DROPPED commit #" << (matched_ + errors_)
                   << ": " << expected_.front().to_string();
                log_.push_back(ss.str());
                std::cerr << log_.back() << "\n";
                expected_.pop_front();
            }
            // Now the actual matches the new front — record as matched
            expected_.pop_front();
            ++matched_;
        } else {
            // Genuine wrong value: expected front, got something else
            ++errors_;
            std::ostringstream ss;
            ss << "[SB] WRONG VALUE #" << (matched_ + errors_)
               << "  expected=" << expected_.front().to_string()
               << "  actual="   << actual.to_string();
            log_.push_back(ss.str());
            std::cerr << log_.back() << "\n";
            expected_.pop_front();  // consume to allow continuation
        }
    }
};
