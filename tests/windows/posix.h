#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_MSC_VER)
using ssize_t = long long;

inline bool S_ISREG(unsigned short mode) { return (mode & _S_IFMT) == _S_IFREG; }
#endif

inline int setenv(char const* name, char const* value, int) { return _putenv_s(name, value); }

inline int unsetenv(char const* name) { return _putenv_s(name, ""); }

inline int pipe(int ends[2]) { return _pipe(ends, 1 << 16, _O_BINARY); }

inline int dprintf(int descriptor, char const* pattern, ...) {
    va_list values;
    va_start(values, pattern);
    char buffer[4096];
    int const length = std::vsnprintf(buffer, sizeof(buffer), pattern, values);
    va_end(values);
    if (length <= 0) return length;
    return _write(descriptor, buffer, static_cast<unsigned>(length < 4096 ? length : 4095));
}

inline int mkstemp(char* pattern) {
    if (_mktemp_s(pattern, std::char_traits<char>::length(pattern) + 1) != 0) return -1;
    return _open(pattern, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
}

#if defined(_MSC_VER)
inline void a_bad_descriptor_is_an_error(wchar_t const*, wchar_t const*, wchar_t const*, unsigned, std::uintptr_t) {}
#endif

inline bool opens_binary() {
#if defined(_MSC_VER)
    _set_invalid_parameter_handler(&a_bad_descriptor_is_an_error);
    _set_fmode(_O_BINARY);
#else
    _fmode = _O_BINARY;
#endif
    return true;
}

inline bool const opened_in_binary = opens_binary();
