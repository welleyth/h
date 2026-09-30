#pragma once

#include <array>
#include <limits>
#include <string>
#include <utility>

#include "core.h"
#include "diag.h"
#include "fmt.h"

namespace eo {

class charset {
public:
    explicit charset(char const* spec) : spec_(spec) { build(); }
    explicit charset(std::string spec) : spec_(std::move(spec)) { build(); }

    bool has(char c) const { return allowed_[static_cast<unsigned char>(c)]; }
    std::string const& text() const { return spec_; }

private:
    void build() {
        allowed_.fill(false);
        for (std::size_t at = 0; at < spec_.size(); at++) {
            if (at + 2 < spec_.size() && spec_[at + 1] == '-') {
                unsigned const from = static_cast<unsigned char>(spec_[at]);
                unsigned const to = static_cast<unsigned char>(spec_[at + 2]);
                if (from > to)
                    detail::library_error(fmt("the character range \"{}\" in charset(\"{}\") runs backwards",
                                              spec_.substr(at, 3), spec_));
                for (unsigned c = from; c <= to; c++) allowed_[c] = true;
                at += 2;
            } else {
                allowed_[static_cast<unsigned char>(spec_[at])] = true;
            }
        }
    }

    std::array<bool, 256> allowed_{};
    std::string spec_;
};

namespace detail {

enum class stated { yes, deliberate, absent };

class value_name {
public:
    value_name(char const* text) : text_(text), state_(stated::yes) {}
    value_name(std::string text) : text_(std::move(text)), state_(stated::yes) {}
    value_name(unnamed_t) : state_(stated::deliberate) {}

    static value_name nothing() {
        value_name made{unnamed};
        made.state_ = stated::absent;
        return made;
    }

    bool known() const { return state_ == stated::yes; }
    bool absent() const { return state_ == stated::absent; }
    std::string text() const { return indexed_ ? fmt("{}[{}]", key(), index_) : key(); }
    std::string const& key() const { return lent_ != nullptr ? *lent_ : text_; }
    bool indexed() const { return indexed_; }
    long long index() const { return index_; }

    value_name field(char const* suffix) const {
        if (!known()) return *this;
        return value_name(text() + suffix);
    }

    value_name at(long long index) const {
        if (!known()) return *this;
        value_name made(indexed_ ? text() : key());
        made.indexed_ = true;
        made.index_ = index;
        return made;
    }

    value_name lent_at(long long index) const {
        if (!known() || indexed_) return at(index);
        value_name made{unnamed};
        made.state_ = stated::yes;
        made.lent_ = &key();
        made.indexed_ = true;
        made.index_ = index;
        return made;
    }

private:
    std::string const* lent_ = nullptr;
    std::string text_;
    stated state_;
    bool indexed_ = false;
    long long index_ = 0;
};

inline bool is_round(long long value) {
    if (value < 1000) return false;
    while (value % 10 == 0) value /= 10;
    return value == 1 || value == 2 || value == 5;
}

inline bool nearly_round(long long value) {
    if (value == std::numeric_limits<long long>::min() || value == std::numeric_limits<long long>::max())
        return false;
    return !is_round(value) && (is_round(value - 1) || is_round(value + 1));
}

inline unsigned char byte_at(std::string const& text, std::size_t at) { return static_cast<unsigned char>(text[at]); }

inline std::size_t utf8_length(std::string const& text, std::size_t at) {
    unsigned char const lead = byte_at(text, at);
    std::size_t length = 4;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead < 0xC2 || lead > 0xF4) return 0;
    if (lead < 0xE0) length = 2;
    else if (lead < 0xF0) length = 3;
    if (lead == 0xE0) low = 0xA0;
    if (lead == 0xED) high = 0x9F;
    if (lead == 0xF0) low = 0x90;
    if (lead == 0xF4) high = 0x8F;
    if (text.size() - at < length) return 0;
    for (std::size_t next = 1; next < length; next++) {
        unsigned char const byte = byte_at(text, at + next);
        if (byte < low || byte > high) return 0;
        low = 0x80;
        high = 0xBF;
    }
    return length;
}

inline std::string escaped(std::string const& text) {
    std::string out;
    std::size_t at = 0;
    while (at < text.size()) {
        unsigned char const byte = byte_at(text, at);
        std::size_t length = 1;
        if (byte >= 0x80) length = utf8_length(text, at);
        else if (byte < 0x20 || byte == 0x7F) length = 0;
        if (length > 0) {
            out.append(text, at, length);
            at += length;
            continue;
        }
        out += "\\x";
        out += "0123456789abcdef"[byte >> 4];
        out += "0123456789abcdef"[byte & 15];
        at++;
    }
    return out;
}

inline std::string shorten(std::string const& text, std::size_t limit = 40) {
    if (text.size() <= limit) return escaped(text);
    std::size_t lead = limit;
    while (lead > 0 && limit - lead < 3 && (byte_at(text, lead) & 0xC0) == 0x80) lead--;
    std::size_t const cut = lead + utf8_length(text, lead) > limit ? lead : limit;
    return escaped(text.substr(0, cut)) + "...";
}

inline char const* name_of(int character) {
    if (character < 0) return "the end of the input";
    if (character == ' ') return "a space";
    if (character == '\n') return "a line break";
    if (character == '\t') return "a tab";
    if (character == '\r') return "a carriage return";
    return "";
}

inline bool same_folded(std::string const& left, char const* right) {
    std::size_t at = 0;
    for (; at < left.size() && right[at] != '\0'; at++) {
        char const one = left[at] >= 'A' && left[at] <= 'Z' ? static_cast<char>(left[at] + 32) : left[at];
        char const other = right[at] >= 'A' && right[at] <= 'Z' ? static_cast<char>(right[at] + 32) : right[at];
        if (one != other) return false;
    }
    return at == left.size() && right[at] == '\0';
}

inline char folded(char one) { return one >= 'A' && one <= 'Z' ? static_cast<char>(one + 32) : one; }

inline bool same_in_any_case(std::string const& left, std::string const& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t at = 0; at < left.size(); at++)
        if (folded(left[at]) != folded(right[at])) return false;
    return true;
}

inline bool is_blank(int character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

}  // namespace detail

inline detail::value_name element(std::string name, long long index) {
    return detail::value_name(std::move(name)).at(index);
}

}  // namespace eo
