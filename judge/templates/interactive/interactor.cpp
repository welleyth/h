#include <eolymp.h>

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    int const n = it.input.read_int(1, 1000000, "n");
    int const secret = it.input.read_int(1, n, "secret");
    eo::budget queries(it, 20, "queries");

    it.send(n);
    for (;;) {
        std::string const command = it.contestant.read_choice({"?", "!"}, "command");
        int const x = it.contestant.read_int(1, n, "x");
        if (command == "!") {
            if (x != secret) eo::wrong("answered {}, the number was {}", x, secret);
            eo::accept("{} queries", queries.used());
        }
        queries.spend();
        it.send(x < secret ? "<" : x > secret ? ">" : "=");
    }
}
