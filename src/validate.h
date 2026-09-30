#pragma once

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "parse.h"
#include "read.h"
#include "structure.h"

namespace eo {

class validator;

namespace detail {

class registered_sum;

inline std::vector<registered_sum*>& live_sums() {
    static std::vector<registered_sum*> all;
    return all;
}

class registered_sum {
public:
    virtual void verify() = 0;

protected:
    ~registered_sum() = default;
};

inline long long& sums_ever_made() {
    static long long count = 0;
    return count;
}

template <class T, class = void>
struct comparable : std::false_type {};

template <class T>
struct comparable<T, std::void_t<decltype(std::declval<T const&>() == std::declval<T const&>())>>
    : std::true_type {};

inline validator*& live_validator() {
    static validator* only = nullptr;
    return only;
}

}  // namespace detail

class sum_limit : public detail::registered_sum {
public:
#if defined(__clang__) || __GNUC__ >= 10
    [[nodiscard]]
#endif
    sum_limit(long long limit, std::string name) : limit_(limit), name_(std::move(name)) {
        detail::live_sums().push_back(this);
        detail::sums_ever_made()++;
    }

    sum_limit(sum_limit const&) = delete;
    sum_limit& operator=(sum_limit const&) = delete;

    ~sum_limit() noexcept(false) {
        std::vector<detail::registered_sum*>& all = detail::live_sums();
        all.erase(std::remove(all.begin(), all.end(), this), all.end());
        if (std::uncaught_exceptions() == 0) verify();
    }

    sum_limit& operator+=(long long value) {
        long long sum = 0;
        if (__builtin_add_overflow(total_, value, &sum))
            detail::finish(3, fmt("{} does not fit a long long: {} was added to {}", name_, value, total_));
        total_ = sum;
        return *this;
    }

    long long total() const { return total_; }

    void verify() override {
        if (checked_) return;
        checked_ = true;
        if (total_ > limit_) detail::finish(3, fmt("{} is {}, above {}", name_, total_, limit_));
    }

private:
    long long limit_;
    long long total_ = 0;
    std::string name_;
    bool checked_ = false;
};

template <class Limits>
struct subtask_row {
    int group;
    Limits limits;
};

template <class Limits>
class subtask_table {
public:
    subtask_table(validator& owner, std::vector<subtask_row<Limits>> rows, detail::site where);
    Limits without_group(Limits fallback);

private:
    validator* owner_;
    std::vector<subtask_row<Limits>> rows_;
    detail::site where_;
};

class validator {
public:
    validator(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_validator() != nullptr)
            detail::library_error(fmt("{}: this program already has a validator", detail::where_of(where)));
        detail::diagnostics::shared().start_the_clock("EO303", "validator", 30000, where);
        std::string path;
        for (int at = 1; at < argc; at++) {
            std::string const argument = argv[at];
            if (argument == "--eo-describe") {
                describing_ = true;
            } else if (argument == "--group") {
                if (at + 1 >= argc) detail::library_error("--group needs the testset index after it");
                set_group(argv[at + 1]);
                at++;
            } else if (argument.rfind("--group=", 0) == 0) {
                set_group(argument.substr(8));
            } else if (argument.rfind("-", 0) != 0) {
                path = argument;
            }
        }
        from_ = detail::reader(path.empty() ? detail::source::over_descriptor(0, false, true)
                                             : detail::source::over_file(path.c_str(), true),
                               detail::fault::invalid_test, "", false, "EO102");
        detail::live_validator() = this;
    }

    validator(validator const&) = delete;
    validator& operator=(validator const&) = delete;

    ~validator() noexcept(false) {
        detail::live_validator() = nullptr;
        detail::current_case() = 0;
        if (std::uncaught_exceptions() == 0) complete();
    }

    void read_space() { expect(' ', "a space"); }

    void read_eoln() { expect('\n', "a line break"); }

    void read_char(char wanted) { expect(wanted, nullptr); }

    void read_eof() {
        check_the_end();
        ended_ = true;
    }

    bool at_eoln() { return from_.at_line_end(); }
    bool at_eof() { return from_.at_end(); }

    int read_int(long long low, long long high, detail::value_name name,
                 detail::site where = detail::site::here()) {
        return whole_int(low, high, detail::stated::yes, name, where);
    }

    int read_int(any_t, detail::value_name name,
                 detail::site where = detail::site::here()) {
        return whole_int(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_int(low, high, eo::unnamed)")]] int
    read_int(long long low, long long high,
             detail::site where = detail::site::here()) {
        return whole_int(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO102: bound this value, or say read_int(eo::any, name)")]] int
    read_int(detail::value_name name, detail::site where = detail::site::here()) {
        return whole_int(0, 0, detail::stated::absent, name, where);
    }

    [[deprecated("eolymp EO101 and EO102: bound and name this value")]] int
    read_int(detail::site where = detail::site::here()) {
        return whole_int(0, 0, detail::stated::absent, detail::value_name::nothing(), where);
    }

    long long read_long(long long low, long long high, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return whole_long(low, high, detail::stated::yes, name, where);
    }

    long long read_long(any_t, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return whole_long(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_long(low, high, eo::unnamed)")]] long long
    read_long(long long low, long long high,
              detail::site where = detail::site::here()) {
        return whole_long(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO102: bound this value, or say read_long(eo::any, name)")]] long long
    read_long(detail::value_name name, detail::site where = detail::site::here()) {
        return whole_long(0, 0, detail::stated::absent, name, where);
    }

    double read_real(double low, double high, int least_decimals, int most_decimals, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true, std::move(name),
                          where);
    }

    double read_real(double low, double high, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return fractional(low, high, detail::stated::yes, 0, 0, false, std::move(name), where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_real(low, high, least, most, eo::unnamed)")]] double
    read_real(double low, double high, int least_decimals, int most_decimals,
              detail::site where = detail::site::here()) {
        return fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true,
                          detail::value_name::nothing(), where);
    }

    std::string read_token(long long least, long long most, charset allowed, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return word(least, most, &allowed, detail::stated::yes, std::move(name), where);
    }

    std::string read_token(long long least, long long most, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return word(least, most, nullptr, detail::stated::yes, std::move(name), where);
    }

    [[deprecated("eolymp EO101: name this token, or say read_token(least, most, allowed, eo::unnamed)")]] std::string
    read_token(long long least, long long most, charset allowed,
               detail::site where = detail::site::here()) {
        return word(least, most, &allowed, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO108: give a length and a charset, or say read_token(eo::any, name)")]] std::string
    read_token(detail::value_name name, detail::site where = detail::site::here()) {
        return word(0, 0, nullptr, detail::stated::absent, std::move(name), where);
    }

    std::string read_token(any_t, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return word(0, 0, nullptr, detail::stated::deliberate, std::move(name), where);
    }

    std::string read_line(long long least, long long most, charset allowed, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return rest_of_line(least, most, &allowed, detail::stated::yes, std::move(name), where);
    }

    std::string read_line(long long least, long long most, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return rest_of_line(least, most, nullptr, detail::stated::yes, std::move(name), where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, detail::value_name name,
                            detail::site where = detail::site::here()) {
        std::string const found = from_.take_word(name, where, "a token", from_.longest_of(choices));
        for (char const* one : choices)
            if (found == one) return found;
        std::string listed;
        for (char const* one : choices) listed += (listed.empty() ? "" : ", ") + std::string(one);
        invalid(name, fmt("\"{}\" is not one of {}", detail::shorten(found), listed));
    }

    std::vector<int> read_ints(long long count, long long low, long long high, detail::value_name name,
                               detail::site where = detail::site::here()) {
        return many<int>(count, name, [&](detail::value_name each) {
            return whole_int(low, high, detail::stated::yes, std::move(each), where);
        });
    }

    std::vector<int> read_ints(long long count, any_t, detail::value_name name,
                               detail::site where = detail::site::here()) {
        return many<int>(count, name, [&](detail::value_name each) {
            return whole_int(0, 0, detail::stated::deliberate, std::move(each), where);
        });
    }

    std::vector<long long> read_longs(long long count, long long low, long long high, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return many<long long>(count, name, [&](detail::value_name each) {
            return whole_long(low, high, detail::stated::yes, std::move(each), where);
        });
    }

    std::vector<long long> read_longs(long long count, any_t, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return many<long long>(count, name, [&](detail::value_name each) {
            return whole_long(0, 0, detail::stated::deliberate, std::move(each), where);
        });
    }

    std::vector<double> read_reals(long long count, double low, double high, int least_decimals,
                                   int most_decimals, detail::value_name name,
                                   detail::site where = detail::site::here()) {
        return many<double>(count, name, [&](detail::value_name each) {
            return fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true,
                              std::move(each), where);
        });
    }

    std::vector<std::string> read_tokens(long long count, long long least, long long most, charset allowed,
                                         detail::value_name name,
                                         detail::site where = detail::site::here()) {
        return many<std::string>(count, name, [&](detail::value_name each) {
            return word(least, most, &allowed, detail::stated::yes, std::move(each), where);
        });
    }

    std::vector<edge> read_tree(int n, detail::value_name name,
                                detail::site where = detail::site::here()) {
        std::vector<edge> edges = edge_lines(n - 1, name, where);
        require(is_tree(n, edges), name);
        return edges;
    }

    std::vector<edge> read_graph(int n, int m, graph_shape shape, detail::value_name name,
                                 detail::site where = detail::site::here()) {
        std::vector<edge> edges = edge_lines(m, name, where);
        require(detail::vertices_are_inside(n, edges), name);
        if ((shape & simple) != 0) require(is_simple_graph(n, edges), name);
        if ((shape & connected) != 0) require(is_connected(n, edges), name);
        return edges;
    }

    std::vector<int> read_permutation(int n, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        std::vector<int> values = read_ints(n, 1, n, name, where);
        require(is_permutation(values), name);
        return values;
    }

    template <class Limits>
    subtask_table<Limits> subtasks(std::initializer_list<subtask_row<Limits>> rows,
                                   detail::site where = detail::site::here()) {
        declared_subtasks_ = true;
        return subtask_table<Limits>(*this, std::vector<subtask_row<Limits>>(rows), where);
    }

    std::optional<int> group() const { return group_; }

    template <class Body>
    void cases(long long count, Body&& body) {
        used_cases_ = true;
        for (long long index = 1; index <= count; index++) {
            detail::current_case() = index;
            body();
        }
        detail::current_case() = 0;
    }

    template <class... Args>
    void require(bool condition, detail::pattern message, Args const&... args) {
        if (!condition) invalid(detail::value_name(unnamed), fmt(message, args...));
    }

    void require(check_result const& outcome, detail::value_name name) {
        if (outcome) return;
        invalid(detail::value_name(unnamed),
                name.known() ? fmt("{}: {}", name.text(), outcome.message()) : outcome.message());
    }

    void feature(std::string name) { features_.emplace(std::move(name), false); }

    void saw(std::string const& name) {
        auto const found = features_.find(name);
        if (found == features_.end())
            detail::library_error(fmt("saw(\"{}\") names a feature that was never declared", name));
        found->second = true;
    }

    [[noreturn]] void invalid(detail::value_name const& name, std::string what) {
        from_.refuse(name, what);
    }

    void complete() {
        if (completed_) return;
        completed_ = true;
        if (!ended_) check_the_end();
        std::vector<detail::registered_sum*> const sums = detail::live_sums();
        for (detail::registered_sum* one : sums) one->verify();
        closing_warnings();
        if (describing_) describe();
        detail::diagnostics::shared().emit();
    }

private:
    template <class Limits>
    friend class subtask_table;


    void set_group(std::string const& text) {
        detail::integer_read const parsed = detail::parse_integer(text);
        if (parsed.problem != detail::number_problem::none)
            detail::library_error(fmt("--group {} is not a testset index", text));
        group_ = static_cast<int>(parsed.value);
    }

    void expect(char wanted, char const* description) {
        int const here = from_.peek();
        if (here == wanted) {
            from_.take();
            from_.mark_separated();
            return;
        }
        std::string const want = description != nullptr ? std::string(description) : fmt("\"{}\"", wanted);
        std::string const after =
            from_.last_value().empty() ? std::string() : fmt(" after {}", from_.last_value());
        from_.refuse(detail::value_name(unnamed),
                     fmt("expected {}{}, found {}", want, after, found_name(here)));
    }

    std::string found_name(int character) {
        char const* const known = detail::name_of(character);
        if (known[0] != '\0') return known;
        return fmt("\"{}\"", detail::escaped(std::string(1, static_cast<char>(character))));
    }

    int whole_int(long long low, long long high, detail::stated bounds, detail::value_name name,
                  detail::site where) {
        return from_.whole_int(low, high, bounds, name, where);
    }

    long long whole_long(long long low, long long high, detail::stated bounds, detail::value_name name,
                         detail::site where) {
        return from_.whole_long(low, high, bounds, name, where);
    }

    double fractional(double low, double high, detail::stated bounds, int least_decimals, int most_decimals,
                      bool decimals_stated, detail::value_name name, detail::site where) {
        return from_.fractional(low, high, bounds, least_decimals, most_decimals, decimals_stated, name,
                                where);
    }

    std::string word(long long least, long long most, charset const* allowed, detail::stated bounds,
                     detail::value_name name, detail::site where) {
        return from_.word(least, most, allowed, bounds, name, where);
    }

    std::string rest_of_line(long long least, long long most, charset const* allowed, detail::stated bounds,
                             detail::value_name name, detail::site where) {
        return from_.rest_of_line(least, most, allowed, bounds, name, where);
    }

    template <class T, class Read>
    std::vector<T> many(long long count, detail::value_name const& name, Read read_one) {
        std::vector<T> values;
        values.reserve(from_.room_for(count, name));
        for (long long index = 1; index <= count; index++) {
            if (index > 1) read_space();
            values.push_back(read_one(name.lent_at(index)));
        }
        return values;
    }

    std::vector<edge> edge_lines(int count, detail::value_name const& name, detail::site where) {
        std::vector<edge> edges;
        edges.reserve(static_cast<std::size_t>(std::max(count, 0)));
        for (int index = 1; index <= count; index++) {
            int const u = whole_int(0, 0, detail::stated::deliberate, name.lent_at(index), where);
            read_space();
            int const v = whole_int(0, 0, detail::stated::deliberate, name.lent_at(index), where);
            read_eoln();
            edges.push_back(edge{u, v});
        }
        return edges;
    }

    void check_the_end() {
        if (from_.peek() < 0) return;
        from_.refuse(detail::value_name(unnamed),
                     fmt("expected the end of the input, found \"{}\"",
                         detail::shorten(from_.rest_of_the_input())));
    }

    void closing_warnings() {
        if (from_.carriage_returns() && !detail::on_judge())
            detail::note("EO110", "this input has CRLF line endings",
                         "the judge converts them, and so does a local run", where_of_run_);
        if (group_.has_value() && *group_ >= 2 && !declared_subtasks_)
            detail::warn("EO301",
                         fmt("this test is in subtask {} and the validator declares no table", *group_),
                         "declare v.subtasks<Limits>({...})", where_of_run_);
        if (used_cases_ && detail::sums_ever_made() == 0)
            detail::note("EO304", "cases is used with no sum_limit",
                         "most multi-test statements bound the sum of n", where_of_run_);
    }

    void describe() {
        std::string out;
        if (group_.has_value()) out += fmt("eo-describe group {}\n", *group_);
        for (auto const& one : from_.bounds())
            out += fmt("eo-describe value {} {} {} {} low={} high={}\n", one.first, one.second.kind,
                       one.second.steady ? one.second.low : "*",
                       one.second.steady ? one.second.high : "*",
                       one.second.reached_low ? "yes" : "no",
                       one.second.reached_high ? "yes" : "no");
        for (auto const& one : features_)
            out += fmt("eo-describe feature {} seen={}\n", one.first, one.second ? "yes" : "no");
        detail::report(out.empty() ? std::string() : out.substr(0, out.size() - 1));
    }

    detail::reader from_;
    std::optional<int> group_;
    std::map<std::string, bool> features_;
    bool completed_ = false;
    bool ended_ = false;
    bool describing_ = false;
    bool declared_subtasks_ = false;
    bool used_cases_ = false;
    detail::site where_of_run_{"validator", 0};
};

template <class Limits>
inline subtask_table<Limits>::subtask_table(validator& owner, std::vector<subtask_row<Limits>> rows,
                                            detail::site where)
    : owner_(&owner), rows_(std::move(rows)), where_(where) {
    for (std::size_t at = 0; at + 1 < rows_.size(); at++)
        for (std::size_t other = at + 1; other < rows_.size(); other++) {
            if (rows_[at].group == rows_[other].group)
                detail::library_error(
                    fmt("{}: subtask {} is listed twice", detail::where_of(where), rows_[at].group));
            if constexpr (detail::comparable<Limits>::value)
                if (rows_[at].limits == rows_[other].limits)
                    detail::warn("EO302",
                                 fmt("subtasks {} and {} have the same limits", rows_[at].group,
                                     rows_[other].group),
                                 "one of them is probably a copy and paste", where);
        }
}

template <class Limits>
inline Limits subtask_table<Limits>::without_group(Limits fallback) {
    std::optional<int> const chosen = owner_->group();
    if (!chosen.has_value()) return fallback;
    for (subtask_row<Limits> const& row : rows_)
        if (row.group == *chosen) return row.limits;
    std::string listed;
    for (subtask_row<Limits> const& row : rows_)
        listed += (listed.empty() ? "" : ", ") + std::to_string(row.group);
    detail::finish(3, fmt("{}: no subtask {}; known: {}", detail::where_of(where_), *chosen, listed));
}

}  // namespace eo
