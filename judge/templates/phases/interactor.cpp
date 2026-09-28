#include <eolymp.h>

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    eo::phases ph(it, 2);
    long long const x = it.input.read_long(0, 1000000000, "x");
    if (ph.number() == 1) {
        it.send("alice", x);
        std::string const code = it.contestant.read_token(1, 60, eo::charset("01"), "code");
        ph.handoff([&](eo::writer& w) { w.line(code); });
    }
    std::string const code = ph.previous().read_token(1, 60, eo::charset("01"), "code");
    it.send("bob", code);
    long long const said = it.contestant.read_long(eo::any, "x");
    if (said != x) eo::wrong("Bob answered {}, the number was {}", said, x);
    if (code.size() <= 30) eo::accept("{} characters", code.size());
    eo::score(eo::ratio(30, static_cast<long long>(code.size())), "{} characters", code.size());
}
