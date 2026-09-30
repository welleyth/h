#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core.h"
#include "diag.h"
#include "fmt.h"

namespace eo {
namespace detail {

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

inline bool file_is_there(char const* path) {
    int const descriptor = ::open(path, O_RDONLY);
    if (descriptor < 0) return false;
    ::close(descriptor);
    return true;
}

inline void write_file(std::string const& path, std::string const& bytes, char const* what) {
    std::FILE* const file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) library_error(fmt("cannot write the {} to {}", what, path));
    bool const whole = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fclose(file) != 0 || !whole)
        library_error(fmt("the {} could not be written to {}: {}", what, path, std::strerror(errno)));
}

enum class absorbed { nothing, some, full };

class source {
public:
    static std::size_t constexpr default_chunk = mebibyte;
    static std::size_t constexpr pipe_chunk = pipe_size;

    source() = default;
    source(source const&) = delete;
    source& operator=(source const&) = delete;
    source(source&& other) noexcept { steal(other); }

    source& operator=(source&& other) noexcept {
        if (this != &other) {
            release();
            steal(other);
        }
        return *this;
    }

    ~source() { release(); }

    static source over_text(std::string_view text, bool normalize, std::size_t chunk = default_chunk) {
        source made(normalize, chunk);
        made.pending_ = text;
        made.text_backed_ = true;
        made.drained_ = text.empty();
        return made;
    }

    static source over_descriptor(int descriptor, bool owned, bool normalize,
                                  std::size_t chunk = default_chunk) {
        source made(normalize, chunk);
        made.descriptor_ = descriptor;
        made.owned_ = owned;
        made.drained_ = false;
        return made;
    }

    static source over_channel(char const* path) { return over_file(path, false, pipe_chunk); }

    static source over_file(char const* path, bool normalize, std::size_t chunk = default_chunk) {
        int const descriptor = ::open(path, O_RDONLY);
        if (descriptor < 0) library_error(fmt("cannot open {}: {}", path, std::strerror(errno)));
        return over_descriptor(descriptor, true, normalize, chunk);
    }

    int peek() {
        if (begin_ < end_) {
            int const quick = static_cast<unsigned char>(buffer_[begin_]);
            if (quick != '\r') return quick;
        }
        return peek_slowly();
    }

    int peek_slowly() {
        for (;;) {
            if (!have(1)) return -1;
            int const here = static_cast<unsigned char>(buffer_[begin_]);
            if (!normalize_ || here != '\r') return here;
            if (!have(2)) return here;
            if (static_cast<unsigned char>(buffer_[begin_ + 1]) != '\n') return here;
            begin_++;
            carriage_returns_ = true;
        }
    }

    int take() {
        int const here = peek();
        if (here < 0) return here;
        begin_++;
        if (here == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        return here;
    }

    bool at_end() { return peek() < 0; }

    std::string ahead(std::size_t limit) {
        while (held() < limit && top_up()) {
        }
        return std::string(buffer_.data() + begin_, std::min(limit, end_ - begin_));
    }

    std::size_t held() const { return end_ - begin_; }

    long long bytes_left() const {
        long long const here = static_cast<long long>(held());
        if (drained_) return here;
        if (text_backed_) return here + static_cast<long long>(pending_.size());
        struct stat seen {};
        if (::fstat(descriptor_, &seen) != 0 || !S_ISREG(seen.st_mode)) return -1;
        off_t const at = ::lseek(descriptor_, 0, SEEK_CUR);
        if (at < 0) return -1;
        return here + static_cast<long long>(seen.st_size - at);
    }
    char const* window() const { return buffer_.data() + begin_; }

    void skip_plain(std::size_t count) {
        begin_ += count;
        column_ += static_cast<long long>(count);
    }

    int listening_descriptor() const { return drained_ || text_backed_ ? -1 : descriptor_; }

    absorbed absorb(std::size_t most) {
        if (drained_ || text_backed_) return absorbed::nothing;
        int ready = 0;
        if (::ioctl(descriptor_, FIONREAD, &ready) != 0 || ready <= 0) return absorbed::nothing;
        std::size_t const wanted = static_cast<std::size_t>(ready);
        if (held() + wanted > most) return absorbed::full;
        compact();
        if (buffer_.size() - end_ < wanted) buffer_.resize(end_ + wanted);
        ssize_t const got = ::read(descriptor_, buffer_.data() + end_, wanted);
        if (got < 0) return errno == EINTR ? absorbed::some : absorbed::nothing;
        end_ += static_cast<std::size_t>(got);
        return got > 0 ? absorbed::some : absorbed::nothing;
    }

    bool top_up() {
        if (drained_) return false;
        if (!text_backed_) {
            int ready = 0;
            if (::ioctl(descriptor_, FIONREAD, &ready) != 0 || ready <= 0) return false;
        }
        return have(held() + 1);
    }

    long long line() const { return line_; }
    long long column() const { return column_; }
    bool carriage_returns() const { return carriage_returns_; }

private:
    source(bool normalize, std::size_t chunk)
        : buffer_(std::max<std::size_t>(chunk, 1)), normalize_(normalize) {}

    void release() {
        if (owned_ && descriptor_ >= 0) ::close(descriptor_);
        descriptor_ = -1;
        owned_ = false;
    }

    void steal(source& other) {
        buffer_ = std::move(other.buffer_);
        begin_ = other.begin_;
        end_ = other.end_;
        descriptor_ = other.descriptor_;
        owned_ = other.owned_;
        drained_ = other.drained_;
        normalize_ = other.normalize_;
        carriage_returns_ = other.carriage_returns_;
        pending_ = other.pending_;
        text_backed_ = other.text_backed_;
        line_ = other.line_;
        column_ = other.column_;
        other.descriptor_ = -1;
        other.owned_ = false;
        other.drained_ = true;
        other.begin_ = other.end_ = 0;
    }

    void compact() {
        if (begin_ == 0) return;
        std::memmove(buffer_.data(), buffer_.data() + begin_, end_ - begin_);
        end_ -= begin_;
        begin_ = 0;
    }

    bool have(std::size_t count) {
        while (end_ - begin_ < count && !drained_) {
            if (end_ == buffer_.size()) {
                compact();
                if (end_ == buffer_.size()) break;
            }
            std::size_t const room = buffer_.size() - end_;
            if (text_backed_) {
                std::size_t const taken = std::min(room, pending_.size());
                std::memcpy(buffer_.data() + end_, pending_.data(), taken);
                pending_.remove_prefix(taken);
                end_ += taken;
                if (pending_.empty()) drained_ = true;
                continue;
            }
            ssize_t const got = ::read(descriptor_, buffer_.data() + end_, room);
            if (got < 0) {
                if (errno == EINTR) continue;
                if (would_block()) {
                    wait_for(descriptor_, POLLIN);
                    continue;
                }
                library_error(fmt("cannot read the input: {}", std::strerror(errno)));
            }
            if (got == 0) {
                drained_ = true;
                release();
            }
            end_ += static_cast<std::size_t>(got);
        }
        return end_ - begin_ >= count;
    }

    std::vector<char> buffer_;
    std::size_t begin_ = 0;
    std::size_t end_ = 0;
    int descriptor_ = -1;
    bool owned_ = false;
    bool drained_ = true;
    bool normalize_ = false;
    bool carriage_returns_ = false;
    std::string_view pending_;
    bool text_backed_ = false;
    long long line_ = 1;
    long long column_ = 1;
};

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

}  // namespace detail
}  // namespace eo
