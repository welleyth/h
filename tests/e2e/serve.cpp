#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int constexpr patience_seconds = 10;
int constexpr most_children = 64;
pid_t children[most_children];
volatile sig_atomic_t started = 0;
volatile sig_atomic_t out_of_time = 0;

void stop_everyone(int) {
    out_of_time = 1;
    for (int at = 0; at < started; at++) ::kill(children[at], SIGKILL);
}

void remember(pid_t child) {
    if (started >= most_children) return;
    children[started] = child;
    started = started + 1;
}

void wait_for(pid_t child, int& status) {
    while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
}

std::string make_fifo(std::string const& directory, std::string const& name) {
    std::string const path = directory + "/" + name;
    ::unlink(path.c_str());
    if (::mkfifo(path.c_str(), 0600) != 0) {
        std::perror("mkfifo");
        std::exit(2);
    }
    return path;
}

std::string read_line(int descriptor) {
    std::string line;
    char one = 0;
    while (::read(descriptor, &one, 1) == 1) {
        if (one == '\n') return line;
        line.push_back(one);
    }
    return line;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "serve <dir> <limit> <controller> <input> <summary> -- <solution>\n");
        return 2;
    }
    std::string const directory = argv[1];
    int const limit = std::atoi(argv[2]);
    int split = 0;
    for (int at = 1; at < argc; at++)
        if (std::string(argv[at]) == "--") split = at;
    if (split == 0) return 2;

    struct sigaction stopping {};
    stopping.sa_handler = stop_everyone;
    ::sigaction(SIGALRM, &stopping, nullptr);
    ::alarm(patience_seconds);

    std::string const requests = make_fifo(directory, "control_out");
    std::string const replies = make_fifo(directory, "control_in");

    pid_t const jury = ::fork();
    if (jury == 0) {
        ::setenv("CONTROL_OUTPUT_FILE", requests.c_str(), 1);
        ::setenv("CONTROL_INPUT_FILE", replies.c_str(), 1);
        ::setenv("INSTANCE_LIMIT", argv[2], 1);
        char* const passed[] = {argv[3], argv[4], argv[5], nullptr};
        ::execv(argv[3], passed);
        ::_exit(127);
    }
    remember(jury);

    int const hear = ::open(requests.c_str(), O_RDONLY);
    int const tell = ::open(replies.c_str(), O_WRONLY);

    std::vector<pid_t> instances;
    while (true) {
        std::string const request = read_line(hear);
        if (request.empty()) break;
        if (request != "SPAWN") continue;
        if (static_cast<int>(instances.size()) >= limit) {
            std::string const refusal = "LIMIT\n";
            ssize_t const sent = ::write(tell, refusal.data(), refusal.size());
            (void)sent;
            continue;
        }
        std::string const name = std::to_string(instances.size());
        std::string const to_them = make_fifo(directory, "in" + name);
        std::string const from_them = make_fifo(directory, "out" + name);
        pid_t const one = ::fork();
        if (one == 0) {
            int const mine_in = ::open(to_them.c_str(), O_RDONLY);
            int const mine_out = ::open(from_them.c_str(), O_WRONLY);
            ::dup2(mine_in, 0);
            ::dup2(mine_out, 1);
            ::execv(argv[split + 1], argv + split + 1);
            ::_exit(127);
        }
        instances.push_back(one);
        remember(one);
        std::string const answer = to_them + " " + from_them + "\n";
        ssize_t const sent = ::write(tell, answer.data(), answer.size());
        (void)sent;
    }

    int jury_status = 0;
    wait_for(jury, jury_status);
    for (pid_t const one : instances) {
        int ignored = 0;
        wait_for(one, ignored);
    }
    if (out_of_time)
        std::fprintf(stderr, "serve: the controller and its instances were still running after %d s, and were "
                             "killed\n", patience_seconds);
    ::close(hear);
    ::close(tell);
    std::printf("controller %d instances %zu\n",
                WIFEXITED(jury_status) ? WEXITSTATUS(jury_status) : -1, instances.size());
    return 0;
}
