#include "eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv, eo::salt("c41e8a07d95b3f26a1e0b7c4d8f29365"));
    int const n = g.option<int>("n", 1, 10);
    g.out.line(n);
    g.out.line(g.rng("values").ints(n, 1, 9));
}
