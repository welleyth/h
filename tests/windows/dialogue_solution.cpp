#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

static void nap(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static void binary_out() {
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

int main(int argc, char** argv) {
    binary_out();
    std::string const mode = argc > 1 ? argv[1] : "";
    long long const param = argc > 2 ? std::atoll(argv[2]) : 0;
    if (mode == "flood") {
        binary_out();
        std::size_t const total = (std::size_t{1} << 24) + static_cast<std::size_t>(param);
        std::vector<char> block(1 << 16);
        for (std::size_t at = 0; at < block.size(); at += 2) block[at] = '7', block[at + 1] = '\n';
        std::size_t left = total;
        while (left > 0) {
            std::size_t const step = left < block.size() ? left : block.size();
            if (std::fwrite(block.data(), 1, step, stdout) != step) return 5;
            left -= step;
        }
        std::fputs("-1\n", stdout);
        std::fflush(stdout);
        std::vector<char> sink(1 << 16);
        while (std::fread(sink.data(), 1, sink.size(), stdin) > 0) {
        }
        return 0;
    }
    if (mode == "early" || mode == "early_answer") {
        for (int at = 0; at < 100; at++) {
            int value = 0;
            if (std::scanf("%d", &value) != 1) return 4;
        }
        if (mode == "early_answer") {
            std::printf("5\n");
            std::fflush(stdout);
        }
        return 0;
    }
    if (mode == "slow") {
        for (;;) {
            long long x = 0;
            if (std::scanf("%lld", &x) != 1 || x == 0) return 0;
            nap(20);
            std::printf("%lld\n", 2 * x);
            std::fflush(stdout);
        }
    }
    if (mode == "sleeper" || mode == "sleeper_forever") {
        std::printf("1\n");
        std::fflush(stdout);
        nap(mode == "sleeper" ? 4000 : 60000);
        return 0;
    }
    if (mode == "echo") {
        for (;;) {
            long long x = 0;
            if (std::scanf("%lld", &x) != 1 || x < 0) return 0;
            std::printf("%lld\n", x + 1);
            std::fflush(stdout);
        }
    }
    if (mode == "closer") {
        std::fclose(stdin);
        std::printf("9\n");
        std::fflush(stdout);
        nap(1000);
        return 0;
    }
    if (mode == "chatter") {
        binary_out();
        std::string junk = "1\n";
        junk += std::string(1 << 20, 'z');
        std::fwrite(junk.data(), 1, junk.size(), stdout);
        std::fflush(stdout);
        auto const started = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - started < std::chrono::seconds(2)) {
            if (std::fwrite(junk.data() + 2, 1, 4096, stdout) != 4096 || std::fflush(stdout) != 0) break;
        }
        return 0;
    }
    if (mode == "doubler") {
        int n = 0;
        if (std::scanf("%d", &n) != 1) return 0;
        for (int at = 0; at < n; at++) {
            int value = 0;
            if (std::scanf("%d", &value) != 1) return 0;
            std::printf("%d\n", 2 * value);
        }
        std::fflush(stdout);
        return 0;
    }
    return 2;
}
