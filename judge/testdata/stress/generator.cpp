#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 100);
    int const most = g.option<int>("max", 1, 1000);
    g.out.line(n);
    g.out.line(g.rng("values").ints(n, 1, most));
}
