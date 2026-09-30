#include "../../eolymp.h"

#include <string>

void every_call_that_takes_a_message(eo::validator& v, eo::generator& g, eo::stream& s, eo::phases& ph, int n) {
    v.require(n > 0, "n = {} is not positive", n);
    g.require(n > 0, "-n={} is not positive", n);
    eo::log("n = {}, {{literally}}", n);
    if (n == 1) s.wrong("{} and {}", n, n + 1);
    if (n == 2) ph.finish(0.5, "half of {}", n);
    if (n == 3) eo::accept("{} queries", n);
    if (n == 4) eo::wrong("answered {}", n);
    if (n == 5) eo::jury_error("the jury has {} and {}", n, n);
    if (n == 6) eo::score(0.5, "{} of {}", n, 2 * n);
    if (n == 7) eo::score(0.5, eo::round_to(1), "{}", n);
    if (n == 8) eo::points(3, "{} points", n);
    eo::accept();
}

int main(int argc, char**) {
    std::string const chosen = argc > 99 ? "{}" : "{} {}";
    bool const told = eo::fmt("{} of {}", 1, 2) == "1 of 2" && eo::fmt(chosen, 7) == "7 {}";
    bool const raised = eo::detail::diagnostics::shared().raised_already("EO112");
    eo::detail::diagnostics::shared().forget_everything();
    char held[16] = "{} and {}";
    if (argc > 99) held[0] = 'x';
    bool const kept = eo::fmt(held, 7) == "7 and {}";
    std::string const digits = argc > 99 ? "[" : "[0-9]+";
    char grouped[8] = "(ab|c)*";
    bool const matched = eo::pattern("[a-z]{1,3}").matches("abc") && eo::pattern(digits).matches("42") &&
                         eo::pattern(grouped).matches("abcab");
    return told && raised && kept && matched && eo::detail::diagnostics::shared().raised_already("EO112") ? 0 : 1;
}
