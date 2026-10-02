#include "eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10, "n");
    v.read_eoln();
    v.read_ints(n, 1, 9, "a");
    v.read_eoln();
}
