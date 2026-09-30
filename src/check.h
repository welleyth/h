#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "read.h"
#include "stream.h"
#include "role.h"
#include "structure.h"
#include "summary.h"

namespace eo {

struct ignore_t {};
struct lenient_t {};
struct plain_t {};
struct any_case_t {};
struct exact_t {};
struct any_order_t {};
struct big_t {};
struct absolute_t {};

inline constexpr ignore_t ignore{};
inline constexpr lenient_t lenient{};
inline constexpr plain_t plain{};
inline constexpr any_case_t any_case{};
inline constexpr exact_t exact{};
inline constexpr any_order_t any_order{};
inline constexpr big_t big{};
inline constexpr absolute_t absolute{};

enum class answers_are { unique, many };

inline constexpr answers_are unique = answers_are::unique;
inline constexpr answers_are many = answers_are::many;

enum class towards { smaller, larger };

inline constexpr towards minimize = towards::smaller;
inline constexpr towards maximize = towards::larger;

struct tolerance {
    double epsilon;
};

inline tolerance within(double epsilon) {
    if (!(epsilon >= 0)) detail::library_error(fmt("eo::within needs a tolerance of 0 or more, not {}", epsilon));
    return tolerance{epsilon};
}

enum class standing { worse, equal, better };

namespace detail {

inline void the_jury_has_a_number(double value) {
    if (std::isnan(value)) judging().fail_jury(fmt("the jury's value is {}, which no answer can equal", value));
}

template <class T>
inline void compared_exactly([[maybe_unused]] char const* call, [[maybe_unused]] site where) {
    if constexpr (std::is_floating_point_v<T>)
        warn("EO214", fmt("{} compares two reals with ==, so a correct answer that rounding moved is not equal", call),
             "say how close is equal with eo::within(eps) after the direction", where);
}

}  // namespace detail

template <class T>
inline standing compare(T const& found, T const& by_the_jury, towards direction,
                        detail::site where = detail::site::here()) {
    detail::compared_exactly<T>("eo::compare", where);
    if constexpr (std::is_floating_point_v<T>) detail::the_jury_has_a_number(static_cast<double>(by_the_jury));
    if (found == by_the_jury) return standing::equal;
    bool const better = direction == towards::smaller ? found < by_the_jury : found > by_the_jury;
    return better ? standing::better : standing::worse;
}

inline standing compare(double found, double by_the_jury, towards direction, tolerance allowed) {
    detail::the_jury_has_a_number(by_the_jury);
    if (found == by_the_jury || close_enough(by_the_jury, found, allowed.epsilon)) return standing::equal;
    bool const better = direction == towards::smaller ? found < by_the_jury : found > by_the_jury;
    return better ? standing::better : standing::worse;
}

class checker;

namespace detail {

inline checker*& live_checker() {
    static checker* only = nullptr;
    return only;
}

inline std::FILE* opened_scratch(int descriptor) {
    if (descriptor < 0) return nullptr;
    std::FILE* const file = ::fdopen(descriptor, "w+b");
    if (file == nullptr) ::close(descriptor);
    return file;
}

inline std::FILE* scratch_in_memory() {
#if defined(__linux__) && defined(SYS_memfd_create)
    return opened_scratch(static_cast<int>(::syscall(SYS_memfd_create, "eolymp-checker-output", 0)));
#else
    return nullptr;
#endif
}

inline std::FILE* scratch_in_the_temporary_directory() { return std::tmpfile(); }

inline std::FILE* scratch_in_the_workspace() {
    char name[] = "eolymp-checker-output-XXXXXX";
    int const descriptor = ::mkstemp(name);
    if (descriptor >= 0) ::unlink(name);
    return opened_scratch(descriptor);
}

using scratch_maker = std::FILE* (*)();

inline std::array<scratch_maker, 3> scratch_makers() {
    return {&scratch_in_the_temporary_directory, &scratch_in_memory, &scratch_in_the_workspace};
}

template <std::size_t Count>
inline std::FILE* first_scratch(std::array<scratch_maker, Count> const& makers) {
    for (scratch_maker const make : makers)
        if (std::FILE* const made = make()) return made;
    return nullptr;
}

inline std::FILE* scratch_file() { return first_scratch(scratch_makers()); }

inline constexpr int deadly_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL};

inline char const* how_it_died(int caught) {
    if (caught == SIGSEGV) return "SIGSEGV: it read or wrote memory it does not own, or ran out of stack";
    if (caught == SIGABRT) return "SIGABRT: it aborted, as a failed assert does";
    if (caught == SIGFPE) return "SIGFPE: an arithmetic error, such as an integer division by zero";
    if (caught == SIGBUS) return "SIGBUS: a memory access the machine refused";
    return "SIGILL: an illegal instruction, which the end of a function that returns no value can reach";
}

inline void (*&on_a_deadly_signal())(int) {
    static void (*hook)(int) = nullptr;
    return hook;
}

inline void deadly_signal(int caught) {
    if (on_a_deadly_signal() != nullptr) on_a_deadly_signal()(caught);
#ifdef EOLYMP_TESTING
    throw stop{128 + caught, how_it_died(caught)};
#else
    ::raise(caught);
#endif
}

inline void write_all(int descriptor, char const* bytes, std::size_t size) {
    while (size > 0) {
        ssize_t const wrote = ::write(descriptor, bytes, size);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) return;
        bytes += wrote;
        size -= static_cast<std::size_t>(wrote);
    }
}

inline void stand_on_a_spare_stack() {
    static char spare[1 << 16];
    stack_t current{};
    if (::sigaltstack(nullptr, &current) != 0 || (current.ss_flags & SS_DISABLE) == 0) return;
    stack_t mine{};
    mine.ss_sp = spare;
    mine.ss_size = sizeof(spare);
    ::sigaltstack(&mine, nullptr);
}

class reader;

}  // namespace detail

class stream {
public:
    stream() = default;

    stream(detail::source from, detail::fault whose, std::string label)
        : reader_(std::move(from), whose, std::move(label), true, "EO103") {
        reader_.exponents(true);
    }

    int read_int(long long low, long long high, detail::value_name name,
                 detail::site where = detail::site::here()) {
        return reader_.whole_int(low, high, detail::stated::yes, name, where);
    }

    int read_int(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_int(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO103: bound this value, or say read_int(eo::any, name)")]] int
    read_int(detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_int(0, 0, detail::stated::absent, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_int(low, high, eo::unnamed)")]] int
    read_int(long long low, long long high, detail::site where = detail::site::here()) {
        return reader_.whole_int(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    long long read_long(long long low, long long high, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return reader_.whole_long(low, high, detail::stated::yes, name, where);
    }

    long long read_long(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_long(0, 0, detail::stated::deliberate, name, where);
    }

    double read_real(double low, double high, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return reader_.fractional(low, high, detail::stated::yes, 0, 0, false, name, where);
    }

    [[deprecated("eolymp EO103: bound this value, or say read_real(eo::any, name)")]] double
    read_real(detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.fractional(0, 0, detail::stated::absent, 0, 0, false, name, where);
    }

    double read_real(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.fractional(0, 0, detail::stated::deliberate, 0, 0, false, name, where);
    }

    std::string read_token(long long least, long long most, charset allowed, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return reader_.word(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_token(long long least, long long most, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return reader_.word(least, most, nullptr, detail::stated::yes, name, where);
    }

    std::string read_token(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.word(0, 0, nullptr, detail::stated::deliberate, name, where);
    }

    std::string read_token(pattern const& told, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.matching(told, name, where);
    }

    std::string read_line(pattern const& told, detail::value_name name) { return reader_.line_matching(told, name); }

    std::string read_line(long long least, long long most, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return reader_.rest_of_line(least, most, nullptr, detail::stated::yes, name, where);
    }

    std::string read_line(long long least, long long most, charset allowed, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return reader_.rest_of_line(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_line(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.rest_of_line(0, 0, nullptr, detail::stated::deliberate, name, where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, detail::value_name name,
                            detail::site where = detail::site::here()) {
        return reader_.choice(choices, false, name, where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, any_case_t, detail::value_name name,
                            detail::site where = detail::site::here()) {
        return reader_.choice(choices, true, name, where);
    }

    std::vector<int> read_ints(long long count, long long low, long long high, detail::value_name name,
                               detail::site where = detail::site::here()) {
        return reader_.many<int>(count, name, [&](detail::value_name const& each) {
            return reader_.whole_int(low, high, detail::stated::yes, each, where);
        });
    }

    std::vector<long long> read_longs(long long count, long long low, long long high, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return reader_.many<long long>(count, name, [&](detail::value_name const& each) {
            return reader_.whole_long(low, high, detail::stated::yes, each, where);
        });
    }

    std::vector<long long> read_longs(long long count, any_t, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        return reader_.many<long long>(count, name, [&](detail::value_name const& each) {
            return reader_.whole_long(0, 0, detail::stated::deliberate, each, where);
        });
    }

    std::vector<double> read_reals(long long count, double low, double high, detail::value_name name,
                                   detail::site where = detail::site::here()) {
        return reader_.many<double>(count, name, [&](detail::value_name const& each) {
            return reader_.fractional(low, high, detail::stated::yes, 0, 0, false, each, where);
        });
    }

    std::vector<std::string> read_tokens(long long count, long long least, long long most, charset allowed,
                                         detail::value_name name,
                                         detail::site where = detail::site::here()) {
        return reader_.many<std::string>(count, name, [&](detail::value_name const& each) {
            return reader_.word(least, most, &allowed, detail::stated::yes, each, where);
        });
    }

    std::vector<std::string> read_tokens(long long count, pattern const& told, detail::value_name name,
                                         detail::site where = detail::site::here()) {
        return reader_.many<std::string>(
            count, name, [&](detail::value_name const& each) { return reader_.matching(told, each, where); });
    }

    std::vector<std::string> read_grid(long long rows, long long cols, charset allowed, detail::value_name name,
                                       detail::site where = detail::site::here()) {
        if (cols < 1)
            detail::library_error(fmt("{}: a grid on a stream has rows of at least one character, not {}",
                                      detail::where_of(where), cols));
        return read_tokens(rows, cols, cols, std::move(allowed), std::move(name), where);
    }

    std::vector<edge> read_edges(long long m, int n, detail::value_name name,
                                 detail::site where = detail::site::here()) {
        return edges_between<edge>(m, detail::stated::yes, n, {0, 0}, name, where);
    }

    std::vector<weighted_edge> read_edges(long long m, int n, weight_bounds weights, detail::value_name name,
                                          detail::site where = detail::site::here()) {
        return edges_between<weighted_edge>(m, detail::stated::yes, n, weights, name, where);
    }

    std::vector<edge> read_tree(int n, detail::value_name name, detail::site where = detail::site::here()) {
        a_tree_has_a_vertex(n, where);
        std::vector<edge> edges = edges_between<edge>(n - 1, detail::stated::deliberate, n, {0, 0}, name, where);
        holds(is_tree(n, edges), name);
        return edges;
    }

    std::vector<weighted_edge> read_tree(int n, weight_bounds weights, detail::value_name name,
                                         detail::site where = detail::site::here()) {
        a_tree_has_a_vertex(n, where);
        std::vector<weighted_edge> edges =
            edges_between<weighted_edge>(n - 1, detail::stated::deliberate, n, weights, name, where);
        holds(is_tree(n, detail::endpoints(edges)), name);
        return edges;
    }

    std::vector<edge> read_graph(int n, int m, graph_shape shape, detail::value_name name,
                                 detail::site where = detail::site::here()) {
        std::vector<edge> edges = edges_between<edge>(m, detail::stated::deliberate, n, {0, 0}, name, where);
        holds(detail::shaped(n, edges, shape), name);
        return edges;
    }

    std::vector<weighted_edge> read_graph(int n, int m, graph_shape shape, weight_bounds weights,
                                          detail::value_name name, detail::site where = detail::site::here()) {
        std::vector<weighted_edge> edges =
            edges_between<weighted_edge>(m, detail::stated::deliberate, n, weights, name, where);
        holds(detail::shaped(n, detail::endpoints(edges), shape), name);
        return edges;
    }

    bool at_eof() { return reader_.at_end(); }
    bool at_eoln() { return reader_.at_line_end(); }

    [[noreturn]] void wrong(std::string const& message) const {
        reader_.refuse(detail::value_name(unnamed), message);
    }

    template <class... Args>
    [[noreturn]] void wrong(detail::pattern_for<Args...> pattern, Args const&... args) const {
        reader_.refuse(detail::value_name(unnamed), fmt(pattern, args...));
    }

    void numbers(lenient_t) { reader_.relaxed(true); }
    void reals(plain_t) { reader_.exponents(false); }
    void trailing(ignore_t) { trailing_matters_ = false; }

    void skip_rest(std::string reason) {
        if (reason.empty()) detail::library_error("skip_rest needs a reason");
        skipped_ = true;
    }

private:
    friend class checker;
    friend class interactor;
    friend class controller;
    friend class channel;

    detail::reader& inside() { return reader_; }
    bool trailing_matters() const { return trailing_matters_; }
    bool skipped() const { return skipped_; }


    template <class Edge>
    std::vector<Edge> edges_between(long long m, detail::stated ends, int n,
                                    [[maybe_unused]] weight_bounds weights,
                                    detail::value_name const& name, detail::site where) {
        std::vector<Edge> edges;
        edges.reserve(reader_.room_for(m, name));
        detail::value_name const weight = name.field(".w");
        for (long long at = 1; at <= m; at++) {
            int const u = reader_.whole_int(1, n, ends, name.lent_at(at), where);
            int const v = reader_.whole_int(1, n, ends, name.lent_at(at), where);
            if constexpr (std::is_same_v<Edge, weighted_edge>)
                edges.push_back(Edge{u, v,
                                     reader_.whole_long(weights.low, weights.high, detail::stated::yes,
                                                        weight.lent_at(at), where)});
            else
                edges.push_back(Edge{u, v});
        }
        return edges;
    }

    static void a_tree_has_a_vertex(int n, detail::site where) {
        if (n < 1)
            detail::library_error(
                fmt("{}: read_tree needs a tree of at least one vertex, not {}", detail::where_of(where), n));
    }

    void holds(check_result const& outcome, detail::value_name const& name) const {
        if (outcome) return;
        reader_.refuse(detail::value_name(unnamed),
                       name.known() ? fmt("{}: {}", name.text(), outcome.message()) : outcome.message());
    }

    detail::reader reader_;
    bool trailing_matters_ = true;
    bool skipped_ = false;
};

using input_stream = stream;
using answer_stream = stream;

}  // namespace eo

namespace eo {

class checker final : public detail::scorer {
public:
    checker(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_checker() != nullptr)
            detail::library_error(fmt("{}: this program already has a checker", detail::where_of(where)));
        detail::diagnostics::shared().start_the_clock("EO209", "checker", 10000, where);
        std::array<char const*, 3> const given = detail::test_paths(argc, argv);
        char const* const kinds[3] = {"input", "output", "answer"};
        std::string paths[3];
        for (int at = 0; at < 3; at++) {
            if (given[static_cast<std::size_t>(at)] == nullptr)
                detail::library_error(
                    fmt("{}: the checker was given no {} file", detail::where_of(where), kinds[at]));
            paths[at] = given[static_cast<std::size_t>(at)];
        }
        if (argc >= 4 && detail::environment("OUTPUT_FILE") != nullptr && paths[1] != argv[2])
            detail::note("EO211", "this checker runs as the legacy type, which swaps its last two arguments",
                         "the ordinary PROGRAM type is the norm", where);
        input = stream(detail::source::over_file(paths[0].c_str(), true), detail::fault::jury_error,
                       "input.txt");
        output = stream(detail::source::over_file(paths[1].c_str(), false), detail::fault::wrong_answer,
                        "output.txt");
        jury = stream(detail::source::over_file(paths[2].c_str(), true), detail::fault::jury_error,
                      "answer.txt");
        if (detail::on_judge()) {
            saved_out_ = ::dup(1);
            saved_err_ = ::dup(2);
            held_ = detail::scratch_file();
            if (held_ == nullptr)
                detail::library_error("the checker cannot open a scratch file for its own output: not in memory, "
                                      "not in the temporary directory and not in the workspace");
            ::dup2(::fileno(held_), 1);
            ::dup2(::fileno(held_), 2);
            detail::emitter() = &checker::write_log;
            catch_deadly_signals();
        }
        detail::live_checker() = this;
        detail::live_scorer() = this;
        detail::close_on_exit(&checker::exited_early);
        end_on_terminate();
    }

    checker(checker const&) = delete;
    checker& operator=(checker const&) = delete;

    ~checker() noexcept(false) {
        detail::on_a_deadly_signal() = nullptr;
        detail::unfinished() = nullptr;
        detail::live_checker() = nullptr;
        detail::live_scorer() = nullptr;
        detail::blaming() = nullptr;
        detail::current_case() = 0;
        detail::emitter() = nullptr;
        if (held_ != nullptr) {
            put_the_output_back();
            let_go_of_what_was_held();
        }
        fail_closed("checker");
    }

    stream input;
    stream output;
    stream jury;

    int group() const { return static_cast<int>(detail::environment_integer("TEST_GROUP")); }
    int index() const { return static_cast<int>(detail::environment_integer("TEST_INDEX")); }

    std::string test_id() const {
        char const* const set = detail::environment("TEST_ID");
        return set == nullptr ? std::string() : std::string(set);
    }

    void answers(answers_are how) { declared_ = how; }

    template <class Body>
    void cases(long long count, Body&& body) {
        for (long long at = 1; at <= count; at++) {
            detail::current_case() = at;
            body();
        }
        detail::current_case() = 0;
    }

    template <class Read>
    auto read_both(Read read_one) {
        auto expected = read_with(jury, read_one);
        auto found = read_with(output, read_one);
        return std::make_pair(std::move(expected), std::move(found));
    }

    template <class T>
    [[noreturn]] void optimum(T const& by_the_jury, T const& found, towards direction,
                              detail::site where = detail::site::here()) {
        detail::compared_exactly<T>("c.optimum", where);
        if (found == by_the_jury) pass(1, fmt("{}", found));
        bool const better = direction == towards::smaller ? found < by_the_jury : found > by_the_jury;
        if (better)
            fail_jury(fmt("the contestant's {} beats the jury's {}", found, by_the_jury));
        fail_run(fmt("the answer is {}; the optimum is {}", found, by_the_jury));
    }

    [[noreturn]] void optimum(double by_the_jury, double found, towards direction, tolerance allowed) {
        standing const said = compare(found, by_the_jury, direction, allowed);
        if (said == standing::equal) pass(1, fmt("{}", found));
        if (said == standing::better)
            fail_jury(fmt("the contestant's {} beats the jury's {}", found, by_the_jury));
        fail_run(fmt("the answer is {}; the optimum is {}", found, by_the_jury));
    }

    [[noreturn]] void tokens() { token_by_token(false); }

    [[noreturn]] void tokens(any_case_t) { token_by_token(true); }

    [[noreturn]] void tokens(any_order_t) {
        compared_only_ = true;
        std::vector<std::string> wanted;
        std::size_t longest = 0;
        while (!jury.at_eof()) {
            wanted.emplace_back();
            jury_token(wanted.back(), detail::site::here());
            longest = std::max(longest, wanted.back().size());
        }
        std::vector<std::string> found;
        while (!output.at_eof()) {
            if (found.size() == wanted.size())
                fail_run(fmt("the answer has {} tokens, the output has more", wanted.size()));
            found.emplace_back();
            contestant_token(found.back(), longest);
            if (found.back().size() > longest)
                fail_run(fmt("token {} is longer than every token of the answer; it starts \"{}\"", found.size(),
                             detail::shorten(found.back())));
        }
        if (found.size() < wanted.size())
            fail_run(fmt("the output has {} tokens, the answer has {}", found.size(), wanted.size()));
        std::sort(wanted.begin(), wanted.end());
        std::sort(found.begin(), found.end());
        auto const differ = std::mismatch(wanted.begin(), wanted.end(), found.begin());
        if (differ.first != wanted.end()) {
            std::string const& token = std::min(*differ.first, *differ.second);
            fail_run(fmt("\"{}\" is in the answer {} and in the output {}", detail::shorten(token),
                         times_in(wanted, token), times_in(found, token)));
        }
        pass(1, fmt("{} tokens in any order", wanted.size()));
    }

    [[noreturn]] void integers() {
        compared_only_ = true;
        long long seen = 0;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} integers", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} integers, the output has more", seen - 1));
            long long const want = jury.read_long(any, unnamed);
            if (output_done) fail_run(fmt("the output ended after {} integers, the answer has more", seen - 1));
            long long const got = output.read_long(any, unnamed);
            if (got != want) fail_run(fmt("integer {} is {}, expected {}", seen, got, want));
        }
    }

    [[noreturn]] void integers(big_t) {
        compared_only_ = true;
        long long seen = 0;
        std::string want;
        std::string got;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} integers", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} integers, the output has more", seen - 1));
            jury_token(want, detail::site::here());
            spelled_as_an_integer(jury, want);
            if (output_done) fail_run(fmt("the output ended after {} integers, the answer has more", seen - 1));
            std::size_t const longest = std::max<std::size_t>(want.size(), detail::reader::longest_number);
            contestant_token(got, longest);
            if (got.size() > longest)
                fail_run(fmt("token {} is longer than {} characters: \"{}\"", seen, longest, detail::shorten(got)));
            spelled_as_an_integer(output, got);
            if (want == got) continue;
            if (want.size() <= 40 && got.size() <= 40) fail_run(fmt("integer {} is {}, expected {}", seen, got, want));
            std::size_t same = 0;
            while (want[same] == got[same]) same++;
            fail_run(fmt("integer {} is {} characters long and the answer's {}; they differ first at character {}",
                         seen, got.size(), want.size(), same + 1));
        }
    }

    [[noreturn]] void reals(double epsilon) { value_by_value(epsilon, true); }

    [[noreturn]] void reals(double epsilon, absolute_t) { value_by_value(epsilon, false); }

    [[noreturn]] void lines() {
        compared_only_ = true;
        long long seen = 0;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} lines", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} lines, the output has more", seen - 1));
            std::string want = jury.read_line(any, unnamed);
            if (output_done) fail_run(fmt("the output ended after {} lines, the answer has more", seen - 1));
            bool longer = false;
            std::string got = output.inside().line_up_to(want.size() + 1, longer, unnamed);
            while (!want.empty() && trailing_blank(want.back())) want.pop_back();
            if (longer)
                fail_run(fmt("line {} is longer than the expected \"{}\"; it starts \"{}\"", seen,
                             detail::shorten(want), detail::shorten(got)));
            while (!got.empty() && trailing_blank(got.back())) got.pop_back();
            if (want != got) fail_run(fmt("line {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                                          detail::shorten(want)));
        }
    }

    [[noreturn]] void lines(exact_t) {
        compared_only_ = true;
        long long seen = 0;
        long long last = 0;
        while (true) {
            bool const jury_done = jury.inside().peek() < 0;
            bool const output_done = output.inside().peek() < 0;
            if (jury_done && output_done) pass(1, fmt("{} lines", last));
            seen++;
            std::string want = jury_done ? std::string() : jury.read_line(any, unnamed);
            bool longer = false;
            std::string got =
                output_done ? std::string() : output.inside().line_up_to(want.size() + 1, longer, unnamed);
            want.erase(want.find_last_not_of(" \t\r") + 1);
            got.erase(got.find_last_not_of(" \t\r") + 1);
            if (jury_done && (longer || !got.empty()))
                fail_run(fmt("the answer has {} lines, the output has more", last));
            if (output_done && !want.empty())
                fail_run(fmt("the output ended after {} lines, the answer has more", last));
            if (longer)
                fail_run(fmt("line {} is longer than the expected \"{}\"; it starts \"{}\"", seen,
                             detail::shorten(want), detail::shorten(got)));
            if (want != got) fail_run(fmt("line {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                                          detail::shorten(want)));
            if (!want.empty()) last = seen;
        }
    }

    [[noreturn]] void from_interactor(detail::site where = detail::site::here()) {
        from_interactor([](summary const& what) { return what.fraction(); }, where);
    }

    template <class Mapping>
    [[noreturn]] void from_interactor(Mapping mapping, detail::site where = detail::site::here()) {
        stock_ = true;
        output.inside().blame(detail::fault::jury_error);
        if (jury.inside().read_anything() == false) jury.skip_rest("an interactive problem is graded by the interactor");
        summary const said = read_summary(output);
        pass(detail::clamped(mapping(said), where), said.message());
    }

    [[noreturn]] void yes_no() {
        compared_only_ = true;
        long long seen = 0;
        long long yes = 0;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} answers, {} of them YES", seen, yes));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} answers, the output has more", seen - 1));
            std::string const want = jury.read_choice({"YES", "NO"}, any_case, unnamed);
            if (output_done) fail_run(fmt("the output ended after {} answers, the answer has more", seen - 1));
            std::string const got = output.read_choice({"YES", "NO"}, any_case, unnamed);
            if (want != got) fail_run(fmt("answer {} is {}, expected {}", seen, got, want));
            if (got == "YES") yes++;
        }
    }

    template <class Certificate>
    [[noreturn]] void yes_no(Certificate certificate, char const* yes = "YES", char const* no = "NO") {
        std::string const by_the_jury = jury.read_choice({yes, no}, any_case, "verdict");
        std::string const found = output.read_choice({yes, no}, any_case, "verdict");
        if (by_the_jury == no && found == no) pass(1, "both say no");
        if (by_the_jury == yes && found == no) fail_run("a solution exists");
        if (by_the_jury == no && found == yes) {
            {
                detail::blame_guard blamed(&output.inside());
                certificate(output);
            }
            fail_jury("the contestant found a solution the jury says does not exist");
        }
        {
            detail::blame_guard blamed(&jury.inside());
            certificate(jury);
        }
        {
            detail::blame_guard blamed(&output.inside());
            certificate(output);
        }
        pass(1, "a valid certificate");
    }

    [[noreturn]] void pass(double fraction, std::string const& message) final {
        if (std::isnan(fraction)) detail::refuse_a_score(fmt("a score of {}", fraction));
        closing_checks(fraction);
        if (fraction >= 1) deliver(0, "ok", message);
        double const paid = fraction * cost();
        std::string const printed = detail::format_points(paid);
        if (cost() > 0 && std::strtof(detail::with_the_local_point(printed).c_str(), nullptr) >=
                              static_cast<float>(cost()))
            detail::warn("EO206", fmt("'points {}' is below the test's {}, but the judge reads points as a "
                                      "float, which rounds it to the full cost: the run counts as accepted",
                                      printed, cost()),
                         "use eo::ratio(a, b), which is exact, or eo::accept for full marks", detail::site::here());
        if (cost() <= 0)
            detail::warn("EO208", fmt("this test is worth {} points, so the judge counts this score of {} as "
                                      "accepted: points reach a cost of 0", cost(), fraction),
                         "end an answer that earns nothing with eo::wrong, which a sample shows as a wrong answer",
                         detail::site::here());
        deliver(7, "points " + printed, message);
    }

    [[noreturn]] void fail_run(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this wrong answer carries no message", "say what was wrong with it",
                         detail::site::here());
        deliver(1, "wrong answer", message);
    }

    [[noreturn]] void fail_jury(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this jury error carries no message", "say what the jury got wrong",
                         detail::site::here());
        deliver(3, "jury error", message);
    }

    char const* called() const final { return "checker"; }

private:
    static bool trailing_blank(char one) { return one == ' ' || one == '\t' || one == '\r'; }

    [[noreturn]] void value_by_value(double epsilon, bool relative) {
        compared_only_ = true;
        long long seen = 0;
        std::string want;
        std::string got;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} values", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} tokens, the output has more", seen - 1));
            jury_token(want, detail::site::here());
            if (output_done) fail_run(fmt("the output ended after {} tokens, the answer has more", seen - 1));
            std::size_t const longest = std::max<std::size_t>(want.size(), detail::reader::longest_number);
            contestant_token(got, longest);
            if (got.size() > longest)
                fail_run(fmt("token {} is longer than {} characters: \"{}\"", seen, longest, detail::shorten(got)));
            detail::real_read const wanted = detail::parse_real(want, true, true);
            detail::real_read const found = detail::parse_real(got, true, true);
            if (wanted.problem == detail::number_problem::none &&
                found.problem == detail::number_problem::none) {
                bool const near = relative ? close_enough(wanted.value, found.value, epsilon)
                                           : std::fabs(wanted.value - found.value) <= epsilon + 1e-15;
                if (!near)
                    fail_run(fmt("value {} is {}, expected {}", seen, found.value, wanted.value));
                continue;
            }
            if (want != got)
                fail_run(fmt("token {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                             detail::shorten(want)));
        }
    }

    static void spelled_as_an_integer(stream& side, std::string& token) {
        detail::number_problem const problem = detail::integer_spelling(token, side.inside().relaxed());
        if (problem != detail::number_problem::none)
            side.wrong("expected an integer, found \"{}\": {}", detail::shorten(token), detail::describe(problem));
        detail::canonical_integer(token);
    }

    static std::string times_in(std::vector<std::string> const& sorted, std::string const& token) {
        auto const range = std::equal_range(sorted.begin(), sorted.end(), token);
        std::size_t const count = static_cast<std::size_t>(range.second - range.first);
        return count == 1 ? std::string("once") : fmt("{} times", count);
    }

    [[noreturn]] void token_by_token(bool fold) {
        compared_only_ = true;
        long long seen = 0;
        std::string want;
        std::string got;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} tokens", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} tokens, the output has more", seen - 1));
            jury_token(want, detail::site::here());
            if (output_done) fail_run(fmt("the output ended after {} tokens, the answer has more", seen - 1));
            contestant_token(got, want.size());
            if (got.size() > want.size())
                fail_run(fmt("token {} is longer than the expected \"{}\"; it starts \"{}\"", seen,
                             detail::shorten(want), detail::shorten(got)));
            if (fold ? !detail::same_in_any_case(want, got) : want != got)
                fail_run(fmt("token {} is \"{}\", expected \"{}\"{}", seen, detail::shorten(got),
                             detail::shorten(want), fold ? " in any case" : ""));
        }
    }

    void jury_token(std::string& want, detail::site where) {
        jury.inside().word_into(want, 0, 0, nullptr, detail::stated::deliberate, unnamed, where);
    }

    void contestant_token(std::string& got, std::size_t longest) {
        output.inside().take_word_into(got, unnamed, detail::site::here(), "a token",
                                       static_cast<long long>(longest) + 1);
    }

    static void exited_early() {
        checker* const one = detail::live_checker();
        if (one != nullptr && !one->delivered_) one->fail_jury(
            "the checker ended without a verdict: exit() was called, or the checker was never destroyed");
    }

    static void write_log(std::string const& verdict) {
        checker* const one = detail::live_checker();
        if (one == nullptr) return;
        one->unwrap(verdict);
    }

    template <class Read>
    auto read_with(stream& which, Read& read_one) {
        detail::blame_guard blamed(&which.inside());
        return read_one(which);
    }

    summary read_summary(stream& said) {
        summary out;
        std::string const marker = said.read_token(any, "marker");
        if (marker == "eolymp-phase") {
            long long const at = said.read_long(1, 1000, "phase");
            long long const of = said.read_long(1, 1000, "phases");
            said.wrong("the interactor stopped at phase {} of {}; set run_count to {}", at - 1, of, of);
        }
        if (marker != summary::marker())
            said.wrong("this is not an interactor's summary: it starts with \"{}\"",
                       detail::shorten(marker));
        said.read_long(1, 1, "version");
        std::set<std::string> seen;
        while (!said.at_eof()) {
            std::string const field = said.read_token(any, "field");
            if (field != "value" && !seen.insert(field).second)
                said.wrong("the summary has a second {} field", detail::shorten(field));
            if (field == "fraction") {
                out.set_fraction(said.read_real(0.0, 1.0, "fraction"));
            } else if (field == "value") {
                std::string const name = said.read_token(any, "name");
                if (out.has(name)) said.wrong("the summary has a second value called \"{}\"", detail::shorten(name));
                out.record(name, said.read_real(any, "value"));
            } else if (field == "message") {
                std::string text = said.read_line(any, "message");
                if (!text.empty() && text.front() == ' ') text.erase(0, 1);
                out.set_message(std::move(text));
            } else {
                said.wrong("the summary has a field called \"{}\"", detail::shorten(field));
            }
        }
        return out;
    }

    void closing_checks(double fraction) {
        if (fraction <= 0) return;
        if (!output.inside().read_anything())
            detail::warn("EO201", "the checker passed the run without reading any of the output",
                         "read the output, or use a built-in checker", detail::site::here());
        if (!stock_ && !input.inside().read_anything() && !jury.inside().read_anything())
            detail::warn("EO202", "the checker read neither the input nor the answer",
                         "a verdict that cannot depend on the test", detail::site::here());
        if (declared_ == answers_are::many && compared_only_)
            detail::warn("EO212", "the problem declares many answers, but this checker only compares "
                                  "with the jury's",
                         "verify the contestant's answer instead", detail::site::here());
        if (!jury.skipped() && !jury.at_eof())
            detail::warn("EO203", fmt("the answer file still holds \"{}\" when the checker finished",
                                      detail::shorten(jury.inside().rest_of_the_input())),
                         "read it, or say why not: c.jury.skip_rest(\"...\")", detail::site::here());
        if (output.trailing_matters() && !output.at_eof())
            output.wrong(fmt("extra output after the answer: \"{}\"",
                             detail::shorten(output.inside().rest_of_the_input())));
    }

    [[noreturn]] void deliver(int code, std::string head, std::string const& message) {
        delivered_ = true;
        detail::finish(code, message.empty() ? head : head + " " + message);
    }

    void put_the_output_back() {
        let_go_of_the_signals();
        std::fflush(stdout);
        std::fflush(stderr);
        ::dup2(saved_out_, 1);
        ::dup2(saved_err_, 2);
        ::close(saved_out_);
        ::close(saved_err_);
    }

    long long copy_what_was_held() {
        std::rewind(held_);
        char buffer[detail::pipe_size];
        long long copied = 0;
        std::size_t got = 0;
        while ((got = std::fread(buffer, 1, sizeof(buffer), held_)) > 0) {
            std::fwrite(buffer, 1, got, stdout);
            copied += static_cast<long long>(got);
        }
        return copied;
    }

    void let_go_of_the_signals() {
        detail::after_a_log_line() = nullptr;
        detail::on_a_deadly_signal() = nullptr;
        for (std::size_t at = 0; at < before_signals_.size(); at++)
            ::sigaction(detail::deadly_signals[at], &before_signals_[at], nullptr);
    }

    void let_go_of_what_was_held() {
        std::fclose(held_);
        held_ = nullptr;
    }

    void catch_deadly_signals() {
        held_descriptor_ = ::fileno(held_);
        detail::on_a_deadly_signal() = &checker::died;
        detail::after_a_log_line() = &checker::logged;
        detail::stand_on_a_spare_stack();
        struct sigaction deadly {};
        deadly.sa_handler = &detail::deadly_signal;
        deadly.sa_flags = static_cast<int>(SA_RESETHAND | SA_ONSTACK);
        sigemptyset(&deadly.sa_mask);
        for (std::size_t at = 0; at < before_signals_.size(); at++)
            ::sigaction(detail::deadly_signals[at], &deadly, &before_signals_[at]);
    }

    static void logged(std::size_t bytes) {
        checker* const one = detail::live_checker();
        if (one->logged_ >= detail::stored_log) return;
        one->logged_ += bytes;
        std::fflush(stdout);
    }

    static void died(int caught) {
        checker* const one = detail::live_checker();
        one->replay_the_log(caught);
        one->let_go_of_the_signals();
    }

    void replay_the_log(int caught) const {
        char const head[] = "jury error the checker died of ";
        detail::write_all(saved_out_, head, sizeof(head) - 1);
        char const* const how = detail::how_it_died(caught);
        detail::write_all(saved_out_, how, std::strlen(how));
        detail::write_all(saved_out_, "\n", 1);
        char buffer[4096];
        off_t at = 0;
        for (ssize_t got = 0; (got = ::pread(held_descriptor_, buffer, sizeof(buffer), at)) > 0; at += got)
            detail::write_all(saved_out_, buffer, static_cast<std::size_t>(got));
        char const tail[] = "eolymp.h " EOLYMP_H_VERSION "\n";
        detail::write_all(saved_out_, tail, sizeof(tail) - 1);
    }

    void unwrap(std::string const& verdict) {
        detail::emitter() = nullptr;
        bool const holding = held_ != nullptr;
        if (holding) put_the_output_back();
        detail::report(verdict);
        long long const held = holding ? copy_what_was_held() : 0;
        if (holding) let_go_of_what_was_held();
        std::fwrite("eolymp.h ", 1, 9, stdout);
        std::fwrite(EOLYMP_H_VERSION, 1, std::strlen(EOLYMP_H_VERSION), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        if (held > static_cast<long long>(detail::stored_log))
            detail::note("EO210", fmt("the checker printed {} bytes before its verdict", held),
                         "stored logs are truncated", detail::site::here());
    }

    answers_are declared_ = answers_are::unique;
    bool stock_ = false;
    bool compared_only_ = false;
    int saved_out_ = -1;
    int saved_err_ = -1;
    std::FILE* held_ = nullptr;
    int held_descriptor_ = -1;
    std::size_t logged_ = 0;
    std::array<struct sigaction, std::size(detail::deadly_signals)> before_signals_{};
};

}  // namespace eo
