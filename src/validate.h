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
#include "os.h"
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
        if (detail::sum_overflows(total_, value, &sum))
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
        for (int at = 1; at < argc; at++) {
            std::string const argument = argv[at];
            if (argument == "--eo-describe") {
                describing_ = true;
            } else if (argument == "--eo-case") {
                if (at + 1 >= argc) detail::library_error("--eo-case needs the number of a case after it");
                set_case(argv[at + 1]);
                at++;
            } else if (argument.rfind("--eo-case=", 0) == 0) {
                set_case(argument.substr(10));
            } else if (argument == "--group") {
                if (at + 1 >= argc) detail::library_error("--group needs the testset index after it");
                set_group(argv[at + 1]);
                at++;
            } else if (argument.rfind("--group=", 0) == 0) {
                set_group(argument.substr(8));
            } else if (argument.rfind("-", 0) != 0) {
                path_ = argument;
            }
        }
        if (describing_ && wanted_case_.has_value())
            detail::library_error("--eo-case and --eo-describe both write to stdout; ask for one");
        detail::source input = path_.empty() ? detail::source::over_descriptor(0, false, true)
                                             : detail::source::over_file(path_.c_str(), true);
        input.wait_to_look_ahead();
        from_ = detail::reader(std::move(input), detail::fault::invalid_test, "", false, "EO102");
        detail::live_validator() = this;
        detail::live_sums();
        detail::keep_binary(1);
        detail::close_on_exit(&validator::exited_early);
    }

    validator(validator const&) = delete;
    validator& operator=(validator const&) = delete;

    ~validator() noexcept(false) {
        detail::unfinished() = nullptr;
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
        return from_.whole_int(low, high, detail::stated::yes, name, where);
    }

    int read_int(any_t, detail::value_name name,
                 detail::site where = detail::site::here()) {
        return from_.whole_int(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_int(low, high, eo::unnamed)")]] int
    read_int(long long low, long long high,
             detail::site where = detail::site::here()) {
        return from_.whole_int(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO102: bound this value, or say read_int(eo::any, name)")]] int
    read_int(detail::value_name name, detail::site where = detail::site::here()) {
        return from_.whole_int(0, 0, detail::stated::absent, name, where);
    }

    [[deprecated("eolymp EO101 and EO102: bound and name this value")]] int
    read_int(detail::site where = detail::site::here()) {
        return from_.whole_int(0, 0, detail::stated::absent, detail::value_name::nothing(), where);
    }

    long long read_long(long long low, long long high, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return from_.whole_long(low, high, detail::stated::yes, name, where);
    }

    long long read_long(any_t, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return from_.whole_long(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_long(low, high, eo::unnamed)")]] long long
    read_long(long long low, long long high,
              detail::site where = detail::site::here()) {
        return from_.whole_long(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO102: bound this value, or say read_long(eo::any, name)")]] long long
    read_long(detail::value_name name, detail::site where = detail::site::here()) {
        return from_.whole_long(0, 0, detail::stated::absent, name, where);
    }

    double read_real(double low, double high, int least_decimals, int most_decimals, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return from_.fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true, name, where);
    }

    double read_real(double low, double high, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return from_.fractional(low, high, detail::stated::yes, 0, 0, false, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_real(low, high, least, most, eo::unnamed)")]] double
    read_real(double low, double high, int least_decimals, int most_decimals,
              detail::site where = detail::site::here()) {
        return from_.fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true,
                                detail::value_name::nothing(), where);
    }

    std::string read_token(long long least, long long most, charset allowed, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return from_.word(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_token(long long least, long long most, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return from_.word(least, most, nullptr, detail::stated::yes, name, where);
    }

    [[deprecated("eolymp EO101: name this token, or say read_token(least, most, allowed, eo::unnamed)")]] std::string
    read_token(long long least, long long most, charset allowed,
               detail::site where = detail::site::here()) {
        return from_.word(least, most, &allowed, detail::stated::yes, detail::value_name::nothing(), where);
    }

    [[deprecated("eolymp EO108: give a length and a charset, or say read_token(eo::any, name)")]] std::string
    read_token(detail::value_name name, detail::site where = detail::site::here()) {
        return from_.word(0, 0, nullptr, detail::stated::absent, name, where);
    }

    std::string read_token(any_t, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return from_.word(0, 0, nullptr, detail::stated::deliberate, name, where);
    }

    std::string read_token(pattern const& told, detail::value_name name, detail::site where = detail::site::here()) {
        return from_.matching(told, name, where);
    }

    std::string read_line(pattern const& told, detail::value_name name) { return from_.line_matching(told, name); }

    std::string read_line(long long least, long long most, charset allowed, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return from_.rest_of_line(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_line(long long least, long long most, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return from_.rest_of_line(least, most, nullptr, detail::stated::yes, name, where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, detail::value_name name,
                            detail::site where = detail::site::here()) {
        return from_.choice(choices, false, name, where);
    }

    std::vector<int> read_ints(long long count, long long low, long long high, detail::value_name name,
                               detail::site where = detail::site::here()) {
        return many<int>(count, name, [&](detail::value_name const& each) {
            return from_.whole_int(low, high, detail::stated::yes, each, where);
        });
    }

    std::vector<int> read_ints(long long count, any_t, detail::value_name name,
                               detail::site where = detail::site::here()) {
        return many<int>(count, name, [&](detail::value_name const& each) {
            return from_.whole_int(0, 0, detail::stated::deliberate, each, where);
        });
    }

    std::vector<long long> read_longs(long long count, long long low, long long high, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return many<long long>(count, name, [&](detail::value_name const& each) {
            return from_.whole_long(low, high, detail::stated::yes, each, where);
        });
    }

    std::vector<long long> read_longs(long long count, any_t, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return many<long long>(count, name, [&](detail::value_name const& each) {
            return from_.whole_long(0, 0, detail::stated::deliberate, each, where);
        });
    }

    std::vector<double> read_reals(long long count, double low, double high, int least_decimals,
                                   int most_decimals, detail::value_name name,
                                   detail::site where = detail::site::here()) {
        return many<double>(count, name, [&](detail::value_name const& each) {
            return from_.fractional(low, high, detail::stated::yes, least_decimals, most_decimals, true, each,
                                    where);
        });
    }

    std::vector<std::string> read_tokens(long long count, long long least, long long most, charset allowed,
                                         detail::value_name name,
                                         detail::site where = detail::site::here()) {
        return many<std::string>(count, name, [&](detail::value_name const& each) {
            return from_.word(least, most, &allowed, detail::stated::yes, each, where);
        });
    }

    std::vector<std::string> read_tokens(long long count, pattern const& told, detail::value_name name,
                                         detail::site where = detail::site::here()) {
        return many<std::string>(count, name,
                                 [&](detail::value_name const& each) { return from_.matching(told, each, where); });
    }

    std::vector<std::string> read_grid(long long rows, long long cols, charset allowed, detail::value_name name,
                                       detail::site where = detail::site::here()) {
        std::vector<std::string> grid;
        grid.reserve(from_.room_for(rows, name));
        for (long long row = 1; row <= rows; row++)
            grid.push_back(from_.rest_of_line(cols, cols, &allowed, detail::stated::yes, name.lent_at(row), where));
        return grid;
    }

    std::vector<edge> read_tree(int n, detail::value_name name,
                                detail::site where = detail::site::here()) {
        std::vector<edge> edges = edge_lines<edge>(n - 1, detail::stated::deliberate, n, {0, 0}, name, where);
        require(is_tree(n, edges), name);
        return edges;
    }

    std::vector<weighted_edge> read_tree(int n, weight_bounds weights, detail::value_name name,
                                         detail::site where = detail::site::here()) {
        std::vector<weighted_edge> edges =
            edge_lines<weighted_edge>(n - 1, detail::stated::deliberate, n, weights, name, where);
        require(is_tree(n, detail::endpoints(edges)), name);
        return edges;
    }

    std::vector<edge> read_graph(int n, int m, graph_shape shape, detail::value_name name,
                                 detail::site where = detail::site::here()) {
        std::vector<edge> edges = edge_lines<edge>(m, detail::stated::deliberate, n, {0, 0}, name, where);
        require(detail::shaped(n, edges, shape), name);
        return edges;
    }

    std::vector<weighted_edge> read_graph(int n, int m, graph_shape shape, weight_bounds weights,
                                          detail::value_name name, detail::site where = detail::site::here()) {
        std::vector<weighted_edge> edges =
            edge_lines<weighted_edge>(m, detail::stated::deliberate, n, weights, name, where);
        require(detail::shaped(n, detail::endpoints(edges), shape), name);
        return edges;
    }

    std::vector<edge> read_edges(long long m, int n, detail::value_name name,
                                 detail::site where = detail::site::here()) {
        return edge_lines<edge>(m, detail::stated::yes, n, {0, 0}, name, where);
    }

    std::vector<weighted_edge> read_edges(long long m, int n, weight_bounds weights, detail::value_name name,
                                          detail::site where = detail::site::here()) {
        return edge_lines<weighted_edge>(m, detail::stated::yes, n, weights, name, where);
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
        case_runs_++;
        bool const marking = case_runs_ == 1 && (describing_ || wanted_case_.has_value());
        if (case_runs_ == 1) cases_counted_ = count;
        for (long long index = 1; index <= count; index++) {
            detail::current_case() = index;
            long long const began = from_.position();
            body();
            if (marking) case_marks_.emplace_back(began, from_.position());
        }
        detail::current_case() = 0;
    }

    template <class... Args>
    void require(bool condition, detail::pattern_for<Args...> message, Args const&... args) {
        if (!condition) invalid(detail::value_name(unnamed), fmt(message, args...));
    }

    void require(check_result const& outcome, detail::value_name name) {
        if (outcome) return;
        invalid(detail::value_name(unnamed),
                name.known() ? fmt("{}: {}", name.text(), outcome.message()) : outcome.message());
    }

    void feature(std::string name) { features_.emplace(std::move(name), false); }

    void features(std::initializer_list<char const*> names) {
        for (char const* one : names) feature(one);
    }

    void saw(std::string const& name, detail::site where = detail::site::here()) {
        auto const found = features_.find(name);
        if (found == features_.end()) never_declared(name, where);
        found->second = true;
    }

    void stat(std::string const& name, long long value, detail::site where = detail::site::here()) {
        for (std::pair<std::string, long long>& one : stats_)
            if (one.first == name) {
                one.second = (std::max)(one.second, value);
                return;
            }
        if (!stat_name(name)) not_a_stat_name(name, where);
        stats_.push_back({name, value});
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
        if (wanted_case_.has_value()) write_the_case(*wanted_case_);
        detail::diagnostics::shared().emit();
    }

private:
    template <class Limits>
    friend class subtask_table;


    [[noreturn]] EOLYMP_COLD static void not_a_stat_name(std::string const& name, detail::site where) {
        detail::library_error(fmt("{}: stat(\"{}\") is not a stat's name; name it with one word of letters, digits, "
                                  "_, . or -, as depth or max_degree",
                                  detail::where_of(where), detail::escaped(name)));
    }

    [[noreturn]] EOLYMP_COLD void never_declared(std::string const& name, detail::site where) const {
        std::string const declared = detail::joined(features_, [](auto const& one) { return one.first; });
        detail::library_error(fmt("{}: saw(\"{}\") names a feature that was never declared; it declares {}",
                                  detail::where_of(where), name, declared.empty() ? "none" : declared));
    }

    static bool stat_name(std::string const& name) {
        bool plain = !name.empty();
        for (char const one : name)
            plain = plain && ((one >= 'a' && one <= 'z') || (one >= 'A' && one <= 'Z') || (one >= '0' && one <= '9') ||
                              one == '_' || one == '.' || one == '-');
        return plain;
    }

    static void keep_the_larger(std::vector<std::pair<std::string, long long>>& into, std::string const& name,
                                long long value) {
        for (std::pair<std::string, long long>& one : into)
            if (one.first == name) {
                one.second = (std::max)(one.second, value);
                return;
            }
        into.push_back({name, value});
    }

    EOLYMP_COLD std::vector<std::pair<std::string, long long>> every_stat() const {
        std::vector<std::pair<std::string, long long>> all;
        for (auto const& one : from_.bounds()) {
            detail::seen_bounds const& seen = one.second;
            bool const whole = seen.kind == "int";
            if (seen.element || seen.reads != 1 || (!whole && seen.kind != "length") || !stat_name(one.first))
                continue;
            all.push_back({whole ? one.first : one.first + ".length", whole ? seen.last_whole : seen.last_length});
        }
        for (auto const& one : stats_) keep_the_larger(all, one.first, one.second);
        return all;
    }

    static void exited_early() {
        validator* const one = detail::live_validator();
        if (one != nullptr) one->complete();
    }

    void set_case(std::string const& text) {
        detail::integer_read const parsed = detail::parse_integer(text);
        if (parsed.problem != detail::number_problem::none)
            detail::library_error(fmt("--eo-case {} is not a case number", text));
        if (wanted_case_.has_value())
            detail::library_error(fmt("--eo-case is given twice, as {} and {}; ask for one case", *wanted_case_,
                                      parsed.value));
        wanted_case_ = parsed.value;
    }

    static std::vector<std::pair<std::size_t, std::size_t>> integers_of(std::string const& text, long long value) {
        std::vector<std::pair<std::size_t, std::size_t>> found;
        std::size_t start = 0;
        while (true) {
            while (start < text.size() && detail::is_blank(text[start])) start++;
            if (start == text.size()) return found;
            std::size_t end = start;
            while (end < text.size() && !detail::is_blank(text[end])) end++;
            detail::integer_read const read = detail::parse_integer(std::string_view(text).substr(start, end - start));
            if (read.problem == detail::number_problem::none && read.value == value) found.emplace_back(start, end);
            start = end;
        }
    }

    void write_the_case(long long wanted) {
        if (case_runs_ == 0) detail::library_error("--eo-case needs a validator that reads the cases with v.cases");
        if (case_runs_ > 1)
            detail::library_error(
                fmt("--eo-case needs one run of v.cases, and this validator ran it {} times", case_runs_));
        if (wanted < 1 || wanted > cases_counted_)
            detail::library_error(fmt("--eo-case={}, but the test has {} cases", wanted, cases_counted_));
        int const descriptor = path_.empty() ? 0 : detail::open_to_read(path_.c_str());
        std::pair<long long, long long> const chosen = case_marks_[static_cast<std::size_t>(wanted - 1)];
        std::string out;
        bool const read = detail::read_range(descriptor, 0, case_marks_.front().first, out) &&
                          detail::read_range(descriptor, chosen.first, chosen.second, out) &&
                          detail::read_range(descriptor, case_marks_.back().second, from_.position(), out);
        if (!path_.empty()) detail::close_descriptor(descriptor);
        if (!read)
            detail::library_error("--eo-case needs the test in a file, and standard input is not one; give the "
                                  "file's path");
        std::size_t const header = static_cast<std::size_t>(case_marks_.front().first);
        std::vector<std::pair<std::size_t, std::size_t>> const counts = integers_of(out.substr(0, header), cases_counted_);
        if (counts.size() != 1)
            detail::library_error(fmt("--eo-case needs the count given to v.cases, {}, to be the one integer of that "
                                      "value before the first case, so that it can be written as 1; {}",
                                      cases_counted_,
                                      counts.empty() ? std::string("there is none")
                                                     : fmt("there are {}, and it cannot tell which", counts.size())));
        out.replace(counts[0].first, counts[0].second - counts[0].first, "1");
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }

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
        if (description != nullptr) from_.refuse_a_number_that_goes_on(here);
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

    template <class T, class Read>
    std::vector<T> many(long long count, detail::value_name const& name, Read read_one) {
        bool first = true;
        return from_.many<T>(count, name, [&](detail::value_name const& each) {
            if (!first) read_space();
            first = false;
            return read_one(each);
        });
    }

    template <class Edge>
    std::vector<Edge> edge_lines(long long count, detail::stated ends, int n,
                                 [[maybe_unused]] weight_bounds weights, detail::value_name const& name,
                                 detail::site where) {
        std::vector<Edge> edges;
        edges.reserve(ends == detail::stated::yes ? from_.room_for(count, name)
                                                  : static_cast<std::size_t>((std::max)(count, 0LL)));
        detail::value_name const weight = name.field(".w");
        for (long long index = 1; index <= count; index++) {
            int const u = from_.whole_int(1, n, ends, name.lent_at(index), where);
            read_space();
            int const v = from_.whole_int(1, n, ends, name.lent_at(index), where);
            if constexpr (std::is_same_v<Edge, weighted_edge>) {
                read_space();
                long long const w =
                    from_.whole_long(weights.low, weights.high, detail::stated::yes, weight.lent_at(index), where);
                edges.push_back(Edge{u, v, w});
            } else {
                edges.push_back(Edge{u, v});
            }
            read_eoln();
        }
        return edges;
    }

    void check_the_end() {
        if (from_.peek() < 0) return;
        from_.refuse_a_number_that_goes_on(from_.peek());
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

    EOLYMP_COLD void describe() {
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
        for (auto const& one : every_stat()) out += fmt("eo-describe stat {} {}\n", one.first, one.second);
        for (std::size_t at = 0; at < case_marks_.size(); at++)
            out += fmt("eo-describe case {} {} {}\n", at + 1, case_marks_[at].first, case_marks_[at].second);
        detail::report(out.empty() ? std::string() : out.substr(0, out.size() - 1));
    }

    detail::reader from_;
    std::string path_;
    std::optional<long long> wanted_case_;
    long long case_runs_ = 0;
    long long cases_counted_ = 0;
    std::vector<std::pair<long long, long long>> case_marks_;
    std::optional<int> group_;
    std::map<std::string, bool> features_;
    std::vector<std::pair<std::string, long long>> stats_;
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
                if (rows_[at].group != 0 && rows_[other].group != 0 && rows_[at].limits == rows_[other].limits)
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
    std::string const listed =
        detail::joined(rows_, [](subtask_row<Limits> const& row) { return std::to_string(row.group); });
    detail::finish(3, fmt("{}: no subtask {}; known: {}", detail::where_of(where_), *chosen, listed));
}

}  // namespace eo
