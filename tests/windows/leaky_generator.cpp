#include "../../eolymp.h"

#include <cstdio>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 100);
    g.out.line(n);
    g.out.flush();
    std::printf("behind its back\n");
}
