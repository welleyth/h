#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    int const want = c.jury.read_int(1, 10000, "sum");
    int const said = c.output.read_int(eo::any, "sum");
    if (said == want + 1) eo::score(0.5, "{} is one above {}", said, want);
    if (said != want) eo::wrong("the sum is {}, not {}", want, said);
    eo::accept("the sum is {}, test {} of group {}", want, c.index(), c.group());
}
