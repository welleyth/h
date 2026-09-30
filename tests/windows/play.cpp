#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

std::string command_line(std::vector<std::string> const& words) {
    std::string line;
    for (std::string const& word : words) line += (line.empty() ? "\"" : " \"") + word + "\"";
    return line;
}

HANDLE start(std::vector<std::string> const& words, HANDLE in, HANDLE out) {
    STARTUPINFOA told{};
    told.cb = sizeof(told);
    told.dwFlags = STARTF_USESTDHANDLES;
    told.hStdInput = in;
    told.hStdOutput = out;
    told.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION started{};
    std::string line = command_line(words);
    SetHandleInformation(in, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(out, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    BOOL const made = CreateProcessA(nullptr, &line[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &told, &started);
    SetHandleInformation(in, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out, HANDLE_FLAG_INHERIT, 0);
    if (!made) {
        std::fprintf(stderr, "play: cannot start %s: error %lu\n", words[0].c_str(), GetLastError());
        std::exit(2);
    }
    CloseHandle(started.hThread);
    return started.hProcess;
}

DWORD constexpr killed = 0xdead;

DWORD ending(HANDLE process) {
    DWORD code = 0;
    GetExitCodeProcess(process, &code);
    return code;
}

std::string spelled(DWORD code) { return code == killed ? "-1" : std::to_string(static_cast<long>(code)); }

}  // namespace

int main(int argc, char** argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    std::vector<std::string> jury;
    std::vector<std::string> player;
    bool const waiting = argc > 1 && std::string(argv[1]) == "--wait";
    bool after = false;
    for (int at = waiting ? 2 : 1; at < argc; at++) {
        std::string const word = argv[at];
        if (word == "--") {
            after = true;
            continue;
        }
        (after ? player : jury).push_back(word);
    }
    if (jury.empty() || player.empty()) {
        std::fprintf(stderr, "play [--wait] <interactor+args...> -- <solution>\n");
        return 2;
    }
    SECURITY_ATTRIBUTES kept{sizeof(kept), nullptr, FALSE};
    HANDLE to_jury_read = nullptr;
    HANDLE to_jury_write = nullptr;
    HANDLE to_player_read = nullptr;
    HANDLE to_player_write = nullptr;
    if (!CreatePipe(&to_jury_read, &to_jury_write, &kept, 0)) return 2;
    if (!CreatePipe(&to_player_read, &to_player_write, &kept, 0)) return 2;
    HANDLE const judge = start(jury, to_jury_read, to_player_write);
    HANDLE const solution = start(player, to_player_read, to_jury_write);
    CloseHandle(to_jury_read);
    CloseHandle(to_jury_write);
    CloseHandle(to_player_read);
    CloseHandle(to_player_write);
    WaitForSingleObject(judge, INFINITE);
    if (WaitForSingleObject(solution, waiting ? 2000 : 200) == WAIT_TIMEOUT) {
        TerminateProcess(solution, killed);
        WaitForSingleObject(solution, INFINITE);
    }
    std::printf("interactor %s solution %s\n", spelled(ending(judge)).c_str(), spelled(ending(solution)).c_str());
    return 0;
}
