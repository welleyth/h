#include "eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv, eo::salt("5be1c09a7d3f42e8b6a0c1d29e7f4b35"));
    int const n = g.option<int>("n", 2, 1000);
    std::string const shape = g.option<std::string>("shape", {"random", "path", "star"});
    std::vector<eo::edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) {
        int const parent = shape == "path" ? vertex - 1 : shape == "star" ? 1 : static_cast<int>(g.rng("tree").uniform(1, vertex - 1));
        edges.push_back({parent, vertex});
    }
    g.out.line(n);
    for (eo::edge const& one : edges) g.out.line(one.u, one.v);
}
