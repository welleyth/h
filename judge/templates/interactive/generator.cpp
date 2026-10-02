#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv, eo::salt("0123456789abcdef0123456789abcdef"));
    int const n = g.option<int>("n", 1, 1000000);
    int secret = g.option<int>("secret", 0, 1000000, 0);
    if (secret == 0 || secret > n) secret = g.rng("secret").uniform(1, n);
    g.out.line(n, secret);
}
