#include "eolymp.h"

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.input.skip_rest("only the count matters");
    int const want = c.jury.read_int(1, 1000, "edges");
    int const said = c.output.read_int(eo::any, "edges");
    if (said != want) eo::wrong("the answer is {}, not {}", want, said);
    eo::accept("{}", want);
}
