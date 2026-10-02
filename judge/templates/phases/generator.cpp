#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv, eo::salt("0123456789abcdef0123456789abcdef"));
    long long x = g.option<long long>("x", -1, 1000000000, -1);
    if (x < 0) x = g.rng("x").uniform(0LL, 1000000000LL);
    g.out.line(x);
}
