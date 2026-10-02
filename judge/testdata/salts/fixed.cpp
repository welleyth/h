#include "eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 10);
    eo::rng& unused = g.rng("values");
    (void)unused;
    g.out.line(n);
    g.out.line(std::vector<int>(static_cast<std::size_t>(n), 1));
}
