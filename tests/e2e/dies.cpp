#include "../../eolymp.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int deeper(int depth, int stop) {
    volatile char pad[4096];
    pad[0] = static_cast<char>(depth);
    if (depth == stop) return pad[0];
    return deeper(depth + 1, stop) + pad[0];
}

}  // namespace

int main(int argc, char** argv) {
    std::string const role = std::getenv("ROLE") != nullptr ? std::getenv("ROLE") : "";
    if (role == "interactor") {
        eo::interactor it(argc, argv);
        it.send(1);
        throw std::runtime_error("the interactor lost count");
    }
    eo::checker c(argc, argv);
    (void)c.output.read_int(eo::any, "x");
    std::printf("printed before the end\n");
    eo::log("logged before the end");
    std::cerr << "said on stderr" << std::endl;
    if (role == "throws") throw std::out_of_range("vector::at: 7 >= 3");
    if (role == "throws_int") throw 7;
    if (role == "aborts") std::abort();
    if (role == "segfaults") std::raise(SIGSEGV);
    if (role == "divides") std::raise(SIGFPE);
    if (role == "overflows") std::printf("%d\n", deeper(0, argc > 1000 ? 0 : -1));
    eo::accept();
}
