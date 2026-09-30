#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

DWORD constexpr patience_ms = 10000;
int serial = 0;

HANDLE made_pipe(std::string& name, DWORD direction) {
    name = "\\\\.\\pipe\\eolymp-serve-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(serial++);
    SECURITY_ATTRIBUTES kept{sizeof(kept), nullptr, FALSE};
    HANDLE const made = CreateNamedPipeA(name.c_str(), direction, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                                         4096, 4096, 0, &kept);
    if (made == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "serve: cannot make %s: error %lu\n", name.c_str(), GetLastError());
        std::exit(2);
    }
    return made;
}

void connected(HANDLE pipe) {
    if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
        std::fprintf(stderr, "serve: nobody opened a pipe: error %lu\n", GetLastError());
        std::exit(2);
    }
}

HANDLE start(std::string line, HANDLE in, HANDLE out) {
    STARTUPINFOA told{};
    told.cb = sizeof(told);
    told.dwFlags = STARTF_USESTDHANDLES;
    told.hStdInput = in != nullptr ? in : GetStdHandle(STD_INPUT_HANDLE);
    told.hStdOutput = out != nullptr ? out : GetStdHandle(STD_OUTPUT_HANDLE);
    told.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    for (HANDLE const one : {in, out})
        if (one != nullptr) SetHandleInformation(one, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    PROCESS_INFORMATION started{};
    BOOL const made = CreateProcessA(nullptr, &line[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &told, &started);
    for (HANDLE const one : {in, out})
        if (one != nullptr) SetHandleInformation(one, HANDLE_FLAG_INHERIT, 0);
    if (!made) {
        std::fprintf(stderr, "serve: cannot start %s: error %lu\n", line.c_str(), GetLastError());
        std::exit(2);
    }
    CloseHandle(started.hThread);
    return started.hProcess;
}

std::string read_line(HANDLE pipe) {
    std::string line;
    char one = 0;
    DWORD got = 0;
    while (ReadFile(pipe, &one, 1, &got, nullptr) && got == 1) {
        if (one == '\n') return line;
        line.push_back(one);
    }
    return line;
}

void say(HANDLE pipe, std::string const& text) {
    DWORD wrote = 0;
    WriteFile(pipe, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr);
}

std::string quoted(char const* word) { return "\"" + std::string(word) + "\""; }

}  // namespace

int main(int argc, char** argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    if (argc < 6) {
        std::fprintf(stderr, "serve <dir> <limit> <controller> <input> <summary> -- <solution>\n");
        return 2;
    }
    int const limit = std::atoi(argv[2]);
    int split = 0;
    for (int at = 1; at < argc; at++)
        if (std::string(argv[at]) == "--") split = at;
    if (split == 0) return 2;
    std::string solution;
    for (int at = split + 1; at < argc; at++) solution += (solution.empty() ? "" : " ") + quoted(argv[at]);

    std::string requests;
    std::string replies;
    HANDLE const hear = made_pipe(requests, PIPE_ACCESS_INBOUND);
    HANDLE const tell = made_pipe(replies, PIPE_ACCESS_OUTBOUND);
    SetEnvironmentVariableA("CONTROL_OUTPUT_FILE", requests.c_str());
    SetEnvironmentVariableA("CONTROL_INPUT_FILE", replies.c_str());
    SetEnvironmentVariableA("INSTANCE_LIMIT", argv[2]);
    HANDLE const jury = start(quoted(argv[3]) + " " + quoted(argv[4]) + " " + quoted(argv[5]), nullptr, nullptr);
    connected(hear);
    connected(tell);

    std::vector<HANDLE> instances;
    for (;;) {
        std::string const request = read_line(hear);
        if (request.empty()) break;
        if (request != "SPAWN") continue;
        if (static_cast<int>(instances.size()) >= limit) {
            say(tell, "LIMIT\n");
            continue;
        }
        std::string to_them;
        std::string from_them;
        HANDLE const their_input = made_pipe(to_them, PIPE_ACCESS_INBOUND);
        HANDLE const their_output = made_pipe(from_them, PIPE_ACCESS_OUTBOUND);
        say(tell, to_them + " " + from_them + "\n");
        connected(their_input);
        connected(their_output);
        instances.push_back(start(solution, their_input, their_output));
        CloseHandle(their_input);
        CloseHandle(their_output);
    }

    bool out_of_time = WaitForSingleObject(jury, patience_ms) == WAIT_TIMEOUT;
    for (HANDLE const one : instances)
        if (WaitForSingleObject(one, patience_ms) == WAIT_TIMEOUT) {
            out_of_time = true;
            TerminateProcess(one, 9);
        }
    if (out_of_time) {
        TerminateProcess(jury, 9);
        std::fprintf(stderr, "serve: the controller and its instances were still running after %lu ms, and were "
                             "killed\n", patience_ms);
    }
    DWORD code = 0;
    GetExitCodeProcess(jury, &code);
    std::printf("controller %ld instances %zu\n", static_cast<long>(code), instances.size());
    return 0;
}
