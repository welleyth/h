#define _CRT_SECURE_NO_WARNINGS
#include "../../eolymp.h"

#include <cstdio>

int main(int argc, char** argv) {
    char const* const to = eo::detail::environment("FREOPEN_OUT");
    if (to != nullptr && std::freopen(to, "w", stdout) == nullptr) return 9;
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 100);
    for (int at = 1; at <= n; at++) g.out.line(at, at * at);
}
