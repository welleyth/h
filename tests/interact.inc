namespace {

struct chat {
    bool stopped = false;
    int code = 0;
    std::string text;
    std::string said;
    std::string log;
    std::string summary;
};

struct swapped_descriptors {
    int kept[3] = {-1, -1, -1};

    swapped_descriptors() {
        for (int at = 0; at < 3; at++) kept[at] = ::dup(at);
    }

    swapped_descriptors(swapped_descriptors const&) = delete;

    ~swapped_descriptors() {
        for (int at = 0; at < 3; at++) {
            ::dup2(kept[at], at);
            ::close(kept[at]);
        }
    }
};

#if !defined(_WIN32)
class file_size_limit {
public:
    file_size_limit() {
        ::getrlimit(RLIMIT_FSIZE, &before_);
        std::signal(SIGXFSZ, SIG_IGN);
    }

    file_size_limit(file_size_limit const&) = delete;

    void cap(rlim_t bytes) {
        rlimit capped = before_;
        capped.rlim_cur = bytes;
        ::setrlimit(RLIMIT_FSIZE, &capped);
    }

    ~file_size_limit() {
        ::setrlimit(RLIMIT_FSIZE, &before_);
        std::signal(SIGXFSZ, SIG_DFL);
    }

private:
    rlimit before_{};
};
#endif

std::string slurp(std::string const& path) {
    std::FILE* const reading = std::fopen(path.c_str(), "rb");
    if (reading == nullptr) return {};
    std::string text;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), reading)) > 0) text.append(buffer, got);
    std::fclose(reading);
    return text;
}

template <class Body>
chat talk(std::string const& test, std::string const& from_solution, Body&& body, bool with_answer = false) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    ::setenv("TEST_COST", "40", 1);
    scratch_file input(test);
    scratch_file said(from_solution);
    scratch_file answer(with_answer ? "7\n" : "");
    std::string const out_path = temporary_path();
    std::string const to_solution = temporary_path();
    std::string const complaints = temporary_path();
    chat result;
    {
        swapped_descriptors kept;
        int const from = ::open(said.path.c_str(), O_RDONLY);
        int const to = ::open(to_solution.c_str(), O_WRONLY | O_TRUNC);
        int const grumbles = ::open(complaints.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(from, 0);
        ::dup2(to, 1);
        ::dup2(grumbles, 2);
        std::string program = "interactor.cpp";
        std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                     const_cast<char*>(out_path.c_str())};
        if (with_answer) arguments.push_back(const_cast<char*>(answer.path.c_str()));
        try {
            eo::interactor it(static_cast<int>(arguments.size()), arguments.data());
            body(it);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        std::fflush(stderr);
        ::close(from);
        ::close(to);
        ::close(grumbles);
    }
    result.said = slurp(to_solution);
    result.log = slurp(complaints);
    result.summary = slurp(out_path);
    ::unlink(out_path.c_str());
    ::unlink(to_solution.c_str());
    ::unlink(complaints.c_str());
    forget_the_environment();
    if (std::getenv("EOT_TRACE") != nullptr) std::fprintf(stderr, "   dialogue: %s\n", result.text.c_str());
    return result;
}

}  // namespace

EO_TEST(an_interactor_that_exits_before_its_verdict_is_a_jury_error) {
    chat const result = talk("100 42\n", "? 1\n", [](eo::interactor& it) {
        it.send(100);
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("jury error the interactor ended without a verdict: exit() was called, "
                                         "or the interactor was never destroyed"));
    EO_CHECK_EQ(result.said, std::string("100\n"));
}

EO_TEST(an_interactor_reads_a_tree_from_the_solution_and_blames_it) {
    chat const good = talk("3\n", "1 2\n1 3\n", [](eo::interactor& it) {
        std::vector<eo::edge> const edges = it.contestant.read_tree(it.input.read_int(1, 9, "n"), "edge");
        eo::accept("{} edges", edges.size());
    });
    EO_CHECK_EQ(good.code, 0);
    chat const bad = talk("3\n", "1 2\n2 1\n", [](eo::interactor& it) {
        it.contestant.read_tree(it.input.read_int(1, 9, "n"), "edge");
        eo::accept();
    });
    EO_CHECK_EQ(bad.code, 1);
    EO_CHECK_IN(bad.text, "wrong answer: the solution, line 2: edge: edges 1 and 2 are both (1, 2)");
}

EO_TEST(an_interactor_answers_and_accepts) {
    chat const result = talk("100 42\n", "? 50\n? 25\n! 42\n", [](eo::interactor& it) {
        int const n = it.input.read_int(1, 1000, "n");
        int const secret = it.input.read_int(1, n, "secret");
        eo::budget queries(it, 20, "queries");
        it.send(n);
        for (;;) {
            std::string const command = it.contestant.read_choice({"?", "!"}, "command");
            int const x = it.contestant.read_int(1, n, "x");
            if (command == "!") {
                if (x != secret) eo::wrong("answered {}, the number was {}", x, secret);
                EO_CHECK_EQ(queries.used(), 2LL);
                EO_CHECK_EQ(queries.left(), 18LL);
                eo::accept("{} queries", queries.used());
            }
            queries.spend();
            it.send(x < secret ? "<" : ">");
        }
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok 2 queries"));
    EO_CHECK_EQ(result.said, std::string("100\n>\n<\n"));
    EO_CHECK_IN(result.summary, "eolymp-summary 1");
    EO_CHECK_IN(result.summary, "fraction 1");
    EO_CHECK_IN(result.summary, "message 2 queries");
    EO_CHECK_IN(result.log, "ok 2 queries");
    EO_CHECK_IN(result.log, "round trips");
}

EO_TEST(an_interactor_refuses_a_score_that_is_not_a_number) {
    chat const result = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        it.pass(std::nan(""), "degenerate");
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "a score of nan is not a number the judge can pay");
    EO_CHECK_EQ(result.summary, std::string());
}

#if !defined(_WIN32)
EO_TEST(a_summary_that_cannot_be_written_whole_is_a_jury_error) {
    file_size_limit disk;
    chat const result = talk("1\n", "5\n", [&](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        disk.cap(27);
        eo::score(0.00001, "almost nothing");
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "eolymp.h: the summary could not be written to ");
    EO_CHECK_EQ(result.summary, std::string("eolymp-summary 1\nfraction 1"));
}
#endif

EO_TEST(an_interactor_can_award_part_of_the_test) {
    chat const half = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        it.value("quality", 12);
        eo::score(0.5, "half of it");
    });
    EO_CHECK_EQ(half.code, 0);
    EO_CHECK_IN(half.summary, "fraction 0.5");
    EO_CHECK_IN(half.summary, "value quality 12");
    EO_CHECK_IN(half.summary, "message half of it");
    chat const absolute = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        EO_CHECK(it.cost() == 40);
        eo::points(10, "ten points");
    });
    EO_CHECK_IN(absolute.summary, "fraction 0.25");
}

EO_TEST(a_summary_carries_only_numbers_the_checker_reads_back) {
    chat const zero = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        it.value("loss", -0.0);
        eo::score(-0.0, "nothing");
    });
    EO_CHECK_EQ(zero.summary, std::string("eolymp-summary 1\nfraction 0\nvalue loss 0\nmessage nothing\n"));
    for (double const value : {std::numeric_limits<double>::infinity(), std::nan("")}) {
        chat const odd = talk("1\n", "5\n", [&](eo::interactor& it) {
            it.input.read_int(1, 9, "n");
            it.contestant.read_int(1, 9, "x");
            it.value("ratio", value);
            eo::accept();
        });
        EO_CHECK_EQ(odd.code, 3);
        EO_CHECK_IN(odd.text, "eolymp.h: the value \"ratio\" is ");
        EO_CHECK_IN(odd.text, "which a summary cannot carry");
    }
    for (char const* name : {"", "two words", "line\nbreak", "tab\there", "del\x7f"}) {
        chat const named = talk("1\n", "5\n", [&](eo::interactor& it) {
            it.input.read_int(1, 9, "n");
            it.contestant.read_int(1, 9, "x");
            it.value(name, 1);
            eo::accept();
        });
        EO_CHECK_EQ(named.code, 3);
        EO_CHECK_IN(named.text, "is not one word; a summary needs a name with no spaces or line breaks in it");
    }
    chat const escaped = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        it.value("line\nbreak", 1);
        eo::accept();
    });
    EO_CHECK_IN(escaped.text, "the value name \"line\\x0abreak\"");
}

EO_TEST(points_above_the_cost_are_full_marks_in_the_summary) {
    chat const result = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::points(50, "more than the test");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.summary, std::string("eolymp-summary 1\nfraction 1\nmessage more than the test\n"));
    EO_CHECK_IN(checker_warnings(), "warning EO207: 50 points is more than the test's 40");
}

EO_TEST(an_interactor_refuses_what_the_solution_sends) {
    chat const early = talk("100 42\n", "", [](eo::interactor& it) {
        it.input.read_int(1, 1000, "n");
        it.contestant.read_int(1, 100, "x");
        eo::accept();
    });
    EO_CHECK_EQ(early.code, 1);
    EO_CHECK_EQ(early.text,
                std::string("wrong answer: the solution, line 1, x: the solution ended the dialogue early"));
    chat const outside = talk("100 42\n", "5000\n", [](eo::interactor& it) {
        it.input.read_int(1, 1000, "n");
        it.contestant.read_int(1, 100, "x");
        eo::accept();
    });
    EO_CHECK_EQ(outside.code, 1);
    EO_CHECK_IN(outside.text, "wrong answer: the solution, line 1, x: 5000 is above 100");
    chat const nonsense = talk("100 42\n", "maybe\n", [](eo::interactor& it) {
        it.input.read_int(1, 1000, "n");
        it.contestant.read_choice({"?", "!"}, "command");
        eo::accept();
    });
    EO_CHECK_IN(nonsense.text, "\"maybe\" is not one of ?, !");
}

EO_TEST(an_interactor_ends_the_run_itself) {
    chat const refused = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::wrong("that is not the answer");
    });
    EO_CHECK_EQ(refused.code, 1);
    EO_CHECK_EQ(refused.text, std::string("wrong answer that is not the answer"));
    EO_CHECK_EQ(refused.summary, std::string(""));
    chat const broken = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::jury_error("the test makes no sense");
    });
    EO_CHECK_EQ(broken.code, 3);
    EO_CHECK_EQ(broken.text, std::string("jury error the test makes no sense"));
    chat const nothing = talk("1\n", "5\n", [](eo::interactor& it) { it.input.read_int(1, 9, "n"); });
    EO_CHECK_EQ(nothing.code, 3);
    EO_CHECK_IN(nothing.text, "the interactor ended without a verdict");
    EO_CHECK_IN(nothing.log, "the interactor ended without a verdict");
    EO_CHECK_EQ(nothing.said, std::string(""));
    chat const silent = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::wrong();
    });
    EO_CHECK_IN(checker_warnings(), "warning EO204: this wrong answer carries no message");
    EO_CHECK_EQ(silent.code, 1);
}

EO_TEST(a_budget_is_spent_before_the_reply) {
    chat const over = talk("100 42\n", "? 1\n? 2\n? 3\n", [](eo::interactor& it) {
        it.input.read_int(1, 1000, "n");
        eo::budget queries(it, 2, "queries");
        for (;;) {
            it.contestant.read_choice({"?", "!"}, "command");
            it.contestant.read_int(1, 100, "x");
            queries.spend();
            it.send("<");
        }
    });
    EO_CHECK_EQ(over.code, 1);
    EO_CHECK_EQ(over.text, std::string("wrong answer more than 2 queries"));
    EO_CHECK_EQ(over.said, std::string("<\n<\n"));
    chat const bulk = talk("100 42\n", "? 1\n", [](eo::interactor& it) {
        it.input.read_int(1, 1000, "n");
        eo::budget queries(it, 10, "queries");
        it.contestant.read_choice({"?", "!"}, "command");
        it.contestant.read_int(1, 100, "x");
        queries.spend(3);
        EO_CHECK_EQ(queries.used(), 3LL);
        eo::accept("{} spent", queries.used());
    });
    EO_CHECK_EQ(bulk.code, 0);
}

EO_TEST(a_last_word_before_the_verdict_reaches_the_solution) {
    chat const told = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.send("guess");
        it.contestant.read_int(1, 9, "x");
        it.send("correct");
        eo::accept();
    });
    EO_CHECK_EQ(told.code, 0);
    EO_CHECK_EQ(told.said, std::string("guess\ncorrect\n"));

    chat const refused = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        it.send("-1");
        eo::wrong("five is not it");
    });
    EO_CHECK_EQ(refused.code, 1);
    EO_CHECK_EQ(refused.said, std::string("-1\n"));
}

EO_TEST(a_budget_starts_over_for_each_round) {
    chat const rounds = talk("3\n", "? 1\n? 2\n? 3\n? 4\n? 5\n? 6\n", [](eo::interactor& it) {
        long long const hunts = it.input.read_int(1, 9, "hunts");
        eo::budget queries(it, 2, "queries");
        for (long long hunt = 0; hunt < hunts; hunt++) {
            queries.restart();
            EO_CHECK_EQ(queries.used(), 0LL);
            EO_CHECK_EQ(queries.left(), 2LL);
            for (long long at = 0; at < 2; at++) {
                it.contestant.read_choice({"?"}, "command");
                it.contestant.read_int(1, 9, "x");
                queries.spend();
                it.send("<");
            }
            EO_CHECK_EQ(queries.used(), 2LL);
        }
        eo::accept("{} hunts", hunts);
    });
    EO_CHECK_EQ(rounds.code, 0);
    EO_CHECK_EQ(rounds.text, std::string("ok 3 hunts"));

    chat const widened = talk("1\n", "? 1\n? 2\n? 3\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::budget queries(it, 1, "queries");
        EO_CHECK_EQ(queries.limit(), 1LL);
        queries.spend();
        queries.restart(3);
        EO_CHECK_EQ(queries.limit(), 3LL);
        EO_CHECK_EQ(queries.used(), 0LL);
        for (long long at = 0; at < 3; at++) {
            it.contestant.read_choice({"?"}, "command");
            it.contestant.read_int(1, 9, "x");
            queries.spend();
        }
        eo::accept("{} of {}", queries.used(), queries.limit());
    });
    EO_CHECK_EQ(widened.code, 0);
    EO_CHECK_EQ(widened.text, std::string("ok 3 of 3"));

    chat const beyond = talk("1\n", "? 1\n? 2\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::budget queries(it, 5, "queries");
        queries.restart(1);
        for (;;) {
            it.contestant.read_choice({"?"}, "command");
            it.contestant.read_int(1, 9, "x");
            queries.spend();
        }
    });
    EO_CHECK_EQ(beyond.code, 1);
    EO_CHECK_EQ(beyond.text, std::string("wrong answer more than 1 queries"));
}

EO_TEST(an_interactor_is_told_when_it_judges_nothing) {
    chat const blind = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::accept("looks fine");
    });
    EO_CHECK_EQ(blind.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO405: the interactor accepted without reading anything");
    chat const unused = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        EO_CHECK(it.has_jury());
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    }, true);
    EO_CHECK_EQ(unused.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO406: the test has an answer file that the interactor never read");
    chat const used = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        EO_CHECK_EQ(it.jury.read_int(1, 9, "a"), 7);
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    }, true);
    EO_CHECK_EQ(used.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO406"));
}

EO_TEST(a_solution_that_stopped_talking_is_not_reported_as_talking) {
    chat const tidy = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(tidy.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO404"));

    chat const ended = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        EO_CHECK(it.contestant.at_eof());
        eo::accept();
    });
    EO_CHECK_EQ(ended.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO404"));

    chat const extra = talk("1\n", "5\n9\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(extra.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO404: the solution was still sending");

    chat const padded = talk("1\n", "5\n" + std::string(9000, ' ') + "9\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(padded.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO404: the solution was still sending");
}

EO_TEST(a_budget_nobody_spends_is_reported) {
    chat const idle = talk("1\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::budget queries(it, 10, "queries");
        it.contestant.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(idle.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO403: a budget was declared and never spent");
    std::string asked;
    for (long long at = 0; at < 10002; at++) asked += "1\n";
    chat const counted = talk("1\n", asked, [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        eo::budget limited(it, 1000000, "queries");
        for (long long at = 0; at < 10002; at++) {
            it.contestant.read_int(1, 9, "x");
            limited.spend();
            it.send(at);
        }
        eo::accept();
    });
    EO_CHECK_EQ(counted.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO402"));
    chat const unlimited = talk("1\n", asked, [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        for (long long at = 0; at < 10002; at++) {
            it.contestant.read_int(1, 9, "x");
            it.send(at);
        }
        eo::accept();
    });
    EO_CHECK_EQ(unlimited.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO402: 10001 round trips were answered with no eo::budget");
}

EO_TEST(the_random_stream_comes_from_the_test) {
    chat const first = talk("12345\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 99999, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept("{} {}", it.rng().uniform(1, 1000000), it.rng().uniform(1, 1000000));
    });
    chat const again = talk("12345\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 99999, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept("{} {}", it.rng().uniform(1, 1000000), it.rng().uniform(1, 1000000));
    });
    chat const other = talk("54321\n", "5\n", [](eo::interactor& it) {
        it.input.read_int(1, 99999, "n");
        it.contestant.read_int(1, 9, "x");
        eo::accept("{} {}", it.rng().uniform(1, 1000000), it.rng().uniform(1, 1000000));
    });
    EO_CHECK_EQ(first.text, again.text);
    EO_CHECK(first.text != other.text);
}

#if !defined(_WIN32)
namespace {

int draining_pipe = -1;
volatile std::sig_atomic_t drained_bytes = 0;

// The pipe's only writer is the thread this handler interrupts, so a read that
// waited here for bytes would wait for ever: the pipe is drained without waiting.
void drain_the_pipe(int) {
    char sink[1 << 15];
    ssize_t const got = ::read(draining_pipe, sink, sizeof(sink));
    if (got > 0) drained_bytes = drained_bytes + static_cast<std::sig_atomic_t>(got);
}

}  // namespace

namespace {

template <class Body>
chat talk_to_the_deaf(std::string const& from_solution, Body&& body) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    scratch_file input("1\n");
    scratch_file said(from_solution);
    std::string const out_path = temporary_path();
    chat result;
    {
        swapped_descriptors kept;
        int ends[2] = {-1, -1};
        EO_CHECK_EQ(::pipe(ends), 0);
        ::close(ends[0]);
        int const from = ::open(said.path.c_str(), O_RDONLY);
        int const quiet = ::open("/dev/null", O_WRONLY);
        ::dup2(from, 0);
        ::dup2(ends[1], 1);
        ::dup2(quiet, 2);
        std::string program = "interactor.cpp";
        std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                     const_cast<char*>(out_path.c_str())};
        try {
            eo::interactor it(static_cast<int>(arguments.size()), arguments.data());
            body(it);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        ::close(from);
        ::close(quiet);
        ::close(ends[1]);
    }
    ::unlink(out_path.c_str());
    forget_the_environment();
    return result;
}

}  // namespace
#endif

#if !defined(_WIN32)
EO_TEST(a_solution_that_stopped_reading_is_judged_by_what_it_said) {
    chat const finished = talk_to_the_deaf("5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.send("are you there");
        it.flush();
        EO_CHECK_EQ(it.contestant.read_int(1, 9, "x"), 5);
        it.send("OK");
        if (!it.contestant.at_eof()) eo::wrong("extra output");
        eo::accept("heard 5");
    });
    EO_CHECK_EQ(finished.code, 0);
    EO_CHECK_EQ(finished.text, std::string("ok heard 5"));
    chat const early = talk_to_the_deaf("5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.send("first");
        it.contestant.read_int(1, 9, "x");
        it.send("second");
        it.contestant.read_int(1, 9, "y");
        eo::accept("never reached");
    });
    EO_CHECK_EQ(early.code, 1);
    EO_CHECK_EQ(early.text,
                std::string("wrong answer: the solution, line 2, y: the solution ended the dialogue early"));
    chat const verdict = talk_to_the_deaf("5\n", [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        it.send("the last line");
        it.contestant.read_int(1, 9, "x");
        it.send("never written");
        eo::wrong("a verdict with output still waiting");
    });
    EO_CHECK_EQ(verdict.code, 1);
    EO_CHECK_EQ(verdict.text, std::string("wrong answer a verdict with output still waiting"));
}

namespace {

template <class Body>
chat against_a_forked_solution(std::string const& test, void (*solve)(std::FILE*, std::FILE*), Body&& body) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    scratch_file input(test);
    std::string const out_path = temporary_path();
    std::string const log_path = temporary_path();
    int towards[2] = {-1, -1};
    int back[2] = {-1, -1};
    EO_CHECK_EQ(::pipe(towards), 0);
    EO_CHECK_EQ(::pipe(back), 0);
    pid_t const solution = ::fork();
    if (solution == 0) {
        ::close(towards[1]);
        ::close(back[0]);
        solve(::fdopen(towards[0], "r"), ::fdopen(back[1], "w"));
        ::_exit(0);
    }
    ::close(towards[0]);
    ::close(back[1]);
    chat result;
    {
        swapped_descriptors kept;
        int const quiet = ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        ::dup2(back[0], 0);
        ::dup2(towards[1], 1);
        ::dup2(quiet, 2);
        ::close(back[0]);
        ::close(towards[1]);
        std::string program = "interactor.cpp";
        std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                     const_cast<char*>(out_path.c_str())};
        ::alarm(20);
        try {
            eo::interactor it(static_cast<int>(arguments.size()), arguments.data());
            body(it);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        ::alarm(0);
        std::fflush(stdout);
        ::close(quiet);
        ::close(0);
        ::close(1);
    }
    int status = 0;
    ::waitpid(solution, &status, 0);
    ::unlink(out_path.c_str());
    result.log = slurp(log_path);
    ::unlink(log_path.c_str());
    forget_the_environment();
    return result;
}

void answer_each_line(std::FILE* questions, std::FILE* answers) {
    long long x = 0;
    while (std::fscanf(questions, "%lld", &x) == 1) {
        std::fprintf(answers, "%lld\n", x + 1);
        std::fflush(answers);
    }
}

void add_slowly(std::FILE* numbers, std::FILE* answers) {
    long long count = 0;
    long long sum = 0;
    long long x = 0;
    if (std::fscanf(numbers, "%lld", &count) != 1) return;
    for (long long at = 0; at < count && std::fscanf(numbers, "%lld", &x) == 1; at++) {
        sum += x;
        if (at % 20000 == 0) ::usleep(20000);
    }
    std::fprintf(answers, "%lld\n", sum);
    std::fflush(answers);
}

void flood_without_reading(std::FILE*, std::FILE* answers) {
    int const out = ::fileno(answers);
    ::fcntl(out, F_SETFL, ::fcntl(out, F_GETFL) | O_NONBLOCK);
    std::string const block(4096, '7');
    pollfd room{out, POLLOUT, 0};
    for (std::size_t sent = 0; sent <= (std::size_t{1} << 24);) {
        ssize_t const wrote = ::write(out, block.data(), block.size());
        if (wrote > 0) {
            sent += static_cast<std::size_t>(wrote);
            continue;
        }
        if (errno == EINTR) continue;
        if (errno != EAGAIN || ::poll(&room, 1, 8000) <= 0) return;
    }
}

}  // namespace
#endif

#if !defined(_WIN32)
EO_TEST(an_interactor_stops_taking_in_answers_at_16_mb) {
    chat const result = against_a_forked_solution("1\n", &flood_without_reading, [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        for (int at = 0; at < 100000; at++) it.send(at);
        it.flush();
        eo::wrong("the solution never read");
    });
    EO_CHECK_EQ(result.code, 1);
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                "warning EO409: the solution sent more than 16 MB while the interactor was still writing to it");
    EO_CHECK_IN(result.log, " the solution sent more than 16 MB while the interactor was still writing to it");
    EO_CHECK_EQ(result.log.rfind("warning EO409 ", 0), std::size_t{0});
}
#endif

#if !defined(_WIN32)
EO_TEST(an_interactor_that_sends_a_lot_hears_the_answers_meanwhile) {
    chat const result = against_a_forked_solution("100000\n", &answer_each_line, [](eo::interactor& it) {
        int const n = it.input.read_int(1, 100000, "n");
        for (int at = 1; at <= n; at++) it.send(at);
        it.flush();
        for (int at = 1; at <= n; at++)
            if (it.contestant.read_long(eo::any, "y") != at + 1) eo::wrong("answer {} is off", at);
        eo::accept("{} answers", n);
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok 100000 answers"));
}
#endif

#if !defined(_WIN32)
EO_TEST(an_interactor_that_prints_on_its_own_still_reaches_a_slow_reader) {
    chat const result = against_a_forked_solution("100000\n", &add_slowly, [](eo::interactor& it) {
        int const n = it.input.read_int(1, 100000, "n");
        std::printf("%d\n", n);
        for (int at = 1; at <= n; at++) std::printf("%d\n", at);
        std::fflush(stdout);
        long long const sum = it.contestant.read_long(eo::any, "sum");
        if (sum != 5000050000LL) eo::wrong("the sum is {}", sum);
        eo::accept("the sum is right");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok the sum is right"));
}
#endif

#if !defined(_WIN32)
EO_TEST(a_write_cut_short_by_a_signal_is_resumed) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    scratch_file input("1\n");
    scratch_file said("5\n");
    std::string const out_path = temporary_path();
    bool finished = false;
    {
        swapped_descriptors kept;
        int ends[2] = {-1, -1};
        EO_CHECK_EQ(::pipe(ends), 0);
        ::fcntl(ends[0], F_SETFL, ::fcntl(ends[0], F_GETFL) | O_NONBLOCK);
        draining_pipe = ends[0];
        drained_bytes = 0;
        struct sigaction wanted = {};
        struct sigaction previous = {};
        wanted.sa_handler = &drain_the_pipe;
        sigemptyset(&wanted.sa_mask);
        wanted.sa_flags = 0;
        ::sigaction(SIGALRM, &wanted, &previous);
        struct itimerval timer = {};
        timer.it_value.tv_usec = 20000;
        timer.it_interval.tv_usec = 20000;
        ::setitimer(ITIMER_REAL, &timer, nullptr);
        int const from = ::open(said.path.c_str(), O_RDONLY);
        int const quiet = ::open("/dev/null", O_WRONLY);
        ::dup2(from, 0);
        ::dup2(ends[1], 1);
        ::dup2(quiet, 2);
        std::string program = "interactor.cpp";
        std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                     const_cast<char*>(out_path.c_str())};
        try {
            eo::interactor it(static_cast<int>(arguments.size()), arguments.data());
            it.input.read_int(1, 9, "n");
            it.send(std::string(400000, 'x'));
            it.flush();
            finished = true;
            eo::accept("sent it all");
        } catch (eo::detail::stop const&) {
        }
        struct itimerval off = {};
        ::setitimer(ITIMER_REAL, &off, nullptr);
        ::sigaction(SIGALRM, &previous, nullptr);
        for (long before = -1; before != drained_bytes;) {
            before = drained_bytes;
            drain_the_pipe(0);
        }
        ::close(from);
        ::close(quiet);
        ::close(ends[0]);
        ::close(ends[1]);
    }
    ::unlink(out_path.c_str());
    forget_the_environment();
    EO_CHECK(finished);
    EO_CHECK_EQ(static_cast<long>(drained_bytes), 400001L);
}
#endif

EO_TEST(an_interactor_needs_its_files_and_only_one_role) {
    forget_the_environment();
    std::string program = "interactor.cpp";
    std::vector<char*> alone{program.data()};
    eot::outcome const bare = eot::run_until_stop([&] {
        eo::interactor it(static_cast<int>(alone.size()), alone.data());
        (void)it.cost();
    });
    EO_CHECK_IN(bare.text, "an interactor needs the test and a file for its summary");
    scratch_file input("1\n");
    std::string const out_path = temporary_path();
    std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                 const_cast<char*>(out_path.c_str())};
    eot::outcome const twice = eot::run_until_stop([&] {
        eo::interactor first(static_cast<int>(arguments.size()), arguments.data());
        eo::interactor second(static_cast<int>(arguments.size()), arguments.data());
        (void)second.cost();
        eo::accept();
    });
    EO_CHECK_IN(twice.text, "this program already has an interactor");
    ::unlink(out_path.c_str());
}

EO_TEST(a_dialogue_the_pipe_cannot_carry_is_reported) {
    std::string many;
    for (long long at = 0; at < 100002; at++) many += "1\n";
    chat const noted = talk("1\n", many, [](eo::interactor& it) {
        it.input.read_int(1, 9, "n");
        for (long long at = 0; at < 100002; at++) {
            it.contestant.read_int(1, 9, "x");
            it.send(at);
        }
        eo::accept("{} round trips", it.round_trips());
    });
    EO_CHECK_EQ(noted.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO401: this run made 100001 round trips");
}

EO_TEST(an_exception_nothing_caught_ends_an_interactor_as_a_jury_error) {
    chat const ended = talk("5\n", "", [](eo::interactor&) {
        try {
            throw std::runtime_error("the interactor lost count");
        } catch (...) {
            eo::detail::scorer::ended_by_terminate();
        }
    });
    EO_CHECK_EQ(ended.code, 3);
    EO_CHECK_EQ(ended.text, std::string("jury error an exception nothing caught ended the interactor: the interactor "
                                        "lost count"));
}
