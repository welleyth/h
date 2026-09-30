#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace eot {

struct test_case {
    char const* name;
    void (*run)();
};

inline std::vector<test_case>& registry() {
    static std::vector<test_case> all;
    return all;
}

inline int& failures() {
    static int count = 0;
    return count;
}

inline int& checks() {
    static int count = 0;
    return count;
}

inline char const*& current() {
    static char const* name = "";
    return name;
}

inline int complaints_go_to() {
    static int const kept = ::dup(2);
    return kept;
}

inline void record(bool ok, char const* expression, char const* file, int line) {
    checks()++;
    if (ok) return;
    failures()++;
    ::dprintf(complaints_go_to(), "%s:%d: in %s: FAILED %s\n", file, line, current(), expression);
}

class redirect {
public:
    explicit redirect(int descriptor = 1)
        : descriptor_(descriptor), saved_(::dup(descriptor)), file_(std::tmpfile()) {
        ::dup2(::fileno(file_), descriptor_);
    }

    std::string finish() {
        std::fflush(descriptor_ == 1 ? stdout : stderr);
        ::dup2(saved_, descriptor_);
        ::close(saved_);
        std::rewind(file_);
        std::string text;
        char buffer[4096];
        std::size_t got = 0;
        while ((got = std::fread(buffer, 1, sizeof(buffer), file_)) > 0) text.append(buffer, got);
        std::fclose(file_);
        return text;
    }

private:
    int descriptor_;
    int saved_;
    std::FILE* file_;
};

struct outcome {
    bool stopped = false;
    int code = 0;
    std::string text;
    std::string printed;
    std::string complained;
};

template <class Body>
inline outcome run_until_stop(Body&& body) {
    outcome result;
    redirect capture(1);
    redirect complaints(2);
    try {
        body();
    } catch (eo::detail::stop const& reached) {
        result.stopped = true;
        result.code = reached.code;
        result.text = reached.text;
    }
    result.complained = complaints.finish();
    result.printed = capture.finish();
    return result;
}

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define EOT_ADDRESS_SANITIZER 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define EOT_ADDRESS_SANITIZER 1
#endif

inline bool sanitized() {
#ifdef EOT_ADDRESS_SANITIZER
    return true;
#else
    return false;
#endif
}

inline bool contains(std::string const& text, char const* part) {
    return text.find(part) != std::string::npos;
}

// A test that hangs must not hold CI for an hour. A watchdog process, forked
// before any test runs, sees which test is running through a shared counter and
// kills the suite when one test outlasts EOT_TEST_SECONDS (default 120; 0 turns
// it off). It is a process, not an alarm or a thread, because the tests arm
// SIGALRM and ITIMER_REAL themselves and expect the signals to land on the one
// thread they interrupt. It ends as soon as the suite does.
inline int watchdog_seconds() {
    char const* const given = std::getenv("EOT_TEST_SECONDS");
    return given != nullptr ? std::atoi(given) : 120;
}

#if !defined(_WIN32)
[[noreturn]] inline void watch(pid_t suite, int alive, std::atomic<long> const& running, int seconds) {
    long seen = running.load();
    auto since = std::chrono::steady_clock::now();
    for (;;) {
        pollfd gone{alive, POLLIN, 0};
        int const ready = ::poll(&gone, 1, 250);
        if (ready > 0 || (ready < 0 && errno != EINTR)) ::_exit(0);
        long const now = running.load();
        auto const at = std::chrono::steady_clock::now();
        if (now != seen) {
            seen = now;
            since = at;
            continue;
        }
        if (at - since < std::chrono::seconds(seconds)) continue;
        ::dprintf(complaints_go_to(),
                  "watchdog: %s has run for %d s; the suite is stopped (EOT_TEST_SECONDS sets the limit)\n",
                  seen >= 0 ? registry()[static_cast<std::size_t>(seen)].name : "the suite", seconds);
        ::kill(suite, SIGKILL);
        ::_exit(0);
    }
}

inline std::atomic<long>* start_the_watchdog() {
    int const seconds = watchdog_seconds();
    if (seconds <= 0) return nullptr;
    void* const shared = ::mmap(nullptr, sizeof(std::atomic<long>), PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) return nullptr;
    std::atomic<long>* const running = new (shared) std::atomic<long>(-1);
    int ends[2] = {-1, -1};
    if (::pipe(ends) != 0) return nullptr;
    pid_t const suite = ::getpid();
    std::fflush(nullptr);
    pid_t const watcher = ::fork();
    if (watcher == 0) {
        ::close(ends[1]);
        watch(suite, ends[0], *running, seconds);
    }
    ::close(ends[0]);
    return watcher > 0 ? running : nullptr;
}
#else
inline std::atomic<long>* start_the_watchdog() { return nullptr; }
#endif

inline int main_of_tests() {
    (void)complaints_go_to();
    bool const tracing = std::getenv("EOT_TRACE") != nullptr;
    std::atomic<long>* const running = start_the_watchdog();
    long at = 0;
    for (test_case const& one : registry()) {
        current() = one.name;
        if (running != nullptr) running->store(at);
        at++;
        if (tracing) std::fprintf(stderr, "-- %s\n", one.name);
        one.run();
    }
    ::dprintf(complaints_go_to(), "%d checks in %zu tests, %d failed\n", checks(), registry().size(),
              failures());
    return failures() == 0 ? 0 : 1;
}

}  // namespace eot

#define EO_TEST(name)                                                                     \
    static void name();                                                                   \
    static bool const name##_registered = (eot::registry().push_back({#name, &name}), true); \
    static void name()

#define EO_CHECK(...) eot::record(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

#define EO_CHECK_EQ(left, right) eot::record((left) == (right), #left " == " #right, __FILE__, __LINE__)

#define EO_CHECK_IN(text, part) eot::record(eot::contains(text, part), #text " contains " #part, __FILE__, __LINE__)
