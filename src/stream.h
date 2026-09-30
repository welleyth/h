#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "parse.h"
#include "pattern.h"
#include "read.h"

namespace eo {
namespace detail {

enum class fault { invalid_test, jury_error, wrong_answer };

inline long long& current_case() {
    static long long number = 0;
    return number;
}

inline std::string case_prefix() {
    return current_case() > 0 ? fmt("case {}: ", current_case()) : std::string();
}

struct studied_bounds {
    bool quiet = false;
    long long low = 0;
    long long high = 0;
    long long type_low = 0;
    long long type_high = 0;
};

struct seen_bounds {
    std::string kind;
    std::string low;
    std::string high;
    bool reached_low;
    bool reached_high;
    site where;
    bool steady;
    long long whole_low = 0;
    long long whole_high = 0;
    long long last_whole = 0;
    bool read_whole = false;
    double exact_low = 0;
    double exact_high = 0;
    char const* spelled = nullptr;
};

enum class number_read { none, integer, real };

struct integer_span {
    long long value = 0;
    long long start = -1;
    long long end = -1;
};

inline char const* phrase_of(std::string const& kind) {
    if (kind == "real") return "a number";
    if (kind == "length") return "a length";
    return "an integer";
}

class reader {
public:
    reader() = default;

    reader(source from, fault whose, std::string label, bool lenient, char const* loose_code)
        : from_(std::move(from)), whose_(whose), label_(std::move(label)), lenient_(lenient),
          loose_code_(loose_code) {}

    [[noreturn]] void refuse(value_name const& name, std::string const& what) const {
        std::string message = verdict_word() + case_prefix();
        if (!label_.empty()) message += fmt("{}, ", label_);
        finish(whose_ == fault::wrong_answer ? 1 : 3, message + line_of(name) + ": " + what);
    }

    bool at_end() {
        settle();
        if (lenient_) skip_blanks(true);
        return from_.peek() < 0;
    }

    bool at_line_end() {
        settle();
        if (!lenient_) return from_.peek() == '\n';
        skip_blanks(false);
        int const here = from_.peek();
        return here < 0 || here == '\n';
    }

    long long line() const { return from_.line(); }
    long long position() const { return from_.position(); }
    integer_span last_integer() const { return last_integer_; }
    bool carriage_returns() const { return from_.carriage_returns(); }
    std::string last_value() const { return last_indexed_ ? fmt("{}[{}]", last_value_, last_index_) : last_value_; }
    void mark_separated() {
        separated_ = true;
        just_read_ = number_read::none;
    }
    std::map<std::string, seen_bounds> const& bounds() const { return bounds_; }
    bool read_anything() const { return read_anything_; }
    void exponents(bool allowed) { exponents_ = allowed; }

    void before_blocking(void (*hook)(void*), void* owner) {
        flush_ = hook;
        owner_ = owner;
    }

    void on_end(std::string text) { end_text_ = std::move(text); }

    long long longest_of(std::initializer_list<char const*> choices) {
        long long most = 64;
        for (char const* one : choices) {
            long long const here = static_cast<long long>(std::char_traits<char>::length(one)) + 1;
            if (here > most) most = here;
        }
        return most;
    }

    std::size_t room_for(long long count, value_name const& name) {
        if (count < 0) refuse(name, fmt("a count of {} cannot be read", count));
        long long const left = from_.bytes_left();
        if (left >= 0) return static_cast<std::size_t>(std::min(count, left / 2 + 1));
        return static_cast<std::size_t>(std::min(count, static_cast<long long>(mebibyte)));
    }

    void blame(fault whose) { whose_ = whose; }
    void relaxed(bool loose) { relaxed_ = loose; }
    bool relaxed() const { return relaxed_; }

    int peek() { return from_.peek(); }
    int take() { return from_.take(); }

    std::string ahead_of_the_value() {
        std::string const rest = from_.ahead(lookahead);
        std::size_t at = 0;
        while (at < rest.size() && !is_blank(static_cast<unsigned char>(rest[at]))) at++;
        return rest.substr(0, at);
    }

    absorbed absorb(std::size_t most) { return from_.absorb(most); }

    int listening_descriptor() const { return from_.listening_descriptor(); }

    bool content_waiting() {
        for (;;) {
            std::string const held = from_.ahead(from_.held());
            for (char const one : held)
                if (!is_blank(static_cast<unsigned char>(one))) return true;
            if (!from_.top_up()) return false;
        }
    }

    std::string rest_of_the_input() {
        std::string rest = from_.ahead(lookahead);
        std::size_t const stop = rest.find('\n');
        if (stop != std::string::npos) rest.resize(stop);
        return rest;
    }

    static long long constexpr int_low = -2147483647LL - 1;
    static long long constexpr int_high = 2147483647LL;
    static long long constexpr long_low = -9223372036854775807LL - 1;
    static long long constexpr long_high = 9223372036854775807LL;

    int whole_int(long long low, long long high, stated bounds, value_name const& name, site where) {
        return static_cast<int>(whole(low, high, bounds, name, where, int_low, int_high, "an int"));
    }

    long long whole_long(long long low, long long high, stated bounds, value_name const& name, site where) {
        return whole(low, high, bounds, name, where, long_low, long_high, "a long long");
    }

    long long whole(long long low, long long high, stated bounds, value_name const& name, site where,
                    long long type_low, long long type_high, char const* type_word) {
        start_value(name, where, "an integer");
        if (lenient_) settle();
        long long const began = from_.position();
        integer_read parsed;
        if (quick_integer(parsed.value)) was_read(name);
        else parsed = spelled_integer(name);
        study(name, low, high, bounds, type_low, type_high, type_word, where);
        if (bounds == stated::yes) {
            if (parsed.value < low) refuse(name, fmt("{} is below {}", parsed.value, low));
            if (parsed.value > high) refuse(name, fmt("{} is above {}", parsed.value, high));
        }
        if (parsed.value < type_low || parsed.value > type_high)
            refuse(name, fmt("{} does not fit {}", parsed.value, type_word));
        if (bounds == stated::yes) {
            remember(name, "int", low, high, parsed.value == low, parsed.value == high,
                     where);
            if (name.known()) {
                last_bounds_->last_whole = parsed.value;
                last_bounds_->read_whole = true;
            }
        }
        if (!lenient_) {
            just_read_ = number_read::integer;
            integer_just_read_ = parsed.value;
            last_integer_ = {parsed.value, began, from_.position()};
        }
        return parsed.value;
    }

    double fractional(double low, double high, stated bounds, int least_decimals, int most_decimals,
                      bool decimals_stated, value_name const& name, site where) {
        std::string token = take_number(name, where, true, "a number");
        real_read const parsed = parse_real(token, exponents_, lenient_);
        if (parsed.problem != number_problem::none) {
            if (token_goes_on()) refuse_the_whole_number(name, token, number_read::real);
            refuse(name, fmt("expected a number, found \"{}\": {}", shorten(token), describe(parsed.problem)));
        }
        if (name.absent() && fresh("EO101", where))
            warn("EO101", "this value is read without a name", "name it, or say eo::unnamed if it needs none",
                 where);
        if (!decimals_stated && !lenient_ && fresh("EO109", where))
            warn("EO109", "this number is read without a rule on its digits",
                 "say how many digits follow the point: read_real(low, high, least, most, name)", where);
        if (bounds == stated::absent && fresh(loose_code_, where))
            warn(loose_code_, "this value is read without bounds", "give the bounds, or say eo::any", where);
        if (bounds == stated::yes) {
            if (parsed.value < low) refuse(name, fmt("{} is below {}", parsed.value, low));
            if (parsed.value > high) refuse(name, fmt("{} is above {}", parsed.value, high));
        }
        if (decimals_stated && (parsed.decimals < least_decimals || parsed.decimals > most_decimals)) {
            if (token_goes_on()) refuse_the_whole_number(name, token, number_read::real);
            std::string const said = fmt("{} has {} digits after the point, not {}..{}", shorten(token),
                                         parsed.decimals, least_decimals, most_decimals);
            refuse(name, said);
        }
        if (bounds == stated::yes)
            remember(name, "real", low, high, parsed.value == low, parsed.value == high,
                     where);
        if (!lenient_) {
            just_read_ = number_read::real;
            real_just_read_.swap(token);
        }
        return parsed.value;
    }

    std::string word(long long least, long long most, charset const* allowed, stated bounds,
                     value_name const& name, site where) {
        std::string token;
        word_into(token, least, most, allowed, bounds, name, where);
        return token;
    }

    void word_into(std::string& token, long long least, long long most, charset const* allowed, stated bounds,
                   value_name const& name, site where) {
        long long const cap = bounds == stated::yes && most < long_high ? most + 1 : 0;
        take_word_into(token, name, where, "a token", cap);
        if (name.absent() && fresh("EO101", where))
            warn("EO101", "this value is read without a name", "name it, or say eo::unnamed if it needs none",
                 where);
        if (bounds == stated::absent) {
            if (fresh(lenient_ ? loose_code_ : "EO108", where))
                warn(lenient_ ? loose_code_ : "EO108", "this token is read with no length and no charset",
                     "give a length and the characters it may hold, or say eo::any", where);
        } else if (allowed == nullptr && bounds == stated::yes && !lenient_ && fresh("EO108", where))
            warn("EO108", "this token is read with no charset",
                 "say which characters it may hold, or say eo::any", where);
        note_a_large_token(token, where);
        if (bounds == stated::yes) {
            long long const length = static_cast<long long>(token.size());
            if (length > most)
                refuse(name, fmt("\"{}\" is longer than {} characters", shorten(token), most));
            if (length < least)
                refuse(name, fmt("\"{}\" is {} characters long, not {}..{}", shorten(token), length, least,
                                 most));
            if (allowed != nullptr)
                for (char const one : token)
                    if (!allowed->has(one))
                        refuse(name, fmt("\"{}\" holds \"{}\", which is not in \"{}\"", shorten(token),
                                         escaped(std::string(1, one)), allowed->text()));
            remember(name, "length", least, most, length == least, length == most,
                     where);
        }
    }

    std::string matching(eo::pattern const& told, value_name const& name, site where) {
        if (!told.in_a_token_ && fresh("EO113", where))
            warn("EO113",
                 fmt("\"{}\" matches only text with a blank in it, and a token holds none", escaped(told.text())),
                 "read the line with read_line; testlib drops a space its pattern does not quote, so its "
                 "\"[a-z] {1,5}\" is \"[a-z]{1,5}\" here",
                 where);
        std::string token;
        long long const cap = told.longest_ == unbounded ? 0 : told.longest_ + 1;
        take_word_into(token, name, where, "a token", cap);
        note_a_large_token(token, where);
        if (cap > 0 && static_cast<long long>(token.size()) == cap)
            refuse(name, fmt("a token that starts \"{}\" is longer than the {} characters \"{}\" allows",
                             shorten(token), told.longest_, escaped(told.text())));
        if (!told.matches(token))
            refuse(name, fmt("\"{}\" does not match \"{}\"", shorten(token), escaped(told.text())));
        return token;
    }

    std::string line_matching(eo::pattern const& told, value_name const& name) {
        std::string text;
        long long const cap = told.longest_ == unbounded ? 0 : told.longest_ + 1;
        long long const seen = line_into(text, cap, name);
        if (cap > 0 && seen >= cap)
            refuse(name, fmt("a line that starts \"{}\" is longer than the {} characters \"{}\" allows",
                             shorten(text), told.longest_, escaped(told.text())));
        if (!told.matches(text))
            refuse(name, fmt("the line \"{}\" does not match \"{}\"", shorten(text), escaped(told.text())));
        end_the_line(name);
        return text;
    }

    std::string rest_of_line(long long least, long long most, charset const* allowed, stated bounds,
                             value_name const& name, site where) {
        std::string text;
        long long const cap = bounds == stated::yes && most < long_high ? most + 1 : 0;
        long long const seen = line_into(text, cap, name);
        if (allowed == nullptr && bounds == stated::yes && !lenient_ && fresh("EO108", where))
            warn("EO108", "this line is read with no charset",
                 "say which characters it may hold, or say eo::any", where);
        long long const length = cap == 0 ? static_cast<long long>(text.size()) : seen;
        if (bounds == stated::yes && length > most)
            refuse(name, fmt("the line is longer than {} characters", most));
        if (bounds == stated::yes && length < least)
            refuse(name, fmt("the line is {} characters long, not {}..{}", length, least, most));
        if (allowed != nullptr)
            for (char const one : text)
                if (!allowed->has(one))
                    refuse(name, fmt("the line holds \"{}\", which is not in \"{}\"", escaped(std::string(1, one)),
                                     allowed->text()));
        if (bounds == stated::yes)
            remember(name, "length", least, most, length == least, length == most,
                     where);
        end_the_line(name);
        return text;
    }

    long long line_into(std::string& text, long long cap, value_name const& name) {
        settle();
        long long seen = 0;
        while (true) {
            int const next = from_.peek();
            if (next < 0 || next == '\n') break;
            std::size_t const run = plain_run();
            if (run > 0) {
                std::size_t const room = cap == 0 ? run : seen >= cap ? 0 : static_cast<std::size_t>(cap - seen);
                text.append(from_.window(), std::min(run, room));
                seen += static_cast<long long>(run);
                from_.skip_plain(run);
                continue;
            }
            from_.take();
            seen++;
            if (cap == 0 || seen <= cap) text.push_back(static_cast<char>(next));
        }
        if (from_.peek() < 0 && !lenient_) refuse(name, "the line has no line break at its end");
        if (lenient_ && !text.empty() && text.back() == '\r') {
            text.pop_back();
            seen--;
        }
        return seen;
    }

    void end_the_line(value_name const& name) {
        if (from_.peek() == '\n') from_.take();
        was_read(name);
        separated_ = true;
    }

    std::string line_up_to(std::size_t keep, bool& longer, value_name const& name) {
        settle();
        std::string text;
        longer = false;
        while (true) {
            int const next = from_.peek();
            if (next < 0 || next == '\n') break;
            std::size_t const run = plain_run();
            if (run > 0) {
                char const* const at = from_.window();
                std::size_t const kept = std::min(run, keep - std::min(keep, text.size()));
                text.append(at, kept);
                for (std::size_t past = kept; past < run && !longer; past++)
                    if (at[past] != ' ' && at[past] != '\t') longer = true;
                from_.skip_plain(run);
                continue;
            }
            from_.take();
            if (text.size() < keep) text.push_back(static_cast<char>(next));
            else if (next != ' ' && next != '\t' && next != '\r') longer = true;
        }
        if (from_.peek() == '\n') from_.take();
        was_read(name);
        separated_ = true;
        return text;
    }

    void start_value(value_name const& name, site where, char const* expected) {
        settle();
        if (lenient_) skip_blanks(true);
        int const here = from_.peek();
        if (here >= 0 && !is_blank(here)) return;
        if (here < 0 && !end_text_.empty()) refuse(name, end_text_);
        if (here >= 0 && (last_indexed_ || !last_value_.empty()) && !separated_) missing_separator(name, where, here);
        refuse(name, fmt("expected {}, found {}", expected, name_of(here)));
    }

    void refuse_a_number_that_goes_on(int found) {
        if (just_read_ == number_read::none || found < 0 || is_blank(found)) return;
        std::string const read = just_read_ == number_read::integer ? fmt("{}", integer_just_read_) : real_just_read_;
        refuse_the_whole_number(named_just_read_ ? value_name(last_value()) : value_name(unnamed), read, just_read_);
    }

    [[noreturn]] void missing_separator(value_name const& name, site where, int found) {
        char const* const call = found == '\n' ? "read_eoln()" : "read_space()";
        finish(3, fmt("{}: {}{}: {} follows {}; read it with {}", where_of(where), case_prefix(), line_of(name),
                      name_of(found), last_value(), call));
    }

    static long long constexpr longest_number = 4096;
    static std::size_t constexpr lookahead = 64;

    std::string take_number(value_name const& name, site where, bool with_a_point, char const* expected) {
        start_value(name, where, expected);
        if (lenient_) settle();
        return number_here(name, with_a_point, expected);
    }

    integer_read spelled_integer(value_name const& name) {
        std::string const token = number_here(name, false, "an integer");
        integer_read const parsed = parse_integer(token, relaxed_);
        if (parsed.problem != number_problem::none) {
            if (token_goes_on()) refuse_the_whole_number(name, token, number_read::integer);
            refuse(name, fmt("expected an integer, found \"{}\": {}", shorten(token), describe(parsed.problem)));
        }
        return parsed;
    }

    bool quick_integer(long long& value) {
        char const* const at = from_.window();
        std::size_t const held = from_.held();
        std::size_t const sign = at[0] == '-' ? 1 : 0;
        std::size_t end = sign;
        unsigned long long magnitude = 0;
        while (end < held && end - sign < 19 && is_digit(at[end]))
            magnitude = magnitude * 10 + static_cast<unsigned long long>(at[end++] - '0');
        std::size_t const digits = end - sign;
        if (end == held || digits == 0 || digits > 18) return false;
        if (at[sign] == '0' && (digits > 1 || sign == 1)) return false;
        if (lenient_ && !is_blank(static_cast<unsigned char>(at[end]))) return false;
        value = sign == 1 ? -static_cast<long long>(magnitude) : static_cast<long long>(magnitude);
        from_.skip_plain(end);
        return true;
    }

    std::string number_here(value_name const& name, bool with_a_point, char const* expected) {
        if (lenient_) {
            std::string word;
            word_here(word, name, longest_number);
            if (word.empty()) refuse(name, fmt("expected {}, found nothing", expected));
            if (from_.peek() >= 0 && !is_blank(from_.peek()))
                refuse(name, fmt("expected {}, found a token longer than {} characters: \"{}\"", expected,
                                 longest_number, shorten(word)));
            return word;
        }
        std::string token;
        if (from_.peek() == '-' || (relaxed_ && from_.peek() == '+'))
            token.push_back(static_cast<char>(from_.take()));
        digits_into(token);
        if (with_a_point && from_.peek() == '.') {
            token.push_back(static_cast<char>(from_.take()));
            digits_into(token);
        }
        if (token.empty() || token == "-" || token == "+")
            refuse(name, fmt("expected {}, found \"{}\"", expected, shorten(token + ahead_of_the_value())));
        was_read(name);
        return token;
    }

    std::size_t plain_run() const {
        char const* const at = from_.window();
        std::size_t const held = from_.held();
        char const* const line_end = static_cast<char const*>(std::memchr(at, '\n', held));
        std::size_t const line = line_end == nullptr ? held : static_cast<std::size_t>(line_end - at);
        char const* const carriage = static_cast<char const*>(std::memchr(at, '\r', line));
        return carriage == nullptr ? line : static_cast<std::size_t>(carriage - at);
    }

    void digits_into(std::string& token) {
        while (from_.peek() >= '0' && from_.peek() <= '9') {
            char const* const at = from_.window();
            std::size_t const held = from_.held();
            std::size_t run = 0;
            while (run < held && at[run] >= '0' && at[run] <= '9') run++;
            token.append(at, run);
            from_.skip_plain(run);
        }
    }

    std::string take_word(value_name const& name, site where, char const* expected, long long cap = 0) {
        std::string token;
        take_word_into(token, name, where, expected, cap);
        return token;
    }

    void take_word_into(std::string& token, value_name const& name, site where, char const* expected,
                        long long cap = 0) {
        start_value(name, where, expected);
        word_here(token, name, cap);
    }

    void word_here(std::string& token, value_name const& name, long long cap) {
        token.clear();
        while (true) {
            int const next = from_.peek();
            if (next < 0 || is_blank(next)) break;
            if (cap > 0 && static_cast<long long>(token.size()) >= cap) break;
            char const* const at = from_.window();
            std::size_t held = from_.held();
            if (cap > 0) held = std::min<std::size_t>(held, static_cast<std::size_t>(cap) - token.size());
            std::size_t run = 0;
            while (run < held && !is_blank(static_cast<unsigned char>(at[run]))) run++;
            token.append(at, run);
            from_.skip_plain(run);
        }
        was_read(name);
    }

    template <class T, class Read>
    std::vector<T> many(long long count, value_name const& name, Read read_one) {
        std::vector<T> values;
        values.reserve(room_for(count, name));
        for (long long at = 1; at <= count; at++) values.push_back(read_one(name.lent_at(at)));
        return values;
    }

    std::string choice(std::initializer_list<char const*> choices, bool fold, value_name const& name, site where) {
        std::string found = take_word(name, where, "a token", longest_of(choices));
        for (char const* one : choices) {
            if (found == one) return found;
            if (fold && same_folded(found, one)) return std::string(one);
        }
        refuse(name, fmt("\"{}\" is not one of {}", shorten(found),
                         joined(choices, [](char const* one) { return std::string(one); })));
    }

    void study(value_name const& name, long long low, long long high, stated bounds, long long type_low,
               long long type_high, char const* type_word, site where) {
        if (name.absent() && fresh("EO101", where))
            warn("EO101", "this value is read without a name", "name it, or say eo::unnamed if it needs none",
                 where);
        if (bounds == stated::absent) {
            if (fresh(loose_code_, where))
                warn(loose_code_, "this value is read without bounds", "give the bounds, or say eo::any", where);
            return;
        }
        if (bounds != stated::yes) return;
        if (last_study_.quiet && last_study_.low == low && last_study_.high == high &&
            last_study_.type_low == type_low && last_study_.type_high == type_high)
            return;
        bool const whole_range = low == type_low && high == type_high;
        bool const too_wide = low < type_low || high > type_high;
        bool const high_near = nearly_round(high);
        bool const low_near = nearly_round(low);
        if (whole_range && fresh("EO104", where))
            warn("EO104", fmt("the bounds are the whole range of {}", type_word),
                 "say eo::any if any value is allowed", where);
        if (too_wide && fresh("EO105", where))
            warn("EO105", fmt("the bounds {}..{} do not fit {}", low, high, type_word), "read a wider type",
                 where);
        if (((high_near && !read_before(high)) || (low_near && !read_before(low))) && fresh("EO106", where))
            note("EO106", fmt("the bounds {}..{} are one away from a round number", low, high),
                 "compare them with the statement", where);
        last_study_ = {!whole_range && !too_wide && !high_near && !low_near, low, high, type_low, type_high};
    }

private:
    static bool fresh(char const* code, site where) { return !diagnostics::shared().again(code, where); }

    bool token_goes_on() {
        if (lenient_) return false;
        int const next = from_.peek();
        return next >= 0 && !is_blank(next);
    }

    [[noreturn]] void refuse_the_whole_number(value_name const& name, std::string token, number_read kind) {
        token += ahead_of_the_value();
        bool const real = kind == number_read::real;
        number_problem const problem =
            real ? parse_real(token, exponents_, lenient_).problem : parse_integer(token, relaxed_).problem;
        refuse(name, fmt("expected {}, found \"{}\": {}", real ? "a number" : "an integer", shorten(token),
                         describe(problem)));
    }

    static void note_a_large_token(std::string const& token, site where) {
        if (token.size() > mebibyte && fresh("EO111", where))
            note("EO111", fmt("a token of {} bytes was held in memory", token.size()),
                 "bound its length if the format allows", where);
    }

    bool read_before(long long bound) const {
        for (auto const& one : bounds_)
            if (one.second.read_whole && one.second.last_whole >= bound - 1 && one.second.last_whole <= bound + 1)
                return true;
        return false;
    }

    std::string line_of(value_name const& name) const {
        std::string line = fmt("line {}", from_.line());
        if (name.known()) line += fmt(", {}", name.text());
        return line;
    }

    char const* verdict_word() const {
        if (whose_ == fault::wrong_answer) return "wrong answer: ";
        if (whose_ == fault::jury_error) return "jury error: ";
        return "";
    }

    void settle() {
        if (flush_ != nullptr) flush_(owner_);
    }

    void skip_blanks(bool across_lines) {
        while (true) {
            int const here = from_.peek();
            if (here < 0) return;
            if (here == '\n' && !across_lines) return;
            if (!is_blank(here)) return;
            from_.take();
        }
    }

    void was_read(value_name const& name) {
        just_read_ = number_read::none;
        if (!lenient_) {
            named_just_read_ = name.known();
            if (!name.known()) last_value_ = "the value before";
            else if (last_value_ != name.key()) last_value_ = name.key();
            last_indexed_ = name.known() && name.indexed();
            last_index_ = name.index();
        }
        separated_ = false;
        read_anything_ = true;
    }

    template <class Bound>
    void remember(value_name const& name, char const* kind, Bound low, Bound high, bool at_low, bool at_high,
                  site where) {
        if (!name.known()) return;
        if (last_bounds_ == nullptr || last_key_ != name.key()) {
            auto const found = bounds_.find(name.key());
            if (found == bounds_.end()) {
                seen_bounds fresh{kind, fmt("{}", low), fmt("{}", high), at_low, at_high, where, true};
                fresh.spelled = kind;
                note_the_numbers(fresh, low, high);
                last_bounds_ = &bounds_.emplace(name.key(), std::move(fresh)).first->second;
                last_key_ = name.key();
                return;
            }
            last_bounds_ = &found->second;
            last_key_ = name.key();
        }
        seen_bounds& known = *last_bounds_;
        if ((known.spelled == kind || known.kind == kind) && same_numbers(known, low, high)) {
            if (at_low) known.reached_low = true;
            if (at_high) known.reached_high = true;
            return;
        }
        bool const same_place = known.where.line == where.line && same_text(known.where.file, where.file);
        if (known.kind != kind)
            warn("EO107",
                 fmt("\"{}\" is read as {} here and as {} elsewhere", name.key(), phrase_of(kind),
                     phrase_of(known.kind)),
                 "read it one way", where);
        else {
            known.steady = false;
            if (!same_place)
                warn("EO107",
                     fmt("\"{}\" is read as {}..{} here and as {}..{} elsewhere", name.key(), low, high,
                         known.low, known.high),
                     "constrain it one way", where);
        }
        if (at_low) known.reached_low = true;
        if (at_high) known.reached_high = true;
    }

    static void note_the_numbers(seen_bounds& into, long long low, long long high) {
        into.whole_low = low;
        into.whole_high = high;
    }

    static void note_the_numbers(seen_bounds& into, double low, double high) {
        into.exact_low = low;
        into.exact_high = high;
    }

    static bool same_numbers(seen_bounds const& known, long long low, long long high) {
        return known.whole_low == low && known.whole_high == high;
    }

    static bool same_numbers(seen_bounds const& known, double low, double high) {
        return known.exact_low == low && known.exact_high == high;
    }

    source from_;
    fault whose_ = fault::invalid_test;
    std::string label_;
    bool lenient_ = false;
    char const* loose_code_ = "EO102";
    std::string last_value_;
    bool last_indexed_ = false;
    long long last_index_ = 0;
    std::map<std::string, seen_bounds> bounds_;
    std::string last_key_;
    seen_bounds* last_bounds_ = nullptr;
    bool separated_ = true;
    bool read_anything_ = false;
    bool exponents_ = false;
    bool relaxed_ = false;
    void (*flush_)(void*) = nullptr;
    void* owner_ = nullptr;
    std::string end_text_;
    studied_bounds last_study_;
    number_read just_read_ = number_read::none;
    long long integer_just_read_ = 0;
    std::string real_just_read_;
    bool named_just_read_ = false;
    integer_span last_integer_;
};

}  // namespace detail
}  // namespace eo
