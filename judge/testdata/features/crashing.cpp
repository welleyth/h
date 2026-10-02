#include "eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(2, 1000, "n");
    v.read_eoln();
    v.read_tree(n, "edge");
    if (n > 100) v.saw("caterpillar");
}
