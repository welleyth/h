#include "eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 10);
    std::string const fill = g.option<std::string>("fill", {"ones", "random"}, "ones");
    std::vector<long long> values(static_cast<std::size_t>(n), 1);
    if (fill == "random") values = g.rng("values").ints(n, 1, 9);
    g.out.line(n);
    g.out.line(values);
}
