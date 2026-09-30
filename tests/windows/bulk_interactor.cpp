#include "../../eolymp.h"

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    int const n = it.input.read_int(1, 1000000, "n");
    it.send(n);
    for (int at = 1; at <= n; at++) it.send(at);
    it.flush();
    long long total = 0;
    for (int at = 1; at <= n; at++) total += it.contestant.read_long(0, 2000000, "double");
    long long const expected = 1LL * n * (n + 1);
    if (total != expected) eo::wrong("the doubles add up to {}, expected {}", total, expected);
    eo::accept("{} lines sent before the first answer was read", n);
}
