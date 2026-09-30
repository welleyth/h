#include "../../eolymp.h"

#include <string>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    std::string const line = c.output.read_line(eo::any, "line");
    std::string shown;
    for (char const one : line) {
        int const code = static_cast<unsigned char>(one);
        shown += code < 32 || code > 126 ? eo::fmt("<{}>", code) : std::string(1, one);
    }
    eo::accept("a line of {} bytes: {}", line.size(), shown);
}
