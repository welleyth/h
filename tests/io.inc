namespace {

std::string drain(eo::detail::source& from) {
    std::string text;
    for (int here = from.take(); here >= 0; here = from.take()) text.push_back(static_cast<char>(here));
    return text;
}

std::string temporary_path() {
    char const* directory = std::getenv("TMPDIR");
    if (directory == nullptr) directory = std::getenv("TEMP");
    std::string pattern = std::string(directory != nullptr ? directory : "/tmp") + "/eolymp_io_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    int const descriptor = ::mkstemp(buffer.data());
    ::close(descriptor);
    return std::string(buffer.data());
}

#if !defined(_WIN32)
int interrupted_writer = -1;

void answer_the_alarm(int) {
    char const byte = 'x';
    ssize_t const written = ::write(interrupted_writer, &byte, 1);
    (void)written;
}
#endif

}  // namespace

EO_TEST(a_text_source_yields_its_bytes) {
    eo::detail::source over = eo::detail::source::over_text("ab\nc", false);
    EO_CHECK_EQ(over.peek(), static_cast<int>('a'));
    EO_CHECK_EQ(over.peek(), static_cast<int>('a'));
    EO_CHECK_EQ(drain(over), std::string("ab\nc"));
    EO_CHECK(over.at_end());
    EO_CHECK_EQ(over.take(), -1);
}

EO_TEST(an_empty_source_ends_at_once) {
    eo::detail::source empty = eo::detail::source::over_text("", false);
    EO_CHECK(empty.at_end());
    eo::detail::source nothing;
    EO_CHECK(nothing.at_end());
}

EO_TEST(a_source_counts_lines_and_columns) {
    eo::detail::source over = eo::detail::source::over_text("ab\ncd", false);
    EO_CHECK_EQ(over.line(), 1LL);
    EO_CHECK_EQ(over.column(), 1LL);
    over.take();
    EO_CHECK_EQ(over.column(), 2LL);
    over.take();
    over.take();
    EO_CHECK_EQ(over.line(), 2LL);
    EO_CHECK_EQ(over.column(), 1LL);
    over.take();
    EO_CHECK_EQ(over.line(), 2LL);
    EO_CHECK_EQ(over.column(), 2LL);
}

EO_TEST(carriage_returns_are_dropped_only_when_asked) {
    eo::detail::source kept = eo::detail::source::over_text("a\r\nb", false);
    EO_CHECK_EQ(drain(kept), std::string("a\r\nb"));
    EO_CHECK(!kept.carriage_returns());
    eo::detail::source dropped = eo::detail::source::over_text("a\r\nb", true);
    EO_CHECK_EQ(drain(dropped), std::string("a\nb"));
    EO_CHECK(dropped.carriage_returns());
}

EO_TEST(a_carriage_return_without_a_line_break_survives) {
    eo::detail::source alone = eo::detail::source::over_text("a\rb", true);
    EO_CHECK_EQ(drain(alone), std::string("a\rb"));
    EO_CHECK(!alone.carriage_returns());
    eo::detail::source doubled = eo::detail::source::over_text("\r\r\n", true);
    EO_CHECK_EQ(drain(doubled), std::string("\r\n"));
    EO_CHECK(doubled.carriage_returns());
    eo::detail::source trailing = eo::detail::source::over_text("a\r", true);
    EO_CHECK_EQ(drain(trailing), std::string("a\r"));
}

EO_TEST(a_source_refills_and_compacts_its_buffer) {
    eo::detail::source small = eo::detail::source::over_text("abcdefghij", false, 4);
    EO_CHECK_EQ(drain(small), std::string("abcdefghij"));
    eo::detail::source across = eo::detail::source::over_text("abc\r\ndef", true, 4);
    EO_CHECK_EQ(drain(across), std::string("abc\ndef"));
}

EO_TEST(a_buffer_too_small_to_look_ahead_keeps_the_carriage_return) {
    eo::detail::source pinched = eo::detail::source::over_text("a\r\nb", true, 1);
    EO_CHECK_EQ(drain(pinched), std::string("a\r\nb"));
}

EO_TEST(ahead_shows_what_comes_next_without_consuming_it) {
    eo::detail::source over = eo::detail::source::over_text("hello world", false);
    EO_CHECK_EQ(over.ahead(5), std::string("hello"));
    EO_CHECK_EQ(over.peek(), static_cast<int>('h'));
    EO_CHECK_EQ(over.ahead(5), std::string("hello"));
    EO_CHECK_EQ(over.ahead(5), std::string("hello"));
    EO_CHECK_EQ(over.take(), static_cast<int>('h'));
    EO_CHECK_EQ(over.ahead(4), std::string("ello"));
    EO_CHECK_EQ(over.ahead(1000), std::string("ello world"));
    eo::detail::source narrow = eo::detail::source::over_text("hello", false, 2);
    EO_CHECK_EQ(narrow.peek(), static_cast<int>('h'));
    EO_CHECK_EQ(narrow.ahead(1000), std::string("he"));
}

EO_TEST(a_file_source_reads_the_file) {
    std::string const path = temporary_path();
    std::FILE* writing = std::fopen(path.c_str(), "w");
    std::fputs("1 2 3\n", writing);
    std::fclose(writing);
    eo::detail::source over = eo::detail::source::over_file(path.c_str(), true);
    EO_CHECK_EQ(drain(over), std::string("1 2 3\n"));
    ::unlink(path.c_str());
}

EO_TEST(a_file_that_is_not_there_is_a_library_error) {
    eot::outcome const result = eot::run_until_stop([] {
        eo::detail::source lost = eo::detail::source::over_file("/eolymp/no/such/file", true);
        (void)lost.peek();
    });
    EO_CHECK(result.stopped);
    EO_CHECK_IN(result.text, "cannot open /eolymp/no/such/file");
}

EO_TEST(looking_ahead_never_waits_for_more_of_a_pipe) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    EO_CHECK_EQ(::write(ends[1], "abc\n", 4), static_cast<ssize_t>(4));

    eo::detail::source over = eo::detail::source::over_descriptor(ends[0], true, false);
    EO_CHECK_EQ(over.peek(), static_cast<int>('a'));
    EO_CHECK_EQ(over.ahead(64), std::string("abc\n"));

    ::close(ends[1]);
    ::close(ends[0]);
}

EO_TEST(looking_ahead_takes_what_has_arrived_beyond_the_buffer) {
    std::string const text = "abcdefghijklmnopqrst";
    eo::detail::source over = eo::detail::source::over_text(text, false, 16);
    for (int at = 0; at < 10; at++) (void)over.take();
    EO_CHECK_EQ(over.held(), static_cast<std::size_t>(6));
    EO_CHECK_EQ(over.ahead(8), std::string("klmnopqr"));
    EO_CHECK_EQ(over.ahead(64), std::string("klmnopqrst"));

    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    EO_CHECK_EQ(::write(ends[1], "abc", 3), static_cast<ssize_t>(3));
    eo::detail::source piped = eo::detail::source::over_descriptor(ends[0], true, false);
    EO_CHECK_EQ(piped.peek(), static_cast<int>('a'));
    EO_CHECK_EQ(::write(ends[1], "def\n", 4), static_cast<ssize_t>(4));
    EO_CHECK_EQ(piped.ahead(64), std::string("abcdef\n"));
    ::close(ends[1]);
}

EO_TEST(topping_up_takes_only_what_is_already_waiting) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    eo::detail::source over = eo::detail::source::over_descriptor(ends[0], true, false);
    EO_CHECK(!over.top_up());
    EO_CHECK_EQ(::write(ends[1], "ab", 2), static_cast<ssize_t>(2));
    EO_CHECK(over.top_up());
    EO_CHECK_EQ(over.held(), static_cast<std::size_t>(2));
    EO_CHECK(!over.top_up());
    ::close(ends[1]);
    EO_CHECK(!over.top_up());
    EO_CHECK_EQ(over.take(), static_cast<int>('a'));
    EO_CHECK_EQ(over.take(), static_cast<int>('b'));
    EO_CHECK(over.at_end());
    EO_CHECK(!over.top_up());
}

EO_TEST(topping_up_a_text_source_never_waits) {
    eo::detail::source over = eo::detail::source::over_text("abcdef", false, 4);
    EO_CHECK_EQ(over.take(), static_cast<int>('a'));
    EO_CHECK_EQ(over.held(), static_cast<std::size_t>(3));
    EO_CHECK(over.top_up());
    EO_CHECK_EQ(over.ahead(64), std::string("bcde"));
    EO_CHECK_EQ(drain(over), std::string("bcdef"));
    EO_CHECK(!over.top_up());
}

#if !defined(_WIN32)
EO_TEST(a_last_word_is_written_while_it_is_read) {
    ::signal(SIGPIPE, SIG_IGN);
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    eo::detail::write_while_read(ends[1], "done\n", 50, 1000);
    char got[8] = {};
    EO_CHECK_EQ(::read(ends[0], got, sizeof(got)), static_cast<ssize_t>(5));
    EO_CHECK_EQ(std::string(got, 5), std::string("done\n"));

    std::string const whole(1 << 20, 'x');
    int counted[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(counted), 0);
    pid_t const reader = ::fork();
    if (reader == 0) {
        ::close(ends[1]);
        long long total = 0;
        char chunk[4096];
        for (int round = 1;; round++) {
            ssize_t const one = ::read(ends[0], chunk, sizeof(chunk));
            if (one <= 0) break;
            total += one;
            if (round % 32 == 0) ::usleep(2000);
        }
        ssize_t const told = ::write(counted[1], &total, sizeof(total));
        ::_exit(told == static_cast<ssize_t>(sizeof(total)) ? 0 : 1);
    }
    ::close(ends[0]);
    eo::detail::write_while_read(ends[1], whole, 1000, 20000);
    ::close(ends[1]);
    long long total = 0;
    EO_CHECK_EQ(::read(counted[0], &total, sizeof(total)), static_cast<ssize_t>(sizeof(total)));
    EO_CHECK_EQ(total, static_cast<long long>(whole.size()));
    int status = 0;
    ::waitpid(reader, &status, 0);
    ::close(counted[0]);
    ::close(counted[1]);

    EO_CHECK_EQ(::pipe(ends), 0);
    auto const started = std::chrono::steady_clock::now();
    eo::detail::write_while_read(ends[1], whole, 50, 1000);
    auto const waited = std::chrono::steady_clock::now() - started;
    EO_CHECK(waited >= std::chrono::milliseconds(50));
    EO_CHECK(waited < std::chrono::seconds(5));
    ::close(ends[0]);
    eo::detail::write_while_read(ends[1], "nobody reads this\n", 5000, 5000);
    EO_CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(5));
    ::close(ends[1]);
}
#endif

#if !defined(_WIN32)
EO_TEST(a_last_word_gives_up_on_a_reader_that_trickles) {
    ::signal(SIGPIPE, SIG_IGN);
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    pid_t const reader = ::fork();
    if (reader == 0) {
        ::close(ends[1]);
        char chunk[4096];
        for (int round = 0; round < 400; round++) {
            if (::read(ends[0], chunk, sizeof(chunk)) <= 0) break;
            ::usleep(20000);
        }
        ::_exit(0);
    }
    ::close(ends[0]);
    std::string const whole(1 << 22, 'x');
    auto const started = std::chrono::steady_clock::now();
    eo::detail::write_while_read(ends[1], whole, 500, 300);
    auto const waited = std::chrono::steady_clock::now() - started;
    EO_CHECK(waited >= std::chrono::milliseconds(300));
    EO_CHECK(waited < std::chrono::milliseconds(2000));
    ::close(ends[1]);
    int status = 0;
    ::waitpid(reader, &status, 0);
}
#endif

#if !defined(_WIN32)
EO_TEST(a_non_blocking_pipe_is_waited_on_both_ways) {
    ::signal(SIGPIPE, SIG_IGN);
    int towards[2] = {-1, -1};
    int back[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(towards), 0);
    EO_CHECK_EQ(::pipe(back), 0);
    ::fcntl(towards[1], F_SETFL, ::fcntl(towards[1], F_GETFL) | O_NONBLOCK);
    ::fcntl(back[0], F_SETFL, ::fcntl(back[0], F_GETFL) | O_NONBLOCK);
    pid_t const echo = ::fork();
    if (echo == 0) {
        ::close(towards[1]);
        ::close(back[0]);
        ::usleep(50000);
        long long total = 0;
        char chunk[4096];
        for (;;) {
            ssize_t const one = ::read(towards[0], chunk, sizeof(chunk));
            if (one <= 0) break;
            total += one;
        }
        std::string const said = std::to_string(total) + "\n";
        ::usleep(50000);
        ssize_t const told = ::write(back[1], said.data(), said.size());
        ::_exit(told == static_cast<ssize_t>(said.size()) ? 0 : 1);
    }
    ::close(towards[0]);
    ::close(back[1]);
    std::string const whole(1 << 18, 'x');
    bool deaf = false;
    eo::detail::source heard = eo::detail::source::over_descriptor(back[0], true, false);
    eo::detail::write_while_absorbing(
        towards[1], whole, heard, deaf, [] { return std::string("the child"); }, "test", "read between sends");
    EO_CHECK(!deaf);
    ::close(towards[1]);
    EO_CHECK_EQ(drain(heard), std::string("262144\n"));
    int status = 0;
    ::waitpid(echo, &status, 0);
    EO_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif

#if !defined(_WIN32)
EO_TEST(a_source_lets_its_descriptor_go_once_it_has_read_everything) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    EO_CHECK_EQ(::write(ends[1], "last\n", 5), static_cast<ssize_t>(5));
    ::close(ends[1]);
    eo::detail::source owned = eo::detail::source::over_descriptor(ends[0], true, false,
                                                                    eo::detail::source::pipe_chunk);
    EO_CHECK(::fcntl(ends[0], F_GETFD) >= 0);
    EO_CHECK_EQ(drain(owned), std::string("last\n"));
    EO_CHECK(::fcntl(ends[0], F_GETFD) < 0);
    EO_CHECK(owned.at_end());
}
#endif

EO_TEST(a_channel_is_read_through_a_64_kb_buffer) {
    std::string const path = temporary_path();
    std::FILE* const writing = std::fopen(path.c_str(), "wb");
    std::string const heard(300000, '7');
    std::fwrite(heard.data(), 1, heard.size(), writing);
    std::fclose(writing);
    eo::detail::source channel = eo::detail::source::over_channel(path.c_str());
    EO_CHECK_EQ(channel.peek(), static_cast<int>('7'));
    EO_CHECK_EQ(channel.held(), static_cast<std::size_t>(1) << 16);
    EO_CHECK_EQ(drain(channel), heard);
    ::unlink(path.c_str());
}

EO_TEST(a_source_absorbs_what_is_waiting_up_to_a_limit) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    eo::detail::source over = eo::detail::source::over_descriptor(ends[0], true, false, 4);
    EO_CHECK(over.absorb(100) == eo::detail::absorbed::nothing);
    EO_CHECK_EQ(::write(ends[1], "0123456789", 10), static_cast<ssize_t>(10));
    EO_CHECK(over.absorb(5) == eo::detail::absorbed::full);
    EO_CHECK(over.absorb(100) == eo::detail::absorbed::some);
    EO_CHECK_EQ(over.held(), static_cast<std::size_t>(10));
    EO_CHECK_EQ(over.take(), static_cast<int>('0'));
    EO_CHECK_EQ(::write(ends[1], "ab", 2), static_cast<ssize_t>(2));
    EO_CHECK(over.absorb(100) == eo::detail::absorbed::some);
    ::close(ends[1]);
    EO_CHECK_EQ(drain(over), std::string("123456789ab"));
    EO_CHECK(over.absorb(100) == eo::detail::absorbed::nothing);
    eo::detail::source text = eo::detail::source::over_text("abc", false);
    EO_CHECK(text.absorb(100) == eo::detail::absorbed::nothing);
}

EO_TEST(a_source_knows_how_much_of_a_file_is_left) {
    std::string const path = temporary_path();
    std::FILE* const writing = std::fopen(path.c_str(), "wb");
    std::fputs("1 2 3\n", writing);
    std::fclose(writing);
    eo::detail::source file = eo::detail::source::over_file(path.c_str(), true, 4);
    EO_CHECK_EQ(file.bytes_left(), 6LL);
    EO_CHECK_EQ(file.take(), static_cast<int>('1'));
    EO_CHECK_EQ(file.bytes_left(), 5LL);
    EO_CHECK_EQ(drain(file), std::string(" 2 3\n"));
    EO_CHECK_EQ(file.bytes_left(), 0LL);
    ::unlink(path.c_str());
    eo::detail::source text = eo::detail::source::over_text("abcdef", false, 4);
    EO_CHECK_EQ(text.take(), static_cast<int>('a'));
    EO_CHECK_EQ(text.bytes_left(), 5LL);
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    EO_CHECK_EQ(::write(ends[1], "1 2 3\n", 6), static_cast<ssize_t>(6));
    ::close(ends[1]);
    eo::detail::source raw = eo::detail::source::over_descriptor(ends[0], false, false);
    EO_CHECK_EQ(raw.bytes_left(), -1LL);
    eo::stream piped(eo::detail::source::over_descriptor(ends[0], true, false), eo::detail::fault::jury_error,
                     "pipe");
    EO_CHECK(piped.read_ints(3, 1, 9, "a") == std::vector<int>({1, 2, 3}));
}

EO_TEST(a_descriptor_that_cannot_be_read_is_a_library_error) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    eot::outcome const result = eot::run_until_stop([&] {
        eo::detail::source over = eo::detail::source::over_descriptor(ends[1], false, false);
        (void)over.peek();
    });
    EO_CHECK(result.stopped);
    EO_CHECK_IN(result.text, "cannot read the input");
    ::close(ends[0]);
    ::close(ends[1]);
}

#if !defined(_WIN32)
EO_TEST(a_read_cut_short_by_a_signal_is_resumed) {
    int ends[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(ends), 0);
    interrupted_writer = ends[1];
    struct sigaction wanted = {};
    struct sigaction previous = {};
    wanted.sa_handler = &answer_the_alarm;
    sigemptyset(&wanted.sa_mask);
    wanted.sa_flags = 0;
    ::sigaction(SIGALRM, &wanted, &previous);
    struct itimerval timer = {};
    timer.it_value.tv_usec = 50000;
    timer.it_interval.tv_usec = 50000;
    ::setitimer(ITIMER_REAL, &timer, nullptr);
    eo::detail::source over = eo::detail::source::over_descriptor(ends[0], true, false);
    EO_CHECK_EQ(over.peek(), static_cast<int>('x'));
    struct itimerval off = {};
    ::setitimer(ITIMER_REAL, &off, nullptr);
    ::sigaction(SIGALRM, &previous, nullptr);
    ::close(ends[1]);
}
#endif

EO_TEST(a_source_can_be_moved) {
    eo::detail::source first = eo::detail::source::over_text("abc", false);
    EO_CHECK_EQ(first.take(), static_cast<int>('a'));
    eo::detail::source second = std::move(first);
    EO_CHECK(first.at_end());
    EO_CHECK_EQ(second.take(), static_cast<int>('b'));
    eo::detail::source third = eo::detail::source::over_text("z", false);
    third = std::move(second);
    EO_CHECK_EQ(third.take(), static_cast<int>('c'));
    EO_CHECK(third.at_end());
    eo::detail::source* alias = &third;
    third = std::move(*alias);
    EO_CHECK(third.at_end());
}
