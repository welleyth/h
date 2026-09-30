#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    int const want = c.jury.read_int(1, 10000, "sum");
    int const said = c.output.read_int(eo::any, "sum");
    if (said != want) eo::wrong("the sum is {}, not {}", want, said);
    eo::accept("the sum is {}", want);
}
