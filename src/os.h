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

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "core.h"
#include "diag.h"
#include "fmt.h"

#if defined(_WIN32)
extern "C" {
__declspec(dllimport) int __stdcall PeekNamedPipe(void*, void*, unsigned long, unsigned long*, unsigned long*,
                                                  unsigned long*);
__declspec(dllimport) unsigned long __stdcall GetFileType(void*);
}
#endif

namespace eo {
namespace detail {

inline int constexpr last_words_patience_ms = 500;
inline int constexpr last_words_deadline_ms = 2000;

enum class absorbed { nothing, some, full };

inline std::size_t constexpr absorb_limit = std::size_t{1} << 24;

#if defined(SIGBUS)
inline constexpr int deadly_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL};
#else
inline constexpr int deadly_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL};
#endif

inline char const* how_it_died(int caught) {
    if (caught == SIGSEGV) return "SIGSEGV: it read or wrote memory it does not own, or ran out of stack";
    if (caught == SIGABRT) return "SIGABRT: it aborted, as a failed assert does";
    if (caught == SIGFPE) return "SIGFPE: an arithmetic error, such as an integer division by zero";
#if defined(SIGBUS)
    if (caught == SIGBUS) return "SIGBUS: a memory access the machine refused";
#endif
    return "SIGILL: an illegal instruction, which the end of a function that returns no value can reach";
}

#if defined(_WIN32)
inline void* handle_of(int descriptor) { return reinterpret_cast<void*>(::_get_osfhandle(descriptor)); }

inline unsigned long constexpr file_type_pipe = 3;

inline bool a_pipe(int descriptor) {
    return ::_get_osfhandle(descriptor) != -1 && ::GetFileType(handle_of(descriptor)) == file_type_pipe;
}

inline bool binary_standard_streams() {
    ::_setmode(0, _O_BINARY);
    ::_setmode(1, _O_BINARY);
    ::_setmode(2, _O_BINARY);
    return true;
}

inline bool const standard_streams_are_binary = binary_standard_streams();

inline void keep_binary(int descriptor) { ::_setmode(descriptor, _O_BINARY); }

inline unsigned constexpr longest_step = 1u << 30;

inline long long read_some(int descriptor, char* into, std::size_t most) {
    return ::_read(descriptor, into, static_cast<unsigned>((std::min)(most, std::size_t{longest_step})));
}

inline long long write_some(int descriptor, char const* from, std::size_t most) {
    return ::_write(descriptor, from, static_cast<unsigned>((std::min)(most, std::size_t{longest_step})));
}

inline int open_to_read(char const* path) { return ::_open(path, _O_RDONLY | _O_BINARY); }

inline int open_to_write(char const* path) { return ::_open(path, _O_WRONLY | _O_BINARY); }

inline void close_descriptor(int descriptor) { ::_close(descriptor); }

inline int duplicate(int descriptor) { return ::_dup(descriptor); }

inline void duplicate_onto(int from, int to) { ::_dup2(from, to); }

inline int descriptor_of(std::FILE* file) { return ::_fileno(file); }

inline bool regular_file(int descriptor, long long& size) {
    struct _stat64 seen {};
    if (::_fstat64(descriptor, &seen) != 0 || (seen.st_mode & _S_IFMT) != _S_IFREG) return false;
    size = static_cast<long long>(seen.st_size);
    return true;
}

inline long long offset_of(int descriptor) { return ::_lseeki64(descriptor, 0, SEEK_CUR); }

inline int at_most_an_int(long long count) { return static_cast<int>((std::min)(count, 0x7fffffffLL)); }

inline bool bytes_waiting(int descriptor, int& ready) {
    if (a_pipe(descriptor)) {
        unsigned long held = 0;
        if (::PeekNamedPipe(handle_of(descriptor), nullptr, 0, nullptr, &held, nullptr) == 0) return false;
        ready = at_most_an_int(static_cast<long long>(held));
        return true;
    }
    long long size = 0;
    if (!regular_file(descriptor, size)) return false;
    long long const at = offset_of(descriptor);
    if (at < 0) return false;
    ready = at_most_an_int((std::max)(size - at, 0LL));
    return true;
}

inline void ignore_broken_pipes() {}

inline long long read_at(int descriptor, char* into, std::size_t most, long long at) {
    long long const was = offset_of(descriptor);
    if (was < 0 || ::_lseeki64(descriptor, at, SEEK_SET) < 0) return -1;
    long long const got = read_some(descriptor, into, most);
    ::_lseeki64(descriptor, was, SEEK_SET);
    return got;
}

inline void write_all(int descriptor, char const* bytes, std::size_t size) {
    while (size > 0) {
        long long const wrote = write_some(descriptor, bytes, size);
        if (wrote <= 0) return;
        bytes += wrote;
        size -= static_cast<std::size_t>(wrote);
    }
}

using signal_dispositions = std::array<void (*)(int), std::size(deadly_signals)>;

inline void catch_the_deadly_signals(void (*handler)(int), signal_dispositions& before) {
    for (std::size_t at = 0; at < before.size(); at++) before[at] = std::signal(deadly_signals[at], handler);
}

inline void restore_the_deadly_signals(signal_dispositions const& before) {
    for (std::size_t at = 0; at < before.size(); at++)
        if (before[at] != SIG_ERR) std::signal(deadly_signals[at], before[at]);
}

inline bool waited_to_read(int) { return false; }

inline void write_while_read(int descriptor, std::string const& bytes, int, int) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        long long const wrote = write_some(descriptor, bytes.data() + sent, bytes.size() - sent);
        if (wrote <= 0) return;
        sent += static_cast<std::size_t>(wrote);
    }
}

template <class Reading, class Naming>
inline void write_while_absorbing(int to, std::string const& bytes, Reading&, bool& deaf, Naming const&,
                                  char const*, char const*) {
    std::size_t sent = 0;
    while (sent < bytes.size() && !deaf) {
        long long const wrote = write_some(to, bytes.data() + sent, bytes.size() - sent);
        if (wrote <= 0) deaf = true;
        if (wrote > 0) sent += static_cast<std::size_t>(wrote);
    }
}

inline std::FILE* opened_scratch(int descriptor) {
    if (descriptor < 0) return nullptr;
    std::FILE* const file = ::_fdopen(descriptor, "w+b");
    if (file == nullptr) ::_close(descriptor);
    return file;
}

inline std::FILE* scratch_in_memory() { return nullptr; }

inline int constexpr scratch_names_tried = 100;

inline std::FILE* scratch_in(std::string folder) {
    static unsigned long long named = 0;
    if (!folder.empty() && folder.back() != '\\' && folder.back() != '/') folder += '\\';
    for (int attempt = 0; attempt < scratch_names_tried; attempt++) {
        std::string const name = folder + fmt("eolymp-checker-output-{}-{}", ::_getpid(), ++named);
        int const descriptor = ::_open(name.c_str(), _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY,
                                       _S_IREAD | _S_IWRITE);
        if (descriptor >= 0) return opened_scratch(descriptor);
        if (errno != EEXIST) return nullptr;
    }
    return nullptr;
}

inline std::FILE* scratch_in_the_temporary_directory() {
    for (char const* variable : {"TEMP", "TMP"}) {
        char const* const folder = environment(variable);
        if (folder == nullptr || *folder == '\0') continue;
        if (std::FILE* const made = scratch_in(folder)) return made;
    }
    return nullptr;
}

inline std::FILE* scratch_in_the_workspace() { return scratch_in(""); }
#else

inline long long read_some(int descriptor, char* into, std::size_t most) {
    return static_cast<long long>(::read(descriptor, into, most));
}

inline void keep_binary(int) {}

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

inline bool waited_to_read(int descriptor) {
    if (!would_block()) return false;
    wait_for(descriptor, POLLIN);
    return true;
}

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
#endif

}  // namespace detail
}  // namespace eo
