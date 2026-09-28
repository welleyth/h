#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.answers(eo::unique);

    long long const want = c.jury.read_long(1, 100000000000000, "sum");
    long long const said = c.output.read_long(eo::any, "sum");
    if (said != want) eo::wrong("the sum is {}, not {}", want, said);
    eo::accept("the sum is {}", want);
}
