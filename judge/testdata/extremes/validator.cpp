#include "eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    v.read_int(1, 1000, "n");
    v.read_eoln();
}
