#include "../../eolymp.h"

#include <string>

int main(int argc, char** argv) {
    char const* const asked = eo::detail::environment("KIND");
    std::string const kind = asked != nullptr ? asked : "";
    eo::checker c(argc, argv);
    if (kind == "any_case") c.tokens(eo::any_case);
    if (kind == "any_order") c.tokens(eo::any_order);
    if (kind == "integers") c.integers();
    if (kind == "big") c.integers(eo::big);
    if (kind == "absolute") c.reals(1e-6, eo::absolute);
    if (kind == "yes_no") c.yes_no();
    eo::jury_error("no comparison is called {}", kind);
}
