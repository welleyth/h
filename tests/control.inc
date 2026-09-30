namespace {

struct staged {
    bool stopped = false;
    int code = 0;
    std::string text;
    std::string asked;
    std::string said;
    std::string log;
    std::string summary;
};

int& deaf_reader() {
    static int descriptor = -1;
    return descriptor;
}

void stop_listening() {
    if (deaf_reader() >= 0) ::close(deaf_reader());
    deaf_reader() = -1;
}

template <class Body>
staged run_control(std::string const& test, std::vector<std::string> const& replies,
                   std::vector<std::string> const& heard, long long limit, Body&& body,
                   bool with_answer = false) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    ::setenv("TEST_COST", "40", 1);
    ::setenv("INSTANCE_LIMIT", std::to_string(limit).c_str(), 1);
    scratch_file input(test);
    scratch_file answer(with_answer ? "7\n" : "");
    std::string const out_path = temporary_path();
    std::string const requests = temporary_path();
    std::string const complaints = temporary_path();
    std::vector<std::string> towards;
    std::string filled;
    for (std::size_t at = 0; at < replies.size(); at++) {
        if (replies[at] == "LIMIT" || replies[at] == "NONSENSE") {
            filled += replies[at] + "\n";
            continue;
        }
        towards.push_back(temporary_path());
#if !defined(_WIN32)
        if (replies[at] == "DEAF") {
            ::unlink(towards.back().c_str());
            EO_CHECK_EQ(::mkfifo(towards.back().c_str(), 0600), 0);
            deaf_reader() = ::open(towards.back().c_str(), O_RDONLY | O_NONBLOCK);
        }
#endif
        std::string const back = temporary_path();
        three_files::put(back, at < heard.size() ? heard[at] : std::string());
        towards.push_back(back);
        filled += towards[towards.size() - 2] + " " + back + "\n";
    }
    std::string const answers = temporary_path();
    three_files::put(answers, filled);
    ::setenv("CONTROL_OUTPUT_FILE", requests.c_str(), 1);
    ::setenv("CONTROL_INPUT_FILE", answers.c_str(), 1);
    staged result;
    {
        swapped_descriptors kept;
        int const grumbles = ::open(complaints.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(grumbles, 2);
        std::string program = "controller.cpp";
        std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                     const_cast<char*>(out_path.c_str())};
        if (with_answer) arguments.push_back(const_cast<char*>(answer.path.c_str()));
        try {
            eo::controller ctl(static_cast<int>(arguments.size()), arguments.data());
            body(ctl);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        std::fflush(stderr);
        ::close(grumbles);
    }
    result.asked = slurp(requests);
    result.log = slurp(complaints);
    result.summary = slurp(out_path);
    stop_listening();
    for (std::size_t at = 0; at + 1 < towards.size(); at += 2) {
        struct stat seen {};
        if (::stat(towards[at].c_str(), &seen) == 0 && S_ISREG(seen.st_mode)) result.said += slurp(towards[at]);
    }
    for (std::string const& one : towards) ::unlink(one.c_str());
    ::unlink(out_path.c_str());
    ::unlink(requests.c_str());
    ::unlink(answers.c_str());
    ::unlink(complaints.c_str());
    ::unsetenv("CONTROL_OUTPUT_FILE");
    ::unsetenv("CONTROL_INPUT_FILE");
    forget_the_environment();
    if (std::getenv("EOT_TRACE") != nullptr) std::fprintf(stderr, "   control: %s\n", result.text.c_str());
    return result;
}

}  // namespace

EO_TEST(a_controller_starts_instances_and_relays_between_them) {
    staged const result = run_control("3 12345\n", {"ok", "ok", "ok"}, {"abc\n", "abc\n", "12345\n"}, 10,
                                      [](eo::controller& ctl) {
        int const k = ctl.input.read_int(2, 100, "instances");
        long long const secret = ctl.input.read_long(eo::any, "secret");
        EO_CHECK_EQ(ctl.instance_limit(), 10LL);
        eo::channel& first = ctl.spawn();
        EO_CHECK_EQ(first.index(), 1LL);
        first.send("first", secret);
        std::string message = first.read_token(1, 20, eo::charset("a-z"), "message");
        first.close();
        for (int at = 2; at < k; at++) {
            eo::channel& middle = ctl.spawn();
            middle.send("middle", message);
            message = middle.read_token(1, 20, eo::charset("a-z"), "message");
            middle.close();
        }
        eo::channel& last = ctl.spawn();
        last.send("last", message);
        long long const said = last.read_long(eo::any, "secret");
        if (said != secret) eo::wrong("the last said {}", said);
        eo::accept("the secret crossed {} instances", k);
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok the secret crossed 3 instances"));
    EO_CHECK_EQ(result.asked, std::string("SPAWN\nSPAWN\nSPAWN\n"));
    EO_CHECK_EQ(result.said, std::string("first 12345\nmiddle abc\nlast abc\n"));
    EO_CHECK_IN(result.summary, "fraction 1");
    EO_CHECK_IN(result.log, "3 instances, 3 round trips");
}

EO_TEST(an_instance_beyond_the_limit_is_the_problem_not_the_solution) {
    staged const result = run_control("3 1\n", {"LIMIT"}, {}, 2, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        one.send("hello");
        eo::accept();
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "the judge refused instance 1; instance_limit is 2");
    EO_CHECK_IN(result.text, "control.inc:");
}

EO_TEST(a_controller_blames_the_instance_it_was_reading) {
    staged const nonsense = run_control("3 1\n", {"ok"}, {"maybe\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        one.send("go");
        one.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(nonsense.code, 1);
    EO_CHECK_IN(nonsense.text, "wrong answer: instance 1, line 1, x: expected an integer, found \"maybe\"");
    staged const silent = run_control("3 1\n", {"ok"}, {""}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        one.send("go");
        one.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(silent.code, 1);
    EO_CHECK_IN(silent.text, "instance 1 ended the dialogue early");
}

EO_TEST(every_channel_is_flushed_before_the_controller_waits_on_any) {
    staged const result = run_control("3 1\n", {"ok", "ok"}, {"5\n", "6\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        eo::channel& two = ctl.spawn();
        one.send("to one");
        two.send("to two");
        EO_CHECK_EQ(one.read_int(1, 9, "x"), 5);
        EO_CHECK_EQ(two.read_int(1, 9, "x"), 6);
        eo::accept("both answered");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.said, std::string("to one\nto two\n"));
    EO_CHECK_IN(result.log, "2 instances, 1 round trips");
}

EO_TEST(a_controller_refuses_a_score_that_is_not_a_number) {
    staged const result = run_control("1\n", {}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        ctl.pass(-std::nan(""), "degenerate");
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "a score of -nan is not a number the judge can pay");
}

#if !defined(_WIN32)
EO_TEST(a_controller_summary_that_cannot_be_written_whole_is_a_jury_error) {
    file_size_limit disk;
    staged const result = run_control("1\n", {}, {}, 10, [&](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        disk.cap(27);
        eo::score(0.00001, "almost nothing");
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "eolymp.h: the summary could not be written to ");
    EO_CHECK_EQ(result.summary, std::string("eolymp-summary 1\nfraction 1"));
}
#endif

EO_TEST(a_controller_writes_points_above_the_cost_as_full_marks) {
    staged const result = run_control("1\n", {}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::points(50, "more than the test");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(result.summary, "fraction 1\n");
}

#if !defined(_WIN32)
EO_TEST(an_instance_that_stopped_reading_is_judged_by_what_it_said) {
    staged const closed = run_control("1\n", {"DEAF"}, {"7\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& one = ctl.spawn();
        stop_listening();
        one.send("go");
        EO_CHECK_EQ(one.read_long(eo::any, "x"), 7LL);
        one.send("thanks");
        one.close();
        eo::accept("heard 7");
    });
    EO_CHECK_EQ(closed.code, 0);
    EO_CHECK_EQ(closed.text, std::string("ok heard 7"));
    staged const early = run_control("1\n", {"DEAF"}, {"7\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& one = ctl.spawn();
        stop_listening();
        one.send("go");
        one.read_long(eo::any, "x");
        one.send("again");
        one.read_long(eo::any, "y");
        eo::accept("never reached");
    });
    EO_CHECK_EQ(early.code, 1);
    EO_CHECK_IN(early.text, "instance 1 ended the dialogue early");
    staged const left = run_control("1\n", {"DEAF"}, {"7\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& one = ctl.spawn();
        stop_listening();
        one.send("go");
        one.read_long(eo::any, "x");
        one.send("unsent");
        eo::accept("with a line left over");
    });
    EO_CHECK_EQ(left.code, 0);
}
#endif

EO_TEST(a_controller_can_award_part_of_the_test) {
    staged const result = run_control("3 1\n", {"ok"}, {"5\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        one.send("go");
        EO_CHECK_EQ(one.read_long(eo::any, "x"), 5LL);
        ctl.value("right", 2);
        eo::score(eo::ratio(2, 3), "2 of 3 instances named the maximum");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(result.summary, "fraction 0.666");
    EO_CHECK_IN(result.summary, "value right 2");
}

EO_TEST(a_controller_reports_what_it_never_used) {
    staged const idle = run_control("3 1\n", {"ok", "ok"}, {"5\n", "5\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        (void)ctl.spawn();
        one.send("go");
        one.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(idle.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO408: instance 2 was started and never talked to");
    staged const deaf = run_control("3 1\n", {"ok"}, {"5\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        one.send("go");
        eo::accept("never listened");
    });
    EO_CHECK_EQ(deaf.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO405: the controller accepted without reading anything");
    staged const unspent = run_control("3 1\n", {"ok"}, {"5\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::budget queries(ctl, 10, "queries");
        eo::channel& one = ctl.spawn();
        one.send("go");
        one.read_int(1, 9, "x");
        eo::accept();
    });
    EO_CHECK_EQ(unspent.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO403: a budget was declared and never spent");
}

EO_TEST(a_controller_that_exits_before_its_verdict_is_a_jury_error) {
    staged const result = run_control("7\n", {}, {}, 5, [](eo::controller& ctl) {
        (void)ctl.input.read_int(eo::any, "n");
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("jury error the controller ended without a verdict: exit() was called, "
                                         "or the controller was never destroyed"));
}

EO_TEST(a_budget_limits_one_instance) {
    staged const result = run_control("3 1\n", {"ok"}, {"1\n2\n3\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::channel& one = ctl.spawn();
        eo::budget queries(ctl, 2, "queries");
        for (;;) {
            one.send("?");
            one.read_int(1, 9, "x");
            queries.spend();
        }
    });
    EO_CHECK_EQ(result.code, 1);
    EO_CHECK_EQ(result.text, std::string("wrong answer more than 2 queries"));
}

EO_TEST(a_controller_needs_a_control_channel_and_only_one_role) {
    forget_the_environment();
    scratch_file input("3 1\n");
    std::string const out_path = temporary_path();
    std::string program = "controller.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(input.path.c_str()),
                                 const_cast<char*>(out_path.c_str())};
    eot::outcome const lonely = eot::run_until_stop([&] {
        eo::controller ctl(static_cast<int>(arguments.size()), arguments.data());
        (void)ctl.spawn();
        eo::accept();
    });
    EO_CHECK_IN(lonely.text, "there is no control channel");
    std::vector<char*> bare{program.data()};
    eot::outcome const naked = eot::run_until_stop([&] {
        eo::controller ctl(static_cast<int>(bare.size()), bare.data());
        (void)ctl.cost();
    });
    EO_CHECK_IN(naked.text, "a controller needs the test and a file for its summary");
    eot::outcome const twice = eot::run_until_stop([&] {
        eo::controller first(static_cast<int>(arguments.size()), arguments.data());
        eo::controller second(static_cast<int>(arguments.size()), arguments.data());
        (void)second.cost();
        eo::accept();
    });
    EO_CHECK_IN(twice.text, "this program already has a controller");
    ::unlink(out_path.c_str());
}

EO_TEST(a_controller_ends_the_test_itself) {
    staged const nothing = run_control("3 1\n", {}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
    });
    EO_CHECK_EQ(nothing.code, 3);
    EO_CHECK_IN(nothing.text, "the controller ended without a verdict");
    staged const broken = run_control("3 1\n", {}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        EO_CHECK(!ctl.has_jury());
        eo::jury_error("the test makes no sense");
    });
    EO_CHECK_EQ(broken.code, 3);
    staged const quiet = run_control("3 1\n", {}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        eo::wrong();
    });
    EO_CHECK_EQ(quiet.code, 1);
    EO_CHECK_IN(checker_warnings(), "warning EO204: this wrong answer carries no message");
    staged const answered = run_control("3 1\n", {}, {}, 10, [](eo::controller& ctl) {
        EO_CHECK(ctl.has_jury());
        EO_CHECK_EQ(ctl.jury.read_int(1, 9, "a"), 7);
        ctl.input.read_int(2, 100, "instances");
        EO_CHECK(ctl.rng().uniform(1, 1000000) == ctl.rng().uniform(1, 1000000) ||
                 ctl.round_trips() == 0);
        eo::accept("nothing to do");
    }, true);
    EO_CHECK_EQ(answered.code, 0);
}

EO_TEST(a_dialogue_the_pipes_cannot_carry_is_reported) {
    std::string many;
    for (long long at = 0; at < 100002; at++) many += "1\n";
    staged const result = run_control("3 1\n", {"ok"}, {many}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        EO_CHECK(ctl.cost() == 40);
        eo::channel& one = ctl.spawn();
        for (long long at = 0; at < 100002; at++) {
            one.send(at);
            one.read_int(1, 9, "x");
        }
        eo::accept("{} round trips", ctl.round_trips());
    });
    EO_CHECK_EQ(result.code, 0);
    std::string const said = checker_warnings();
    EO_CHECK_IN(said, "note EO401: this run made 100002 round trips");
    EO_CHECK_IN(said, "warning EO402: 100002 round trips were answered with no eo::budget declared");
}

EO_TEST(a_channel_reads_what_an_instance_says_as_a_stream_does) {
    staged const result = run_control("1\n", {"ok"}, {"5 2.5 YES 1 2 3 tail\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& only = ctl.spawn();
        only.send("go");
        EO_CHECK_EQ(only.read_long(1, 9, "a"), 5LL);
        EO_CHECK(only.read_real(0.0, 9.0, "r") == 2.5);
        EO_CHECK_EQ(only.read_choice({"YES", "NO"}, "c"), std::string("YES"));
        EO_CHECK(only.read_longs(3, 1, 9, "v") == std::vector<long long>({1, 2, 3}));
        EO_CHECK(!only.at_eof());
        EO_CHECK_EQ(only.from().read_token(eo::any, "t"), std::string("tail"));
        EO_CHECK(only.at_eof());
        eo::accept("heard it all");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok heard it all"));
    staged const outside = run_control("1\n", {"ok"}, {"10\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& only = ctl.spawn();
        only.send("go");
        only.read_long(1, 9, "a");
        eo::accept("never reached");
    });
    EO_CHECK_EQ(outside.code, 1);
    EO_CHECK_EQ(outside.text, std::string("wrong answer: instance 1, line 1, a: 10 is above 9"));
}

EO_TEST(a_controller_stops_taking_in_an_instance_at_16_mb) {
    std::string flood;
    while (flood.size() <= (std::size_t{1} << 24)) flood += "7\n";
    staged const result = run_control("1\n", {"ok", "ok"}, {"1\n", flood}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& first = ctl.spawn();
        eo::channel& second = ctl.spawn();
        first.send("quiet");
        first.read_int(1, 9, "x");
        second.send("loud");
        second.read_int(1, 9, "x");
        eo::accept("heard both");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                "warning EO409: instance 2 sent more than 16 MB while the controller was still writing to it, "
                "and the rest of it waits in the pipe");
    EO_CHECK_EQ(result.log.rfind("warning EO409 ", 0), std::size_t{0});
    EO_CHECK_IN(result.log,
                " instance 2 sent more than 16 MB while the controller was still writing to it, and the rest "
                "of it waits in the pipe\n");
    EO_CHECK(result.log.find("instance 1 sent") == std::string::npos);
}

EO_TEST(a_judge_that_answers_nonsense_is_a_jury_error) {
    staged const odd = run_control("3 1\n", {"ok"}, {"5\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        (void)ctl.spawn();
        (void)ctl.spawn();
        eo::accept();
    });
    EO_CHECK_EQ(odd.code, 3);
    EO_CHECK_IN(odd.text, "the judge closed the control channel");
    staged const garbled = run_control("3 1\n", {"NONSENSE"}, {}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(2, 100, "instances");
        (void)ctl.spawn();
        eo::accept();
    });
    EO_CHECK_EQ(garbled.code, 3);
    EO_CHECK_IN(garbled.text, "the judge answered SPAWN with \"NONSENSE\"");
}


EO_TEST(a_last_word_to_an_instance_is_delivered_with_the_verdict) {
    staged const result = run_control("1\n", {"ok"}, {"7\n"}, 10, [](eo::controller& ctl) {
        ctl.input.read_int(1, 9, "n");
        eo::channel& only = ctl.spawn();
        only.send("guess");
        only.read_int(1, 9, "x");
        only.send("done");
        eo::accept("heard");
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.said, std::string("guess\ndone\n"));
}

namespace {

template <class T, class = void>
struct skips_the_rest : std::false_type {};

template <class T>
struct skips_the_rest<T, std::void_t<decltype(std::declval<T&>().skip_rest(std::string()))>> : std::true_type {};

template <class T, class = void>
struct ignores_trailing : std::false_type {};

template <class T>
struct ignores_trailing<T, std::void_t<decltype(std::declval<T&>().trailing(eo::ignore))>> : std::true_type {};

static_assert(skips_the_rest<eo::stream>::value && !skips_the_rest<eo::channel>::value,
              "only a checker's closing checks look at skip_rest");
static_assert(ignores_trailing<eo::stream>::value && !ignores_trailing<eo::channel>::value,
              "only a checker's closing checks look at trailing");

}  // namespace

EO_TEST(a_channel_reads_everything_a_stream_reads) {
    staged const result =
        run_control("1\n", {"ok"}, {"a line of words\n3 1 2\n0.5 1.5\nab cd\n+7\n"}, 1, [](eo::controller& ctl) {
            eo::channel& one = ctl.spawn();
            one.send(1);
            std::string const line = one.read_line(0, 100, "line");
            std::vector<int> const ints = one.read_ints(3, 1, 3, "a");
            EO_CHECK(one.at_eoln());
            std::vector<double> const reals = one.read_reals(2, 0, 2, "r");
            std::vector<std::string> const words = one.read_tokens(2, 2, 2, eo::charset("a-z"), "w");
            one.numbers(eo::lenient);
            long long const plus = one.read_long(1, 9, "plus");
            EO_CHECK(one.at_eof());
            one.wrong("{} {} {} {} {}", line, ints[0], reals[1], words[1], plus);
        });
    EO_CHECK_EQ(result.text, std::string("wrong answer: instance 1, line 6: a line of words 3 1.5 cd 7"));
}

EO_TEST(a_channel_reads_edges_trees_and_graphs) {
    staged const result =
        run_control("1\n", {"ok"}, {"1 2\n2 3\n1 2 5\n2 3 6\n1 3\n"}, 1, [](eo::controller& ctl) {
            eo::channel& one = ctl.spawn();
            one.send(1);
            std::vector<eo::edge> const plain = one.read_edges(2, 3, "e");
            std::vector<eo::weighted_edge> const tree = one.read_tree(3, eo::weighted(1, 9), "t");
            std::vector<eo::edge> const graph = one.read_graph(3, 1, eo::simple, "g");
            eo::accept("{} {} {} {}", plain[1].v, tree[1].w, graph[0].v, one.at_eof());
        });
    EO_CHECK_EQ(result.text, std::string("ok 3 6 3 true"));
}

EO_TEST(an_exception_nothing_caught_ends_a_controller_as_a_jury_error) {
    staged const ended = run_control("3 1\n", {}, {}, 2, [](eo::controller&) {
        try {
            throw std::logic_error("no instance answered");
        } catch (...) {
            eo::detail::scorer::ended_by_terminate();
        }
    });
    EO_CHECK_EQ(ended.code, 3);
    EO_CHECK_EQ(ended.text, std::string("jury error an exception nothing caught ended the controller: no instance "
                                        "answered"));
}
