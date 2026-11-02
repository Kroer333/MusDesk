#pragma once

#include "musdesk/core/Composition.hpp"
#include <deque>
#include <optional>

namespace musdesk {
class CompositionHistory {
public:
    static constexpr std::size_t capacity = 100;
    void reset(const CompositionSettings& settings) {
        current_ = settings;
        before_.reset();
        undo_.clear();
        redo_.clear();
    }
    void observe(const CompositionSettings& settings, bool editing) {
        if (settings != current_) {
            if (!before_) before_ = current_;
            current_ = settings;
        }
        if (!editing) finish();
    }
    bool canUndo() const { return !undo_.empty() || (before_ && *before_ != current_); }
    bool canRedo() const { return !redo_.empty() && !(before_ && *before_ != current_); }
    std::optional<CompositionSettings> undo() {
        finish();
        if (undo_.empty()) return {};
        redo_.push_back(current_);
        current_ = undo_.back();
        undo_.pop_back();
        return current_;
    }
    std::optional<CompositionSettings> redo() {
        finish();
        if (redo_.empty()) return {};
        undo_.push_back(current_);
        current_ = redo_.back();
        redo_.pop_back();
        return current_;
    }
private:
    void finish() {
        if (before_ && *before_ != current_) {
            undo_.push_back(*before_);
            if (undo_.size() > capacity) undo_.pop_front();
            redo_.clear();
        }
        before_.reset();
    }
    CompositionSettings current_;
    std::optional<CompositionSettings> before_;
    std::deque<CompositionSettings> undo_, redo_;
};
}
