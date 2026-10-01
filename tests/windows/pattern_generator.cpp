#include "../../eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 1000);
    for (int at = 0; at < n; at++) g.out.line(g.rng("names").pattern("[A-Z][a-z]{2,7}( [a-z]{1,3}|-[0-9]{2})?"));
}
