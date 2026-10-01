#include "../../eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const t = v.read_int(1, 10, "t");
    v.read_eoln();
    v.cases(t, [&] {
        int const n = v.read_int(1, 5, "n");
        v.read_eoln();
        v.read_ints(n, -9, 9, "a");
        v.read_eoln();
    });
    v.read_eof();
}
