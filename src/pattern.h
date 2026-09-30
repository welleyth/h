#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "read.h"

namespace eo {
namespace detail {

class reader;

enum class pattern_problem {
    none,
    unclosed_class,
    empty_class,
    backwards_range,
    unclosed_group,
    lone_parenthesis,
    lone_closer,
    nothing_to_repeat,
    second_repeat,
    bad_count,
    huge_count,
    backwards_count,
    dangling_backslash,
    letter_escape,
    anchor,
    too_deep,
};

inline char const* describe(pattern_problem problem) {
    static char const* const words[] = {
        "",
        "the [ that opens here has no ] to close it",
        "a class needs at least one character; write \\] for a ] inside it",
        "this range runs backwards; write its low end first",
        "the ( that opens here has no ) to close it",
        "this ) closes no group; put a backslash before it for the character",
        "closes nothing; put a backslash before it for the character",
        "this repeat has nothing before it to repeat; put a backslash before it for the character",
        "a repeat follows a repeat; group the first, as in (a{2}){3}",
        "a count is {n}, {n,m} or {n,}, with n and m written in digits",
        "a count is at most 1000000000",
        "this count's least is above its most",
        "the pattern ends in a backslash with nothing after it to quote",
        "a backslash quotes a symbol, and before a letter or a digit it means nothing here; write the character alone, or a class such as [0-9]",
        "a pattern matches the whole token, so it needs no ^ or $; put a backslash before it for the character",
        "groups nest more than 50 deep",
    };
    return words[static_cast<int>(problem)];
}

struct syntax_fault {
    std::size_t column = 0;
    pattern_problem problem = pattern_problem::none;
};

using byte_set = std::array<std::uint64_t, 4>;

constexpr void add_byte(byte_set& set, unsigned byte) { set[byte >> 6] |= std::uint64_t{1} << (byte & 63); }

constexpr bool has_byte(byte_set const& set, unsigned byte) { return (set[byte >> 6] >> (byte & 63) & 1) != 0; }

inline long long constexpr most_in_a_count = 1000000000;
inline long long constexpr unbounded = -1;
inline int constexpr deepest_group = 50;

constexpr bool is_letter_or_digit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

struct no_pattern_tree {
    constexpr int one(byte_set const&) { return 0; }
    constexpr int row() { return 0; }
    constexpr void extend(int, int) {}
    constexpr int either(int) { return 0; }
    constexpr void offer(int, int) {}
    constexpr int again(int, long long, long long) { return 0; }
};

template <class Tree>
class pattern_parser {
public:
    constexpr pattern_parser(std::string_view text, Tree& tree) : text_(text), tree_(tree) {}

    constexpr int whole() {
        int const root = alternatives(0);
        if (!failed() && at_ < text_.size()) fail(at_, pattern_problem::lone_parenthesis);
        return root;
    }

    constexpr syntax_fault fault() const { return fault_; }

private:
    constexpr bool failed() const { return fault_.problem != pattern_problem::none; }

    constexpr void fail(std::size_t at, pattern_problem problem) {
        if (!failed()) fault_ = syntax_fault{at + 1, problem};
    }

    constexpr bool ahead(char wanted) const { return at_ < text_.size() && text_[at_] == wanted; }

    constexpr int alternatives(int depth) {
        int const first = sequence(depth);
        if (failed() || !ahead('|')) return first;
        int const choice = tree_.either(first);
        while (!failed() && ahead('|')) {
            at_++;
            tree_.offer(choice, sequence(depth));
        }
        return choice;
    }

    constexpr int sequence(int depth) {
        int const row = tree_.row();
        while (!failed() && at_ < text_.size() && text_[at_] != '|' && text_[at_] != ')')
            tree_.extend(row, item(depth));
        return row;
    }

    constexpr int item(int depth) {
        int const repeated = atom(depth);
        if (failed() || !repeat_ahead()) return repeated;
        long long least = 0;
        long long most = 0;
        counts(least, most);
        if (!failed() && repeat_ahead()) fail(at_, pattern_problem::second_repeat);
        return tree_.again(repeated, least, most);
    }

    constexpr bool repeat_ahead() const { return ahead('?') || ahead('*') || ahead('+') || ahead('{'); }

    constexpr void counts(long long& least, long long& most) {
        char const symbol = text_[at_];
        std::size_t const opened = at_++;
        if (symbol != '{') {
            least = symbol == '+' ? 1 : 0;
            most = symbol == '?' ? 1 : unbounded;
            return;
        }
        if (!number(least, opened)) return;
        most = least;
        if (ahead(',')) {
            at_++;
            most = unbounded;
            if (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9' && !number(most, opened)) return;
        }
        if (!ahead('}')) return fail(opened, pattern_problem::bad_count);
        at_++;
        if (most != unbounded && least > most) fail(opened, pattern_problem::backwards_count);
    }

    constexpr bool number(long long& value, std::size_t opened) {
        std::size_t const first = at_;
        value = 0;
        while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') {
            value = value * 10 + (text_[at_++] - '0');
            if (value > most_in_a_count) {
                fail(opened, pattern_problem::huge_count);
                return false;
            }
        }
        if (at_ == first) fail(opened, pattern_problem::bad_count);
        return at_ != first;
    }

    constexpr int atom(int depth) {
        char const here = text_[at_];
        if (here == '(') return group(depth);
        if (here == '[') return klass();
        if (here == ']' || here == '}') {
            fail(at_, pattern_problem::lone_closer);
            return 0;
        }
        if (here == '?' || here == '*' || here == '+' || here == '{') {
            fail(at_, pattern_problem::nothing_to_repeat);
            return 0;
        }
        if (here == '^' || here == '$') {
            fail(at_, pattern_problem::anchor);
            return 0;
        }
        byte_set one{};
        add_byte(one, static_cast<unsigned char>(quoted()));
        return tree_.one(one);
    }

    constexpr char quoted() {
        std::size_t const at = at_++;
        if (text_[at] != '\\') return text_[at];
        if (at_ == text_.size()) {
            fail(at, pattern_problem::dangling_backslash);
            return '\\';
        }
        if (is_letter_or_digit(text_[at_])) fail(at, pattern_problem::letter_escape);
        return text_[at_++];
    }

    constexpr int group(int depth) {
        std::size_t const opened = at_++;
        if (depth == deepest_group) {
            fail(opened, pattern_problem::too_deep);
            return 0;
        }
        int const inside = alternatives(depth + 1);
        if (!failed() && !ahead(')')) fail(opened, pattern_problem::unclosed_group);
        at_++;
        return inside;
    }

    constexpr int klass() {
        std::size_t const opened = at_++;
        bool const negated = ahead('^');
        if (negated) at_++;
        byte_set chosen{};
        bool any = false;
        while (!failed() && at_ < text_.size() && text_[at_] != ']') {
            std::size_t const start = at_;
            unsigned const low = static_cast<unsigned char>(quoted());
            unsigned high = low;
            if (!failed() && ahead('-') && at_ + 1 < text_.size() && text_[at_ + 1] != ']') {
                at_++;
                high = static_cast<unsigned char>(quoted());
                if (!failed() && high < low) fail(start, pattern_problem::backwards_range);
            }
            for (unsigned byte = low; byte <= high; byte++) add_byte(chosen, byte);
            any = true;
        }
        if (failed()) return 0;
        if (at_ == text_.size()) {
            fail(opened, pattern_problem::unclosed_class);
            return 0;
        }
        at_++;
        if (!any) fail(opened, pattern_problem::empty_class);
        if (negated)
            for (std::uint64_t& word : chosen) word = ~word;
        return tree_.one(chosen);
    }

    std::string_view text_;
    Tree& tree_;
    std::size_t at_ = 0;
    syntax_fault fault_;
};

constexpr syntax_fault first_fault(std::string_view text) {
    no_pattern_tree nothing;
    pattern_parser<no_pattern_tree> parser(text, nothing);
    parser.whole();
    return parser.fault();
}

#if defined(EOLYMP_CHECK_PATTERNS) && defined(__cpp_consteval)

inline void a_pattern_that_does_not_parse() {}

class pattern_text {
public:
    template <std::size_t Size>
    consteval pattern_text(char const (&text)[Size], char const* file = __builtin_FILE(),
                           int line = __builtin_LINE())
        : text_(text, Size - 1), where_{file, line} {
        if (first_fault(text_).problem != pattern_problem::none) a_pattern_that_does_not_parse();
    }

    template <std::size_t Size>
    pattern_text(char (&text)[Size], char const* file = __builtin_FILE(), int line = __builtin_LINE())
        : text_(text), where_{file, line} {}

    template <class T, class = std::enable_if_t<std::is_convertible_v<T const&, std::string_view> &&
                                                !std::is_array_v<T>>>
    pattern_text(T const& text, char const* file = __builtin_FILE(), int line = __builtin_LINE())
        : text_(text), where_{file, line} {}

    std::string_view text() const { return text_; }
    site where() const { return where_; }

private:
    std::string_view text_;
    site where_;
};

#else

class pattern_text {
public:
    template <class T, class = std::enable_if_t<std::is_convertible_v<T const&, std::string_view>>>
    pattern_text(T const& text, char const* file = __builtin_FILE(), int line = __builtin_LINE())
        : text_(text), where_{file, line} {}

    std::string_view text() const { return text_; }
    site where() const { return where_; }

private:
    std::string_view text_;
    site where_;
};

#endif

enum class piece_shape { one, row, either, again };

struct pattern_piece {
    piece_shape shape = piece_shape::one;
    byte_set set{};
    std::vector<int> parts;
    long long least = 1;
    long long most = 1;
};

inline pattern_piece piece_of(piece_shape shape) {
    pattern_piece made;
    made.shape = shape;
    return made;
}

class pattern_tree {
public:
    int one(byte_set const& set) {
        pattern_piece made = piece_of(piece_shape::one);
        made.set = set;
        return add(std::move(made));
    }

    int row() { return add(piece_of(piece_shape::row)); }
    void extend(int row, int part) { pieces_[static_cast<std::size_t>(row)].parts.push_back(part); }

    int either(int first) {
        pattern_piece made = piece_of(piece_shape::either);
        made.parts.push_back(first);
        return add(std::move(made));
    }

    void offer(int choice, int part) { extend(choice, part); }

    int again(int part, long long least, long long most) {
        pattern_piece made = piece_of(piece_shape::again);
        made.parts.push_back(part);
        made.least = least;
        made.most = most;
        return add(std::move(made));
    }

    pattern_piece const& at(int index) const { return pieces_[static_cast<std::size_t>(index)]; }

private:
    int add(pattern_piece made) {
        pieces_.push_back(std::move(made));
        return static_cast<int>(pieces_.size()) - 1;
    }

    std::vector<pattern_piece> pieces_;
};

enum class step_code { one, fork, jump, counted, done };

struct pattern_step {
    step_code code = step_code::done;
    int next = 0;
    int other = 0;
    byte_set set{};
    long long least = 0;
    long long most = 0;
    int queue = -1;
};

inline pattern_step step_to(step_code code, int next = 0) {
    pattern_step made;
    made.code = code;
    made.next = next;
    return made;
}


inline long long constexpr most_steps = 4096;

class pattern_program {
public:
    pattern_program() = default;

    pattern_program(pattern_tree const& tree, int root) {
        emit(tree, root);
        steps_.push_back(step_to(step_code::done));
        marks_.assign(steps_.size(), 0);
    }

    static long long cost(pattern_tree const& tree, int index) {
        pattern_piece const& piece = tree.at(index);
        if (piece.shape == piece_shape::one) return 1;
        if (piece.shape == piece_shape::again) {
            if (tree.at(piece.parts[0]).shape == piece_shape::one) return 1;
            long long const each = cost(tree, piece.parts[0]);
            if (each == 0) return 0;
            long long const optional = piece.most == unbounded ? each + 2 : (piece.most - piece.least) * (each + 1);
            return std::min(piece.least * each + optional, most_steps + 1);
        }
        long long total = piece.shape == piece_shape::either ? 2 * static_cast<long long>(piece.parts.size() - 1) : 0;
        for (int const part : piece.parts) total = std::min(total + cost(tree, part), most_steps + 1);
        return total;
    }

    bool matches(std::string_view text) const {
        for (std::size_t at = 0; at < queues_.size(); at++) {
            queues_[at].clear();
            heads_[at] = 0;
        }
        current_.clear();
        stamp_++;
        reach(current_, 0, 0);
        for (std::size_t at = 0; at < text.size() && !current_.empty(); at++) {
            unsigned const byte = static_cast<unsigned char>(text[at]);
            long long const after = static_cast<long long>(at) + 1;
            for (int const index : current_) {
                pattern_step const& step = steps_[static_cast<std::size_t>(index)];
                if (step.code == step_code::counted) advance(step, byte, after);
            }
            upcoming_.clear();
            stamp_++;
            visits_ += current_.size();
            for (int const index : current_) {
                pattern_step const& step = steps_[static_cast<std::size_t>(index)];
                if (step.code == step_code::one && has_byte(step.set, byte)) reach(upcoming_, step.next, after);
                if (step.code == step_code::counted) stay(upcoming_, index, after);
            }
            current_.swap(upcoming_);
        }
        return !current_.empty() && marks_.back() == stamp_;
    }

    std::uint64_t visits() const { return visits_; }

    std::size_t entries_held() const {
        std::size_t held = 0;
        for (std::vector<long long> const& entries : queues_) held += entries.size();
        return held;
    }

private:
    int here() const { return static_cast<int>(steps_.size()); }

    int push(pattern_step made) {
        steps_.push_back(made);
        return here() - 1;
    }

    void emit(pattern_tree const& tree, int index) {
        pattern_piece const& piece = tree.at(index);
        if (piece.shape == piece_shape::one) {
            pattern_step made = step_to(step_code::one, here() + 1);
            made.set = piece.set;
            push(made);
        } else if (piece.shape == piece_shape::row) {
            for (int const part : piece.parts) emit(tree, part);
        } else if (piece.shape == piece_shape::either) {
            std::vector<int> jumps;
            for (std::size_t at = 0; at + 1 < piece.parts.size(); at++) {
                int const fork = push(step_to(step_code::fork, here() + 1));
                emit(tree, piece.parts[at]);
                jumps.push_back(push(step_to(step_code::jump)));
                steps_[static_cast<std::size_t>(fork)].other = here();
            }
            emit(tree, piece.parts.back());
            for (int const jump : jumps) steps_[static_cast<std::size_t>(jump)].next = here();
        } else {
            repeat(tree, piece);
        }
    }

    void repeat(pattern_tree const& tree, pattern_piece const& piece) {
        int const part = piece.parts[0];
        if (cost(tree, part) == 0) return;
        if (tree.at(part).shape == piece_shape::one) {
            pattern_step made = step_to(step_code::counted, here() + 1);
            made.set = tree.at(part).set;
            made.least = piece.least;
            made.most = piece.most;
            made.queue = static_cast<int>(queues_.size());
            queues_.emplace_back();
            heads_.push_back(0);
            push(made);
            return;
        }
        for (long long copy = 0; copy < piece.least; copy++) emit(tree, part);
        if (piece.most == unbounded) {
            int const loop = push(step_to(step_code::fork, here() + 1));
            emit(tree, part);
            push(step_to(step_code::jump, loop));
            steps_[static_cast<std::size_t>(loop)].other = here();
            return;
        }
        std::vector<int> forks;
        for (long long copy = piece.least; copy < piece.most; copy++) {
            forks.push_back(push(step_to(step_code::fork, here() + 1)));
            emit(tree, part);
        }
        for (int const fork : forks) steps_[static_cast<std::size_t>(fork)].other = here();
    }

    void advance(pattern_step const& step, unsigned byte, long long after) const {
        std::vector<long long>& entries = queues_[static_cast<std::size_t>(step.queue)];
        std::size_t& head = heads_[static_cast<std::size_t>(step.queue)];
        if (!has_byte(step.set, byte)) {
            entries.clear();
            head = 0;
            return;
        }
        while (head < entries.size() && step.most != unbounded && after - entries[head] > step.most) head++;
        if (head == entries.size()) {
            entries.clear();
            head = 0;
        }
    }

    void stay(std::vector<int>& into, int index, long long at) const {
        pattern_step const& step = steps_[static_cast<std::size_t>(index)];
        std::vector<long long> const& entries = queues_[static_cast<std::size_t>(step.queue)];
        std::size_t const head = heads_[static_cast<std::size_t>(step.queue)];
        if (head == entries.size()) return;
        if (marks_[static_cast<std::size_t>(index)] != stamp_) {
            marks_[static_cast<std::size_t>(index)] = stamp_;
            into.push_back(index);
        }
        if (at - entries[head] >= step.least) reach(into, step.next, at);
    }

    void enter(pattern_step const& step, long long at) const {
        std::vector<long long>& entries = queues_[static_cast<std::size_t>(step.queue)];
        std::size_t& head = heads_[static_cast<std::size_t>(step.queue)];
        if (head > 64 && head * 2 > entries.size()) {
            entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(head));
            head = 0;
        }
        if (entries.size() == head || (step.most != unbounded && entries.back() != at)) entries.push_back(at);
    }

    void reach(std::vector<int>& into, int start, long long at) const {
        pending_.clear();
        pending_.push_back(start);
        while (!pending_.empty()) {
            int const index = pending_.back();
            pending_.pop_back();
            visits_++;
            pattern_step const& step = steps_[static_cast<std::size_t>(index)];
            if (step.code == step_code::counted) enter(step, at);
            if (marks_[static_cast<std::size_t>(index)] == stamp_) continue;
            marks_[static_cast<std::size_t>(index)] = stamp_;
            if (step.code == step_code::fork) {
                pending_.push_back(step.other);
                pending_.push_back(step.next);
            } else if (step.code == step_code::jump) {
                pending_.push_back(step.next);
            } else {
                into.push_back(index);
                if (step.code == step_code::counted && step.least == 0) pending_.push_back(step.next);
            }
        }
    }

    std::vector<pattern_step> steps_;
    mutable std::vector<std::vector<long long>> queues_;
    mutable std::vector<std::size_t> heads_;
    mutable std::vector<std::uint64_t> marks_;
    mutable std::uint64_t stamp_ = 0;
    mutable std::vector<int> current_;
    mutable std::vector<int> upcoming_;
    mutable std::vector<int> pending_;
    mutable std::uint64_t visits_ = 0;
};

inline bool matches_without_a_blank(pattern_tree const& tree, int index) {
    pattern_piece const& piece = tree.at(index);
    if (piece.shape == piece_shape::one) {
        byte_set blanks{};
        for (unsigned const blank : {unsigned{' '}, unsigned{'\t'}, unsigned{'\n'}, unsigned{'\r'}}) add_byte(blanks, blank);
        for (std::size_t at = 0; at < piece.set.size(); at++)
            if ((piece.set[at] & ~blanks[at]) != 0) return true;
        return false;
    }
    if (piece.shape == piece_shape::again) return piece.least == 0 || matches_without_a_blank(tree, piece.parts[0]);
    bool const either = piece.shape == piece_shape::either;
    for (int const part : piece.parts)
        if (matches_without_a_blank(tree, part) == either) return either;
    return !either;
}

inline long long longest_match(pattern_tree const& tree, int index) {
    pattern_piece const& piece = tree.at(index);
    if (piece.shape == piece_shape::one) return 1;
    if (piece.shape == piece_shape::again) {
        long long const each = longest_match(tree, piece.parts[0]);
        if (each == 0 || piece.most == 0) return 0;
        if (each == unbounded || piece.most == unbounded) return unbounded;
        return each * piece.most;
    }
    long long total = 0;
    for (int const part : piece.parts) {
        long long const each = longest_match(tree, part);
        if (each == unbounded) return unbounded;
        total = piece.shape == piece_shape::row ? total + each : std::max(total, each);
    }
    return total;
}

}  // namespace detail

class pattern {
public:
    explicit pattern(detail::pattern_text told) : text_(told.text()) {
        detail::syntax_fault const fault = detail::first_fault(text_);
        if (fault.problem != detail::pattern_problem::none) {
            std::string said = detail::describe(fault.problem);
            if (fault.problem == detail::pattern_problem::lone_closer)
                said = fmt("this {} {}", text_[fault.column - 1], said);
            detail::library_error(fmt("{}: eo::pattern(\"{}\") does not parse at column {}: {}",
                                      detail::where_of(told.where()), detail::escaped(text_), fault.column, said));
        }
        detail::pattern_parser<detail::pattern_tree> parser(text_, tree_);
        root_ = parser.whole();
        if (detail::pattern_program::cost(tree_, root_) + 1 > detail::most_steps)
            detail::library_error(fmt("{}: eo::pattern(\"{}\") needs more than {} steps to match; repeat a "
                                      "character or a class, which costs one step at any count, rather than a "
                                      "group",
                                      detail::where_of(told.where()), detail::escaped(text_), detail::most_steps));
        program_ = detail::pattern_program(tree_, root_);
        longest_ = detail::longest_match(tree_, root_);
        in_a_token_ = detail::matches_without_a_blank(tree_, root_);
    }

    bool matches(std::string_view token) const { return program_.matches(token); }
    std::string const& text() const { return text_; }

private:
    friend class detail::reader;

    std::string text_;
    detail::pattern_tree tree_;
    int root_ = 0;
    detail::pattern_program program_;
    long long longest_ = 0;
    bool in_a_token_ = true;
};

}  // namespace eo
