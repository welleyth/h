#include "../../eolymp.h"

#include <string>

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    int const mode = it.input.read_int(1, 9, "mode");
    long long const param = it.input.read_long(0, 100000000, "param");
    if (mode == 1) {
        it.send(std::string(std::size_t{1} << 20, 'a'));
        long long count = 0;
        for (;;) {
            long long const value = it.contestant.read_long(-1, 7, "value");
            if (value == -1) break;
            count++;
        }
        eo::accept("{} lines read after a 1 MB send", count);
    }
    if (mode == 2) {
        for (long long at = 0; at < param; at++) it.send(123456);
        it.flush();
        long long const answer = it.contestant.read_long(eo::any, "answer");
        eo::accept("got {}", answer);
    }
    if (mode == 3) {
        for (long long at = 1; at <= param; at++) {
            it.send(at);
            long long const got = it.contestant.read_long(eo::any, "double");
            if (got != 2 * at) eo::wrong("round {}: {} is not {}", at, got, 2 * at);
        }
        it.send(0);
        eo::accept("{} slow rounds", param);
    }
    if (mode == 4) {
        long long const said = it.contestant.read_long(eo::any, "said");
        it.send(std::string(60000, 'b'));
        eo::accept("last words after {}", said);
    }
    if (mode == 5) {
        long long x = 0;
        for (long long round = 0; round < param; round++) {
            it.send(x);
            long long const next = it.contestant.read_long(eo::any, "next");
            if (next != x + 1) eo::wrong("round {}: {} after {}", round, next, x);
            x = next;
        }
        it.send(-1);
        eo::accept("{} round trips", param);
    }
    if (mode == 7) {
        long long const said = it.contestant.read_long(eo::any, "said");
        eo::accept("heard {}", said);
    }
    if (mode == 8) {
        it.send(param);
        for (long long at = 1; at <= param; at++) it.send(at);
        it.flush();
        long long total = 0;
        for (long long at = 1; at <= param; at++) total += it.contestant.read_long(0, 400000000, "double");
        if (total != param * (param + 1)) eo::wrong("the doubles add up to {}", total);
        eo::accept("{} lines both ways", param);
    }
    eo::jury_error("unknown mode {}", mode);
}
