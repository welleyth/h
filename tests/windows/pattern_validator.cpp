#include "../../eolymp.h"

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 5, "n");
    v.read_eoln();
    v.read_tokens(n, eo::pattern("[a-z]{1,3}"), "word");
    v.read_eoln();
    v.read_line(eo::pattern("[A-Z][a-z]*( [A-Z][a-z]*)*"), "title");
    v.read_eof();
}
