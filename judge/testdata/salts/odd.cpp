#include <cstdio>
#include <cstring>

#include "eolymp.h"

int main(int argc, char** argv) {
    for (int at = 1; at < argc; at++)
        if (std::strcmp(argv[at], "-n=1") == 0) {
            std::puts("1");
            return 0;
        }
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 2, 10);
    g.out.line(n);
    g.out.line(g.rng("values").ints(n, 1, 9));
}
