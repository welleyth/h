#include "../../eolymp.h"

#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    std::string const role = std::getenv("ROLE") != nullptr ? std::getenv("ROLE") : "";
    if (role == "checker") {
        eo::checker c(argc, argv);
        (void)c.output.read_int(eo::any, "x");
        std::exit(0);
    }
    if (role == "quick") {
        eo::checker c(argc, argv);
        (void)c.output.read_int(eo::any, "x");
#if defined(__APPLE__) || (defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_AT_QUICK_EXIT))
        std::exit(0);
#else
        std::quick_exit(0);
#endif
    }
    if (role == "leaked") {
        eo::checker* const c = new eo::checker(argc, argv);
        (void)c->output.read_int(eo::any, "x");
        return 0;
    }
    if (role == "validator") {
        eo::validator v(argc, argv);
        (void)v.read_int(1, 9, "n");
        eo::sum_limit total(5, "sum of n");
        total += 7;
        std::exit(0);
    }
    if (role == "interactor") {
        eo::interactor it(argc, argv);
        it.send(1);
        std::exit(0);
    }
    if (role == "controller") {
        eo::controller co(argc, argv);
        (void)co.input.read_int(eo::any, "n");
        std::exit(0);
    }
    if (role == "generator") {
        eo::generator g(argc, argv);
        g.out.line(g.option<int>("n", 1, 9));
        std::exit(0);
    }
    return 2;
}
