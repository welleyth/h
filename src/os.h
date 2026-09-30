#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "core.h"
#include "diag.h"
#include "fmt.h"

namespace eo {
namespace detail {

inline long long read_some(int descriptor, char* into, std::size_t most) {
    return static_cast<long long>(::read(descriptor, into, most));
}

inline int open_to_read(char const* path) { return ::open(path, O_RDONLY); }

inline int open_to_write(char const* path) { return ::open(path, O_WRONLY); }

inline void close_descriptor(int descriptor) { ::close(descriptor); }

inline int duplicate(int descriptor) { return ::dup(descriptor); }

inline void duplicate_onto(int from, int to) { ::dup2(from, to); }

inline int descriptor_of(std::FILE* file) { return ::fileno(file); }

inline bool regular_file(int descriptor, long long& size) {
    struct stat seen {};
    if (::fstat(descriptor, &seen) != 0 || !S_ISREG(seen.st_mode)) return false;
    size = static_cast<long long>(seen.st_size);
    return true;
}

inline long long offset_of(int descriptor) { return static_cast<long long>(::lseek(descriptor, 0, SEEK_CUR)); }

inline bool bytes_waiting(int descriptor, int& ready) { return ::ioctl(descriptor, FIONREAD, &ready) == 0; }

inline void ignore_broken_pipes() { ::signal(SIGPIPE, SIG_IGN); }

inline long long read_at(int descriptor, char* into, std::size_t most, long long at) {
    return static_cast<long long>(::pread(descriptor, into, most, static_cast<off_t>(at)));
}

inline void write_all(int descriptor, char const* bytes, std::size_t size) {
    while (size > 0) {
        ssize_t const wrote = ::write(descriptor, bytes, size);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) return;
        bytes += wrote;
        size -= static_cast<std::size_t>(wrote);
    }
}

inline constexpr int deadly_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL};

inline char const* how_it_died(int caught) {
    if (caught == SIGSEGV) return "SIGSEGV: it read or wrote memory it does not own, or ran out of stack";
    if (caught == SIGABRT) return "SIGABRT: it aborted, as a failed assert does";
    if (caught == SIGFPE) return "SIGFPE: an arithmetic error, such as an integer division by zero";
    if (caught == SIGBUS) return "SIGBUS: a memory access the machine refused";
    return "SIGILL: an illegal instruction, which the end of a function that returns no value can reach";
}

inline void stand_on_a_spare_stack() {
    static char spare[1 << 16];
    stack_t current{};
    if (::sigaltstack(nullptr, &current) != 0 || (current.ss_flags & SS_DISABLE) == 0) return;
    stack_t mine{};
    mine.ss_sp = spare;
    mine.ss_size = sizeof(spare);
    ::sigaltstack(&mine, nullptr);
}

using signal_dispositions = std::array<struct sigaction, std::size(deadly_signals)>;

inline void catch_the_deadly_signals(void (*handler)(int), signal_dispositions& before) {
    stand_on_a_spare_stack();
    struct sigaction deadly {};
    deadly.sa_handler = handler;
    deadly.sa_flags = static_cast<int>(SA_RESETHAND | SA_ONSTACK);
    sigemptyset(&deadly.sa_mask);
    for (std::size_t at = 0; at < before.size(); at++) ::sigaction(deadly_signals[at], &deadly, &before[at]);
}

inline void restore_the_deadly_signals(signal_dispositions const& before) {
    for (std::size_t at = 0; at < before.size(); at++) ::sigaction(deadly_signals[at], &before[at], nullptr);
}

inline bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }

inline void wait_for(int descriptor, short event) {
    pollfd ready{descriptor, event, 0};
    ::poll(&ready, 1, -1);
}

inline int constexpr last_words_patience_ms = 500;
inline int constexpr last_words_deadline_ms = 2000;

inline void write_while_read(int descriptor, std::string const& bytes, int patience_ms, int deadline_ms) {
    int const flags = ::fcntl(descriptor, F_GETFL);
    if (flags >= 0) ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);
    auto const started = std::chrono::steady_clock::now();
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        ssize_t const wrote = ::write(descriptor, bytes.data() + sent, bytes.size() - sent);
        if (wrote > 0) {
            sent += static_cast<std::size_t>(wrote);
            continue;
        }
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote == 0 || !would_block()) return;
        long long const spent = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
        if (spent >= deadline_ms) return;
        pollfd room{descriptor, POLLOUT, 0};
        int const ready = ::poll(&room, 1, static_cast<int>(std::min<long long>(patience_ms, deadline_ms - spent)));
        if (ready == 0 || (ready < 0 && errno != EINTR)) return;
    }
}

enum class absorbed { nothing, some, full };

inline std::size_t constexpr absorb_limit = std::size_t{1} << 24;

template <class Reading, class Naming>
inline void write_while_absorbing(int to, std::string const& bytes, Reading& from, bool& deaf,
                                  Naming const& who, char const* role, char const* instead) {
    std::size_t sent = 0;
    bool listening = true;
    while (sent < bytes.size()) {
        pollfd both[2] = {{to, POLLOUT, 0}, {listening ? from.listening_descriptor() : -1, POLLIN, 0}};
        int const ready = ::poll(both, 2, -1);
        if (ready < 0 && errno != EINTR && errno != EAGAIN) deaf = true;
        if (deaf) return;
        if (ready < 0) continue;
        if (both[1].revents != 0) {
            absorbed const what = from.absorb(absorb_limit);
            if (what == absorbed::full)
                warn_at_once("EO409",
                             fmt("{} sent more than {} MB while the {} was still writing to it, and the rest "
                                 "of it waits in the pipe",
                                 who(), absorb_limit >> 20, role),
                             instead, site::here());
            listening = what == absorbed::some;
        }
        if (both[0].revents == 0) continue;
        std::size_t const step = std::min<std::size_t>(bytes.size() - sent, PIPE_BUF);
        ssize_t const wrote = ::write(to, bytes.data() + sent, step);
        if (wrote > 0) sent += static_cast<std::size_t>(wrote);
        if (wrote < 0 && errno != EINTR && !would_block()) {
            deaf = true;
            return;
        }
    }
}

inline std::FILE* opened_scratch(int descriptor) {
    if (descriptor < 0) return nullptr;
    std::FILE* const file = ::fdopen(descriptor, "w+b");
    if (file == nullptr) ::close(descriptor);
    return file;
}

inline std::FILE* scratch_in_memory() {
#if defined(__linux__) && defined(SYS_memfd_create)
    return opened_scratch(static_cast<int>(::syscall(SYS_memfd_create, "eolymp-checker-output", 0)));
#else
    return nullptr;
#endif
}

inline std::FILE* scratch_in_the_temporary_directory() { return std::tmpfile(); }

inline std::FILE* scratch_in_the_workspace() {
    char name[] = "eolymp-checker-output-XXXXXX";
    int const descriptor = ::mkstemp(name);
    if (descriptor >= 0) ::unlink(name);
    return opened_scratch(descriptor);
}

}  // namespace detail
}  // namespace eo
