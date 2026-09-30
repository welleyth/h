#include <algorithm>
#include <array>
#include <cerrno>
#include <cfenv>
#include <charconv>
#include <chrono>
#include <climits>
#include <clocale>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <poll.h>
#include <set>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define near
#define far
#define small char
#define hyper __int64
#define interface struct
#define IN
#define OUT
#define OPTIONAL
#define CONST const
#define VOID void
#define ERROR 0
#define DELETE (0x00010000L)
#define TRANSPARENT 1
#define OPAQUE 2
#define ABSOLUTE 1
#define RELATIVE 2
#define NO_ERROR 0L
#define INFINITE 0xFFFFFFFF
#define CALLBACK
#define PASCAL
#define pascal
#define cdecl
#define STRICT 1
#define BOOL int
#define TRUE 1
#define FALSE 0
#define Yield()
#define GetMessage GetMessageA
#define SendMessage SendMessageA
#define CreateFile CreateFileA
#define DeleteFile DeleteFileA
#define GetObject GetObjectA

#include "../../eolymp.h"
#include "../../eolymp-shapes.h"

int main() {
    eo::rng draw(7);
    std::vector<eo::edge> const shaped = eo::shapes::presented(draw, eo::shapes::caterpillar(draw, 6));
    std::string const said = eo::fmt("{} {}", eo::detail::parse_real("0.5", true).value, min(2, 3));
    return shaped.size() == 5 && said == "0.5 2" ? 0 : 1;
}
