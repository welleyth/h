#define _CRT_SECURE_NO_WARNINGS
#include "../../eolymp.h"

#include <cstdio>

int main(int argc, char** argv) {
    char const* const from = eo::detail::environment("FREOPEN_IN");
    if (from != nullptr && std::freopen(from, "r", stdin) == nullptr) return 9;
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 9, "n");
    v.read_eoln();
    v.read_eof();
    return n - n;
}
