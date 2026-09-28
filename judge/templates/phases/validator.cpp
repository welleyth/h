#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    v.read_long(0, 1000000000, "x");
    v.read_eoln();
}
