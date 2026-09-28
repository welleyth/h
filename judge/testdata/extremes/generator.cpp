#include "eolymp.h"

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 2000);
    g.out.line(n);
}
