#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "stream.h"

namespace eo {

struct rounding {
    int digits;
};

inline rounding round_to(int digits) { return rounding{digits}; }

inline double ratio(long long part, long long whole) {
    if (whole == 0) detail::library_error("eo::ratio divides by zero");
    return static_cast<double>(part) / static_cast<double>(whole);
}

inline bool close_enough(double expected, double found, double epsilon) {
    double const spread = std::fabs(expected - found);
    if (spread <= epsilon) return true;
    double const scale = std::fabs(expected);
    return scale > 0 && spread / scale <= epsilon;
}

namespace detail {

class scorer;

class limits_keeper {
public:
    virtual void declare_budget() = 0;
    virtual void spent_a_budget() = 0;

protected:
    ~limits_keeper() = default;
};

inline std::array<char const*, 3> test_paths(int argc, char** argv) {
    char const* const names[3] = {"INPUT_FILE", "OUTPUT_FILE", "ANSWER_FILE"};
    std::array<char const*, 3> paths{};
    for (int at = 0; at < 3; at++) {
        char const* const set = environment(names[at]);
        paths[static_cast<std::size_t>(at)] = set != nullptr ? set : at + 1 < argc ? argv[at + 1] : nullptr;
    }
    return paths;
}

inline scorer*& live_scorer() {
    static scorer* only = nullptr;
    return only;
}

inline scorer& judging() {
    if (live_scorer() == nullptr) library_error("this verdict needs an eo::checker or an eo::interactor");
    return *live_scorer();
}

inline reader*& blaming() {
    static reader* current = nullptr;
    return current;
}

class blame_guard {
public:
    explicit blame_guard(reader* who) : before_(blaming()) { blaming() = who; }
    blame_guard(blame_guard const&) = delete;
    blame_guard& operator=(blame_guard const&) = delete;
    ~blame_guard() { blaming() = before_; }

private:
    reader* before_;
};

struct scored {
    double value;
    site where;

    template <class T, class = std::enable_if_t<std::is_convertible_v<T, double>>>
    scored(T&& what, char const* file = __builtin_FILE(), int line = __builtin_LINE())
        : value(static_cast<double>(std::forward<T>(what))), where{file, line} {}
};

[[noreturn]] inline void refuse_a_score(std::string const& what) {
    library_error(fmt("{} is not a number the judge can pay; look for zero divided by zero, or an infinity "
                      "less an infinity, in the formula", what));
}

inline double clamped(double fraction, site where) {
    if (std::isnan(fraction)) refuse_a_score(fmt("a score of {}", fraction));
    if (fraction >= 2 && std::isfinite(fraction)) {
        warn("EO205", fmt("a score of {} was clamped to 1; it looks like a percentage or points, and eo::score "
                          "takes a fraction of the test", fraction),
             "use eo::ratio(a, b) for a out of b, or eo::points for points", where);
        return 1.0;
    }
    if (fraction < 0 || fraction > 1) {
        warn("EO205", fmt("a score of {} was clamped into 0..1", fraction), "keep the formula inside the test",
             where);
        return fraction < 0 ? 0.0 : 1.0;
    }
    if (fraction > 0 && fraction < 1 && fraction > 1 - 1e-9)
        warn("EO206", fmt("a score of {} is a hair below full marks, which the judge may read as full marks",
                          fraction),
             "use eo::ratio(a, b), which is exact", where);
    return fraction;
}

inline std::string format_points(double value) {
    if (value == std::floor(value) && std::fabs(value) < 1e15) {
        std::string out;
        append_integer(out, static_cast<long long>(value));
        return out;
    }
    char buffer[40];
    int const written = std::snprintf(buffer, sizeof(buffer), "%.10g", value);
    return std::string(buffer, static_cast<std::size_t>(written));
}

inline double test_cost() {
    char const* const set = environment("TEST_COST");
    if (set == nullptr) {
        if (on_judge() && !diagnostics::shared().raised_already("EO213"))
            warn("EO213", "TEST_COST is not set, so this test is taken to be worth 100 points",
                 "points and partial scores follow from the cost; report the judge's configuration", site::here());
        return 100;
    }
    real_read const parsed = parse_real(set, true);
    if (parsed.problem == number_problem::none) return parsed.value;
    if (on_judge() && !diagnostics::shared().raised_already("EO213"))
        warn("EO213", fmt("TEST_COST is \"{}\", which is not a number, so this test is taken to be worth 0 points",
                          shorten(set)),
             "points and partial scores follow from the cost; report the judge's configuration", site::here());
    return 0;
}

class scorer {
public:
    double cost() const { return test_cost(); }
    virtual void pass(double fraction, std::string const& message) = 0;
    virtual void fail_run(std::string const& message) = 0;
    virtual void fail_jury(std::string const& message) = 0;

protected:
    ~scorer() = default;

    void fail_closed(char const* role) {
        if (delivered_) return;
        if (std::uncaught_exceptions() == 0) fail_jury(fmt("the {} ended without a verdict", role));
#ifndef EOLYMP_TESTING
        fail_jury(fmt("an exception left the {} before its verdict; catch it inside the {}'s scope and give a "
                      "verdict there, or let it end the program",
                      role, role));
#endif
    }

    bool delivered_ = false;
};

inline double rounded(double value, int digits) {
    if (digits > 15) return value;
    double scale = 1;
    for (int at = 0; at < digits; at++) scale *= 10;
    return std::round(value * scale) / scale;
}

}  // namespace detail

class budget {
public:
    template <class Owner>
#if defined(__clang__) || __GNUC__ >= 10
    [[nodiscard]]
#endif
    budget(Owner& owner, long long limit, std::string name)
        : keeper_(&owner), judge_(&owner), limit_(limit), name_(std::move(name)) {
        keeper_->declare_budget();
    }

    budget(budget const&) = delete;
    budget& operator=(budget const&) = delete;

    void spend(long long how_many = 1) {
        keeper_->spent_a_budget();
        used_ += how_many;
        if (used_ > limit_) judge_->fail_run(fmt("more than {} {}", limit_, name_));
    }

    void restart() { used_ = 0; }

    void restart(long long limit) {
        limit_ = limit;
        used_ = 0;
    }

    long long used() const { return used_; }
    long long left() const { return limit_ - used_; }
    long long limit() const { return limit_; }

private:
    detail::limits_keeper* keeper_;
    detail::scorer* judge_;
    long long limit_;
    long long used_ = 0;
    std::string name_;
};

template <class... Args>
[[noreturn]] inline void accept(detail::pattern pattern = "", Args const&... args) {
    detail::judging().pass(1, fmt(pattern, args...));
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
[[noreturn]] inline void wrong(detail::pattern pattern = "", Args const&... args) {
    std::string const message = fmt(pattern, args...);
    if (detail::blaming() != nullptr) detail::blaming()->refuse(detail::value_name(unnamed), message);
    detail::judging().fail_run(message);
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
[[noreturn]] inline void jury_error(detail::pattern pattern = "", Args const&... args) {
    detail::judging().fail_jury(fmt(pattern, args...));
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
[[noreturn]] inline void score(detail::scored fraction, detail::pattern pattern = "", Args const&... args) {
    detail::judging().pass(detail::clamped(fraction.value, fraction.where), fmt(pattern, args...));
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
[[noreturn]] inline void score(detail::scored fraction, rounding how, detail::pattern pattern = "",
                               Args const&... args) {
    detail::scorer& one = detail::judging();
    double const paid = detail::rounded(detail::clamped(fraction.value, fraction.where) * one.cost(), how.digits);
    one.pass(one.cost() > 0 ? paid / one.cost() : 0, fmt(pattern, args...));
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
[[noreturn]] inline void points(detail::scored given, detail::pattern pattern = "", Args const&... args) {
    detail::scorer& one = detail::judging();
    double paid = given.value;
    if (std::isnan(paid)) detail::refuse_a_score(fmt("{} points", paid));
    if (paid < 0) {
        detail::warn("EO205", fmt("{} points was clamped to 0", paid), "keep the formula inside the test",
                     given.where);
        paid = 0;
    }
    if (paid > one.cost())
        detail::warn("EO207", fmt("{} points is more than the test's {}", paid, one.cost()),
                     "the judge clamps it", given.where);
    one.pass(one.cost() > 0 ? paid / one.cost() : 0, fmt(pattern, args...));
    __builtin_unreachable();  // LCOV_EXCL: the verdict above ends the program
}

template <class... Args>
inline void log(detail::pattern pattern, Args const&... args) {
    detail::log_line(fmt(pattern, args...));
}

}  // namespace eo
