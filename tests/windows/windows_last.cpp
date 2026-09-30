#include "../../eolymp.h"
#include "../../eolymp-shapes.h"

#include <windows.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 9, "n");
    v.read_eoln();
    v.read_eof();
    return GetCurrentProcessId() == 0 ? 1 : n - n;
}
