#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 100000, "n");
    v.read_eoln();
    v.read_longs(n, 1, 1000000000, "a");
    v.read_eoln();
}
