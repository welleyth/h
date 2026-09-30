#include "../../eolymp.h"
#include "../../eolymp-shapes.h"

#include <string>
#include <vector>

namespace {

struct limits {
    int n;
    bool tree;
    bool operator==(limits const& other) const { return n == other.n && tree == other.tree; }
};

void validate(int argc, char** argv) {
    eo::validator v(argc, argv);
    limits const bounds = v.subtasks<limits>({{1, {10, true}}, {2, {200000, false}}}).without_group({200000, false});
    int const t = v.read_int(1, 10, "t");
    v.read_eoln();
    eo::sum_limit total(200000, "sum of n");
    v.cases(t, [&] {
        int const n = v.read_int(2, bounds.n, "n");
        v.read_space();
        long long const k = v.read_long(1, 1000000000000LL, "k");
        v.read_eoln();
        std::vector<int> const a = v.read_ints(n, -1000000000, 1000000000, "a");
        v.read_eoln();
        v.require(eo::all_distinct(a), "a");
        std::vector<double> const x = v.read_reals(n, -1e9, 1e9, 0, 6, "x");
        v.read_eoln();
        v.read_token(1, n, eo::charset("a-z"), "s");
        v.read_space();
        v.read_choice({"yes", "no"}, "answer");
        v.read_eoln();
        v.read_line(0, 100, eo::charset("a-z "), "line");
        std::vector<eo::edge> const edges = v.read_tree(n, "edge");
        v.read_graph(n, n - 1, eo::simple | eo::connected, "road");
        v.read_permutation(n, "p");
        v.read_eoln();
        total += n;
        v.require(k > 0 && !x.empty() && !edges.empty(), "k");
    });
    v.read_eof();
}

void check(int argc, char** argv) {
    eo::checker c(argc, argv);
    int const mode = c.input.read_int(0, 4, "mode");
    if (mode == 0) c.tokens();
    if (mode == 1) c.reals(1e-6);
    if (mode == 2) c.lines();
    if (mode == 3) {
        long long const n = c.input.read_long(1, 100, "n");
        c.yes_no([&](eo::stream& s) {
            std::vector<long long> const values = s.read_longs(n, eo::any, "x");
            if (values.empty()) s.wrong("no values");
        });
    }
    auto const both = c.read_both([](eo::stream& s) { return s.read_real(0, 1e9, "r"); });
    if (eo::close_enough(both.first, both.second, 1e-6)) eo::accept("r = {}", both.second);
    c.optimum(static_cast<long long>(both.first), static_cast<long long>(both.second), eo::minimize);
}

void interact(int argc, char** argv) {
    eo::interactor it(argc, argv);
    int const n = it.input.read_int(1, 1000000000, "n");
    eo::budget queries(it, 60, "queries");
    it.send(n, eo::fixed(0.5, 3), std::vector<int>{1, 2});
    for (;;) {
        std::string const command = it.contestant.read_choice({"?", "!"}, "command");
        int const x = it.contestant.read_int(1, n, "x");
        if (command == "!") eo::score(eo::ratio(60 - queries.used(), 60), "{} queries", queries.used());
        queries.spend();
        it.send(x < n ? "<" : ">");
    }
}

void control(int argc, char** argv) {
    eo::controller ctl(argc, argv);
    int const n = ctl.input.read_int(1, 10, "n");
    eo::channel& first = ctl.spawn();
    first.send(n);
    long long const said = first.read_long(1, 100, "said");
    eo::accept("{} said {}", first.index(), said);
}

void generate(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 2, 200000);
    std::string const shape = g.option<std::string>("shape", {"random", "path"}, "random");
    eo::graph const made = shape == "path" ? eo::shapes::path(n) : eo::shapes::random_tree(g.rng("shape"), n);
    std::vector<eo::edge> const edges = eo::shapes::presented(g.rng("labels"), made);
    std::vector<long long> const weights = g.rng("weights").distinct(n - 1, 1, 1000000000);
    std::vector<long long> order = g.rng().ints(n, 1, n);
    g.rng().shuffle(order);
    g.out.line(n, g.rng().real(0, 1), eo::fixed(g.rng().real(-1, 1), 6));
    for (std::size_t at = 0; at < edges.size(); at++) g.out.line(edges[at].u, edges[at].v, weights[at]);
    g.out.line(order);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 0;
    std::string const role = argv[1];
    if (role == "validator") validate(argc - 1, argv + 1);
    if (role == "checker") check(argc - 1, argv + 1);
    if (role == "interactor") interact(argc - 1, argv + 1);
    if (role == "controller") control(argc - 1, argv + 1);
    if (role == "generator") generate(argc - 1, argv + 1);
    return 0;
}
