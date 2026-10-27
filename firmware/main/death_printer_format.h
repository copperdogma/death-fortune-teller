#pragma once

#include <stddef.h>
#include <stdint.h>

namespace skull::printer::detail {

// Allocation-free formatter shared by firmware and host tests. It produces
// print-buffer lines; the transport appends LF and controls pacing.
class Formatter {
public:
    explicit Formatter(const char *text) : text_(text ? text : ""), pos_(0), phase_(0), trailing_empty_(false) {}

    bool next(char *line, size_t capacity, size_t &length) {
        if (!line || capacity == 0) return false;
        length = 0;
        if (phase_ == 0) { phase_ = 1; return copy_literal("Your fortune:", line, capacity, length); }
        if (phase_ == 1) { phase_ = 2; return copy_literal("", line, capacity, length); }
        if (phase_ == 2 && (text_[pos_] != '\0' || trailing_empty_)) return body_line(line, capacity, length);
        if (phase_ < 3) phase_ = 3;
        if (phase_ == 3) { phase_ = 4; return copy_literal("", line, capacity, length); }
        if (phase_ == 4) { phase_ = 5; return copy_literal("-- Death, the Fortune-Teller", line, capacity, length); }
        // Two existing blank lines plus three extra lines for tear-off clearance.
        if (phase_ >= 5 && phase_ < 10) { ++phase_; return copy_literal("", line, capacity, length); }
        return false;
    }

private:
    static bool copy_literal(const char *value, char *line, size_t capacity, size_t &length) {
        while (value[length] && length + 1 < capacity) { line[length] = value[length]; ++length; }
        line[length] = '\0';
        return value[length] == '\0';
    }

    bool body_line(char *line, size_t capacity, size_t &length) {
        if (capacity < 33) return false;
        if (trailing_empty_) { trailing_empty_ = false; line[0] = '\0'; return true; }
        size_t last_space = static_cast<size_t>(-1);
        bool explicit_newline = false;
        while (text_[pos_] && length < 32) {
            const unsigned char ch = static_cast<unsigned char>(text_[pos_]);
            if (ch == '\r') { ++pos_; continue; }
            if (ch == '\n') {
                ++pos_; explicit_newline = true;
                if (text_[pos_] == '\0') trailing_empty_ = true;
                break;
            }
            line[length] = (ch >= 32 && ch <= 126) ? static_cast<char>(ch) : '?';
            if (line[length] == ' ') last_space = length;
            ++length; ++pos_;
        }
        // Only word-wrap a line that ended because the column limit was reached.
        const bool at_newline = text_[pos_] == '\n' ||
            (text_[pos_] == '\r' && text_[pos_ + 1] == '\n');
        if (!explicit_newline && length == 32 && text_[pos_] && !at_newline &&
            last_space != static_cast<size_t>(-1)) {
            pos_ -= (length - last_space - 1);
            length = last_space;
        }
        while (length && line[length - 1] == ' ') --length;
        size_t first = 0;
        while (first < length && line[first] == ' ') ++first;
        if (first) {
            for (size_t i = first; i < length; ++i) line[i - first] = line[i];
            length -= first;
        }
        line[length] = '\0';
        // Strip spaces carried across a wrap; do not discard explicit newlines.
        if (!explicit_newline) while (text_[pos_] == ' ') ++pos_;
        return true;
    }

    const char *text_;
    size_t pos_;
    uint8_t phase_;
    bool trailing_empty_;
};

} // namespace skull::printer::detail
