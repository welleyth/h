#include "eolymp.h"

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    int const want = c.jury.read_int(1, 1000, "n");
    int const said = c.output.read_int(eo::any, "n");
    if (said != want) eo::wrong("the answer is {}, not {}", want, said);
    eo::accept("{}", want);
}
