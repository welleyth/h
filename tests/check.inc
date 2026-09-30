namespace {

struct three_files {
    std::string input;
    std::string output;
    std::string answer;

    three_files(std::string const& in, std::string const& out, std::string const& ans)
        : input(temporary_path()), output(temporary_path()), answer(temporary_path()) {
        put(input, in);
        put(output, out);
        put(answer, ans);
    }

    three_files(three_files const&) = delete;

    ~three_files() {
        ::unlink(input.c_str());
        ::unlink(output.c_str());
        ::unlink(answer.c_str());
    }

    static void put(std::string const& path, std::string const& text) {
        std::FILE* const writing = std::fopen(path.c_str(), "wb");
        std::fwrite(text.data(), 1, text.size(), writing);
        std::fclose(writing);
    }
};

void forget_the_environment() {
    for (char const* one : {"INPUT_FILE", "OUTPUT_FILE", "ANSWER_FILE", "TEST_COST", "TEST_GROUP",
                            "TEST_INDEX", "TEST_ID", "EOLYMP"})
        ::unsetenv(one);
}

template <class Body>
eot::outcome check_run(std::string const& in, std::string const& out, std::string const& ans, Body&& body) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    ::setenv("TEST_COST", "40", 1);
    three_files files(in, out, ans);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        body(c);
    });
    forget_the_environment();
    if (std::getenv("EOT_TRACE") != nullptr) std::fprintf(stderr, "   verdict: %s\n", result.text.c_str());
    return result;
}

std::string checker_warnings() { return eo::detail::diagnostics::shared().local_block(); }

}  // namespace

EO_TEST(a_checker_reads_three_files_and_accepts) {
    eot::outcome const result = check_run("3\n", "60\n", "60\n", [](eo::checker& c) {
        int const n = c.input.read_int(1, 100, "n");
        EO_CHECK_EQ(n, 3);
        long long const want = c.jury.read_long(eo::any, "sum");
        long long const got = c.output.read_long(eo::any, "sum");
        if (got != want) eo::wrong("the sum is {}, not {}", want, got);
        eo::accept("the sum is {}", got);
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok the sum is 60"));
}

EO_TEST(a_stream_decides_whose_fault_a_bad_read_is) {
    eot::outcome const contestant = check_run("3\n", "abc\n", "60\n", [](eo::checker& c) {
        c.input.read_int(1, 100, "n");
        c.jury.read_long(eo::any, "sum");
        c.output.read_long(eo::any, "sum");
        eo::accept();
    });
    EO_CHECK_EQ(contestant.code, 1);
    EO_CHECK_IN(contestant.text,
                "wrong answer: output.txt, line 1, sum: expected an integer, found \"abc\"");
    eot::outcome const paper = check_run("3\n", "60\n", "abc\n", [](eo::checker& c) {
        c.input.read_int(1, 100, "n");
        c.jury.read_long(eo::any, "sum");
        eo::accept();
    });
    EO_CHECK_EQ(paper.code, 3);
    EO_CHECK_IN(paper.text, "jury error: answer.txt, line 1, sum: expected an integer");
    eot::outcome const test = check_run("x\n", "60\n", "60\n", [](eo::checker& c) {
        c.input.read_int(1, 100, "n");
        eo::accept();
    });
    EO_CHECK_EQ(test.code, 3);
    EO_CHECK_IN(test.text, "jury error: input.txt, line 1, n: expected an integer");
}

EO_TEST(a_checker_stream_does_not_care_where_the_line_breaks_are) {
    eot::outcome const result = check_run("2\n", "  7\n\n  8  \n", "7 8\n", [](eo::checker& c) {
        int const n = c.input.read_int(1, 100, "n");
        std::vector<long long> const want = c.jury.read_longs(n, eo::any, "a");
        std::vector<long long> const got = c.output.read_longs(n, 1, 100, "a");
        EO_CHECK(want == got);
        eo::accept("{} values", n);
    });
    EO_CHECK(!result.stopped || result.code == 0);
    EO_CHECK_EQ(result.code, 0);
}

EO_TEST(a_partial_score_is_a_fraction_of_the_test) {
    eot::outcome const half = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        c.jury.read_int(1, 9, "a");
        eo::score(0.5, "half of it");
    });
    EO_CHECK_EQ(half.code, 7);
    EO_CHECK_EQ(half.text, std::string("points 20 half of it"));
    eot::outcome const exact = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        c.jury.read_int(1, 9, "a");
        eo::score(eo::ratio(3, 3), "all of it");
    });
    EO_CHECK_EQ(exact.code, 0);
    EO_CHECK_EQ(exact.text, std::string("ok all of it"));
    eot::outcome const none = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(eo::ratio(0, 3), "none of it");
    });
    EO_CHECK_EQ(none.code, 7);
    EO_CHECK_EQ(none.text, std::string("points 0 none of it"));
    eot::outcome const third = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(eo::ratio(1, 3), "a third");
    });
    EO_CHECK_EQ(third.text, std::string("points 13.33333333 a third"));
}

EO_TEST(points_and_rounding_go_through_the_cost) {
    eot::outcome const absolute = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(7.5, "a bonus");
    });
    EO_CHECK_EQ(absolute.text, std::string("points 7.5 a bonus"));
    eot::outcome const rounded = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(1.0 / 3, eo::round_to(0), "rounded down");
    });
    EO_CHECK_EQ(rounded.text, std::string("points 13 rounded down"));
    eot::outcome const over = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(99, "too many");
    });
    EO_CHECK_EQ(over.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO207: 99 points is more than the test's 40");
    eot::outcome const negative = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(-5, "a penalty");
    });
    EO_CHECK_EQ(negative.code, 7);
    EO_CHECK_EQ(negative.text, std::string("points 0 a penalty"));
    EO_CHECK_IN(checker_warnings(), "warning EO205: -5 points was clamped to 0");
    eot::outcome const slight = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(-0.5, "half a penalty");
    });
    EO_CHECK_EQ(slight.text, std::string("points 0 half a penalty"));
    eot::outcome const endless = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(-std::numeric_limits<double>::infinity(), "an endless penalty");
    });
    EO_CHECK_EQ(endless.text, std::string("points 0 an endless penalty"));
    EO_CHECK_IN(checker_warnings(), "warning EO205: -inf points was clamped to 0");
}

EO_TEST(a_message_that_does_not_match_its_values_keeps_the_verdict) {
    int line = 0;
    eot::outcome const refused = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        long long const a = c.output.read_long(eo::any, "a");
        line = __LINE__ + 1;
        eo::wrong("expected %lld, found %lld", 2LL, a);
    });
    EO_CHECK_EQ(refused.code, 1);
    EO_CHECK_EQ(refused.text, std::string("wrong answer expected %lld, found %lld 2 1"));
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO112: the message \"expected %lld, found %lld\" "
                                            "has 0 {{}} for 2 values", line).c_str());
    eot::outcome const accepted = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_long(eo::any, "a");
        c.jury.read_long(eo::any, "a");
        eo::accept("{} of {}", 1);
    });
    EO_CHECK_EQ(accepted.code, 0);
    EO_CHECK_EQ(accepted.text, std::string("ok 1 of {}"));
}

EO_TEST(a_scored_zero_is_not_the_same_run_as_a_wrong_answer) {
    eot::outcome const scored = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(0.0, "nothing matched");
    });
    EO_CHECK_EQ(scored.code, 7);
    EO_CHECK_EQ(scored.text, std::string("points 0 nothing matched"));

    eot::outcome const refused = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::wrong("not an answer at all");
    });
    EO_CHECK_EQ(refused.code, 1);
    EO_CHECK_EQ(refused.text, std::string("wrong answer not an answer at all"));
}

EO_TEST(a_score_that_is_not_a_number_is_a_jury_error) {
    for (double const value : {std::nan(""), -std::nan("")}) {
        eot::outcome const scored = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
            c.input.read_int(1, 9, "n");
            c.output.read_int(1, 9, "a");
            eo::score(value, "degenerate");
        });
        EO_CHECK_EQ(scored.code, 3);
        EO_CHECK_IN(scored.text, "eolymp.h: a score of ");
        EO_CHECK_IN(scored.text, "is not a number the judge can pay");
        eot::outcome const paid = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
            c.input.read_int(1, 9, "n");
            c.output.read_int(1, 9, "a");
            eo::points(value, "degenerate");
        });
        EO_CHECK_EQ(paid.code, 3);
        EO_CHECK_IN(paid.text, " points is not a number the judge can pay");
        eot::outcome const direct = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
            c.input.read_int(1, 9, "n");
            c.output.read_int(1, 9, "a");
            c.pass(value, "degenerate");
        });
        EO_CHECK_EQ(direct.code, 3);
        EO_CHECK_IN(direct.text, "is not a number the judge can pay");
    }
}

EO_TEST(an_infinite_score_is_clamped_like_any_score_outside_the_test) {
    double const infinity = std::numeric_limits<double>::infinity();
    eot::outcome const beaten = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(infinity, "the jury's cost over a cost of 0");
    });
    EO_CHECK_EQ(beaten.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO205: a score of inf was clamped into 0..1");
    eot::outcome const below = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(-infinity, "far below");
    });
    EO_CHECK_EQ(below.code, 7);
    EO_CHECK_EQ(below.text, std::string("points 0 far below"));
    EO_CHECK_IN(checker_warnings(), "warning EO205: a score of -inf was clamped into 0..1");
    eot::outcome const paid = check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(infinity, "endless");
    });
    EO_CHECK_EQ(paid.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO207: inf points is more than the test's 40");
}

EO_TEST(rounding_to_more_digits_than_a_double_holds_changes_nothing) {
    eot::outcome const result = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(0.5, eo::round_to(400), "half");
    });
    EO_CHECK_EQ(result.code, 7);
    EO_CHECK_EQ(result.text, std::string("points 20 half"));
}

EO_TEST(a_score_outside_the_test_is_clamped_and_reported) {
    eot::outcome const above = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(1.5, "overshot");
    });
    EO_CHECK_EQ(above.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO205: a score of 1.5 was clamped into 0..1");
    EO_CHECK(!eot::contains(checker_warnings(), "percentage"));
    eot::outcome const percent = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(30, "a percentage");
    });
    EO_CHECK_EQ(percent.code, 0);
    EO_CHECK_EQ(percent.text, std::string("ok a percentage"));
    EO_CHECK_IN(checker_warnings(), "warning EO205: a score of 30 was clamped to 1; it looks like a percentage");
    EO_CHECK_IN(checker_warnings(), "use eo::ratio(a, b) for a out of b, or eo::points for points");
    eot::outcome const twice = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(2, eo::round_to(0), "double");
    });
    EO_CHECK_EQ(twice.code, 0);
    EO_CHECK_IN(checker_warnings(), "a score of 2 was clamped to 1; it looks like a percentage");
    eot::outcome const over = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(1 + 1e-12, "a hair over");
    });
    EO_CHECK_EQ(over.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO205: a score of 1.000000000001 was clamped into 0..1");
    eot::outcome const below = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(-0.5, "undershot");
    });
    EO_CHECK_EQ(below.code, 7);
    EO_CHECK_IN(below.text, "points 0");
    eot::outcome const hair = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(1 - 1e-12, "a hair short");
    });
    EO_CHECK_EQ(hair.code, 7);
    EO_CHECK_IN(checker_warnings(), "warning EO206");
}

EO_TEST(a_score_warning_points_at_the_line_that_scored) {
    int line = 0;
    check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        line = __LINE__ + 1;
        eo::score(-0.5, "undershot");
    });
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO205", line).c_str());
    check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        line = __LINE__ + 1;
        eo::score(30, "a percentage");
    });
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO205: a score of 30", line).c_str());
    check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        line = __LINE__ + 1;
        eo::score(1 - 1e-12, eo::round_to(20), "a hair short");
    });
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO206", line).c_str());
    check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        line = __LINE__ + 1;
        eo::points(99, "too many");
    });
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO207", line).c_str());
}

EO_TEST(a_score_may_be_anything_that_converts_to_a_double) {
    struct measured {
        double share;
        operator double() const { return share; }
    };
    eot::outcome const scored = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(measured{0.5}, "half");
    });
    EO_CHECK_EQ(scored.text, std::string("points 20 half"));
    eot::outcome const rounded = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(measured{0.26}, eo::round_to(0), "rounded");
    });
    EO_CHECK_EQ(rounded.text, std::string("points 10 rounded"));
    eot::outcome const paid = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(measured{7.5}, "a bonus");
    });
    EO_CHECK_EQ(paid.text, std::string("points 7.5 a bonus"));
    int line = 0;
    check_run("1\n", "1\n", "1\n", [&](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        line = __LINE__ + 1;
        eo::score(measured{-0.5}, "undershot");
    });
    EO_CHECK_IN(checker_warnings(), eo::fmt("check.inc:{}: warning EO205", line).c_str());
}

EO_TEST(points_the_judge_rounds_up_to_the_cost_are_reported) {
    eot::outcome const close = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(1 - 1e-8, "almost");
    });
    EO_CHECK_EQ(close.code, 7);
    EO_CHECK_EQ(close.text, std::string("points 39.9999996 almost"));
    EO_CHECK_IN(checker_warnings(), "warning EO206: 'points 39.9999996' is below the test's 40, but the judge reads "
                                     "points as a float, which rounds it to the full cost");
    eot::outcome const clear = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::points(39.9999, "not quite");
    });
    EO_CHECK_EQ(clear.text, std::string("points 39.9999 not quite"));
    EO_CHECK(!eot::contains(checker_warnings(), "EO206"));
}

EO_TEST(a_cost_the_judge_did_not_give_is_reported_on_the_judge) {
    three_files files("1\n", "1\n", "1\n");
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    auto const scored = [&](char const* judge, char const* cost) {
        eo::detail::diagnostics::shared().forget_everything();
        forget_the_environment();
        if (judge != nullptr) ::setenv("EOLYMP", judge, 1);
        if (cost != nullptr) ::setenv("TEST_COST", cost, 1);
        eot::outcome const result = eot::run_until_stop([&] {
            eo::checker c(static_cast<int>(arguments.size()), arguments.data());
            c.input.read_int(1, 9, "n");
            c.output.read_int(1, 9, "a");
            eo::score(0.5, "half");
        });
        forget_the_environment();
        return result;
    };
    eot::outcome const missing = scored("1", nullptr);
    EO_CHECK_IN(missing.text, "points 50 half");
    EO_CHECK_IN(checker_warnings(), "warning EO213: TEST_COST is not set, so this test is taken to be worth 100");
    EO_CHECK(!eot::contains(checker_warnings(), "times)"));
    eot::outcome const garbled = scored("1", "ten");
    EO_CHECK_IN(garbled.text, "points 0 half");
    EO_CHECK_IN(checker_warnings(), "warning EO213: TEST_COST is \"ten\", which is not a number");
    EO_CHECK(!eot::contains(checker_warnings(), "times)"));
    eot::outcome const local = scored(nullptr, nullptr);
    EO_CHECK_EQ(local.text, std::string("points 50 half"));
    EO_CHECK(!eot::contains(checker_warnings(), "EO213"));
    eot::outcome const given = scored("1", "40");
    EO_CHECK_IN(given.text, "points 20 half");
    EO_CHECK(!eot::contains(checker_warnings(), "EO213"));
}

EO_TEST(a_test_worth_nothing_says_so) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "1\n", "1\n");
    ::setenv("TEST_COST", "0", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        EO_CHECK(c.cost() == 0);
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(0.5, "half");
    });
    forget_the_environment();
    EO_CHECK_EQ(result.text, std::string("points 0 half"));
    EO_CHECK_IN(checker_warnings(), "warning EO208: this test is worth 0 points, so the judge counts this "
                                     "score of 0.5 as accepted");
    EO_CHECK_IN(checker_warnings(), "end an answer that earns nothing with eo::wrong");
    eo::detail::diagnostics::shared().forget_everything();
    ::setenv("TEST_COST", "0", 1);
    eot::outcome const zero = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::score(0.0, "nothing");
    });
    forget_the_environment();
    EO_CHECK_EQ(zero.code, 7);
    EO_CHECK_EQ(zero.text, std::string("points 0 nothing"));
    EO_CHECK_IN(checker_warnings(), "score of 0 as accepted");
}

EO_TEST(the_checker_knows_which_test_it_is_on) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "1\n", "1\n");
    ::setenv("TEST_GROUP", "3", 1);
    ::setenv("TEST_INDEX", "7", 1);
    ::setenv("TEST_ID", "abc123", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        EO_CHECK_EQ(c.group(), 3);
        EO_CHECK_EQ(c.index(), 7);
        EO_CHECK_EQ(c.test_id(), std::string("abc123"));
        EO_CHECK(c.cost() == 100);
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 0);
}

EO_TEST(one_reader_serves_the_jury_and_the_contestant) {
    auto const chosen = [](eo::checker&) {
        return [](eo::answer_stream& a) {
            int const k = a.read_int(1, 3, "k");
            std::vector<int> const items = a.read_ints(k, 1, 9, "item");
            if (!eo::all_distinct(items)) a.wrong("an item is chosen twice");
            return k;
        };
    };
    eot::outcome const good = check_run("3\n", "2 4 5\n", "2 6 7\n", [&](eo::checker& c) {
        auto const [by_jury, found] = c.read_both(chosen(c));
        EO_CHECK_EQ(by_jury, 2);
        EO_CHECK_EQ(found, 2);
        eo::accept("{} items", found);
    });
    EO_CHECK_EQ(good.code, 0);
    eot::outcome const contestant = check_run("3\n", "2 4 4\n", "2 6 7\n", [&](eo::checker& c) {
        auto const [by_jury, found] = c.read_both(chosen(c));
        (void)by_jury;
        (void)found;
        eo::accept();
    });
    EO_CHECK_EQ(contestant.code, 1);
    EO_CHECK_IN(contestant.text, "wrong answer: output.txt");
    EO_CHECK_IN(contestant.text, "an item is chosen twice");
    eot::outcome const paper = check_run("3\n", "2 4 5\n", "2 6 6\n", [&](eo::checker& c) {
        auto const [by_jury, found] = c.read_both(chosen(c));
        (void)by_jury;
        (void)found;
        eo::accept();
    });
    EO_CHECK_EQ(paper.code, 3);
    EO_CHECK_IN(paper.text, "jury error: answer.txt");
}

EO_TEST(a_bare_wrong_inside_a_shared_reader_is_mapped_too) {
    auto const reader = [](eo::answer_stream& a) {
        int const k = a.read_int(1, 9, "k");
        if (k == 5) eo::wrong("five is not allowed");
        return k;
    };
    eot::outcome const paper = check_run("1\n", "3\n", "5\n", [&](eo::checker& c) {
        auto const both = c.read_both(reader);
        (void)both;
        eo::accept();
    });
    EO_CHECK_EQ(paper.code, 3);
    EO_CHECK_IN(paper.text, "jury error: answer.txt");
    eot::outcome const contestant = check_run("1\n", "5\n", "3\n", [&](eo::checker& c) {
        auto const both = c.read_both(reader);
        (void)both;
        eo::accept();
    });
    EO_CHECK_EQ(contestant.code, 1);
    EO_CHECK_IN(contestant.text, "wrong answer: output.txt");
}

EO_TEST(a_real_optimum_without_a_tolerance_keeps_its_verdict_and_says_so) {
    auto const run = [](char const* out, char const* ans) {
        return check_run("1\n", out, ans, [](eo::checker& c) {
            double const by_jury = c.jury.read_real(eo::any, "cost");
            double const found = c.output.read_real(eo::any, "cost");
            c.optimum(by_jury, found, eo::minimize);
        });
    };
    eot::outcome const moved = run("0.30000000000000004\n", "0.3\n");
    EO_CHECK_EQ(moved.code, 1);
    EO_CHECK_EQ(moved.text, std::string("wrong answer the answer is 0.30000000000000004; the optimum is 0.3"));
    EO_CHECK_IN(checker_warnings(), "warning EO214: c.optimum compares two reals with ==");
    EO_CHECK_IN(checker_warnings(), "tests/check.inc:");
    eot::outcome const under = run("0.29999999999999999\n", "0.30000000000000004\n");
    EO_CHECK_EQ(under.code, 3);
    EO_CHECK_IN(under.text, "the contestant's 0.3 beats the jury's 0.30000000000000004");
    EO_CHECK_EQ(run("0.3\n", "0.3\n").code, 0);
    EO_CHECK_IN(checker_warnings(), "EO214");
    eot::outcome const whole = check_run("1\n", "10\n", "10\n", [](eo::checker& c) {
        c.optimum(c.jury.read_long(eo::any, "cost"), c.output.read_long(eo::any, "cost"), eo::minimize);
    });
    EO_CHECK_EQ(whole.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO214"));
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK(eo::compare(0.1 + 0.2, 0.3, eo::minimize) == eo::standing::worse);
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(), "warning EO214: eo::compare compares two reals");
    eo::detail::diagnostics::shared().forget_everything();
    EO_CHECK(eo::compare(3LL, 3LL, eo::maximize) == eo::standing::equal);
    EO_CHECK(!eo::detail::diagnostics::shared().anything());
}

EO_TEST(an_optimum_has_three_outcomes) {
    auto const run = [](char const* out, char const* ans) {
        return check_run("1\n", out, ans, [](eo::checker& c) {
            long long const by_jury = c.jury.read_long(eo::any, "cost");
            long long const found = c.output.read_long(eo::any, "cost");
            c.optimum(by_jury, found, eo::minimize);
        });
    };
    EO_CHECK_EQ(run("10\n", "10\n").code, 0);
    eot::outcome const worse = run("17\n", "12\n");
    EO_CHECK_EQ(worse.code, 1);
    EO_CHECK_EQ(worse.text, std::string("wrong answer the answer is 17; the optimum is 12"));
    eot::outcome const better = run("9\n", "12\n");
    EO_CHECK_EQ(better.code, 3);
    EO_CHECK_IN(better.text, "the contestant's 9 beats the jury's 12");
    eot::outcome const upward = check_run("1\n", "9\n", "12\n", [](eo::checker& c) {
        long long const by_jury = c.jury.read_long(eo::any, "gain");
        long long const found = c.output.read_long(eo::any, "gain");
        c.optimum(by_jury, found, eo::maximize);
    });
    EO_CHECK_EQ(upward.code, 1);
}

EO_TEST(an_optimum_within_a_tolerance_accepts_what_rounding_moved) {
    auto const run = [](char const* out, char const* ans, eo::towards direction) {
        return check_run("1\n", out, ans, [direction](eo::checker& c) {
            double const by_jury = c.jury.read_real(eo::any, "cost");
            double const found = c.output.read_real(eo::any, "cost");
            c.optimum(by_jury, found, direction, eo::within(1e-6));
        });
    };
    eot::outcome const close = run("0.30000000000000004\n", "0.3\n", eo::minimize);
    EO_CHECK_EQ(close.code, 0);
    EO_CHECK_EQ(close.text, std::string("ok 0.30000000000000004"));
    EO_CHECK_EQ(run("1000000.5\n", "1000000\n", eo::minimize).code, 0);
    eot::outcome const worse = run("0.31\n", "0.3\n", eo::minimize);
    EO_CHECK_EQ(worse.code, 1);
    EO_CHECK_EQ(worse.text, std::string("wrong answer the answer is 0.31; the optimum is 0.3"));
    eot::outcome const better = run("0.29\n", "0.3\n", eo::minimize);
    EO_CHECK_EQ(better.code, 3);
    EO_CHECK_IN(better.text, "the contestant's 0.29 beats the jury's 0.3");
    EO_CHECK_EQ(run("0.29\n", "0.3\n", eo::maximize).code, 1);
    EO_CHECK_EQ(run("0.31\n", "0.3\n", eo::maximize).code, 3);
}

EO_TEST(a_comparison_says_better_equal_or_worse) {
    EO_CHECK(eo::compare(5, 7, eo::minimize) == eo::standing::better);
    EO_CHECK(eo::compare(7, 7, eo::minimize) == eo::standing::equal);
    EO_CHECK(eo::compare(5, 7, eo::maximize) == eo::standing::worse);
    EO_CHECK(eo::compare(0.1 + 0.2, 0.3, eo::minimize, eo::within(1e-9)) == eo::standing::equal);
    EO_CHECK(eo::compare(0.4, 0.3, eo::minimize, eo::within(1e-9)) == eo::standing::worse);
    EO_CHECK(eo::compare(0.4, 0.3, eo::maximize, eo::within(1e-9)) == eo::standing::better);
    EO_CHECK(eo::compare(0.3 + 1e-12, 0.3, eo::maximize, eo::within(0)) == eo::standing::better);
    double const endless = std::numeric_limits<double>::infinity();
    EO_CHECK(eo::compare(endless, endless, eo::minimize, eo::within(1e-9)) == eo::standing::equal);
    EO_CHECK(eo::compare(endless, 5.0, eo::minimize, eo::within(1e-9)) == eo::standing::worse);
    EO_CHECK(eo::compare(std::nan(""), 5.0, eo::minimize, eo::within(1e-9)) == eo::standing::worse);
    EO_CHECK(eo::compare(5.0, 5.0 + 5e-10, eo::minimize, eo::within(1e-9)) == eo::standing::equal);
    EO_CHECK(eo::compare(99.0, 100.0, eo::minimize, eo::within(0.0101)) == eo::standing::equal);
    EO_CHECK(eo::compare(100.0, 99.0, eo::minimize, eo::within(0.0101)) == eo::standing::worse);
    eot::outcome const unknown = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        (void)c.output.read_int(1, 9, "x");
        (void)eo::compare(1.0, std::nan(""), eo::minimize, eo::within(1e-6));
    });
    EO_CHECK_EQ(unknown.code, 3);
    EO_CHECK_IN(unknown.text, "the jury's value is nan, which no answer can equal");
    eot::outcome const exact = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        (void)c.output.read_int(1, 9, "x");
        (void)eo::compare(1.0, std::nan(""), eo::minimize);
    });
    EO_CHECK_EQ(exact.code, 3);
    eot::outcome const optimum = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.optimum(std::nan(""), static_cast<double>(c.output.read_int(1, 9, "x")), eo::minimize, eo::within(1e-6));
    });
    EO_CHECK_EQ(optimum.code, 3);
    EO_CHECK_IN(eot::run_until_stop([] { (void)eo::within(-1e-6); }).text,
                "eo::within needs a tolerance of 0 or more, not -1e-06");
    EO_CHECK_IN(eot::run_until_stop([] { (void)eo::within(std::nan("")); }).text, "not nan");
}

EO_TEST(the_ready_made_comparisons_compare) {
    EO_CHECK_EQ(check_run("1\n", "a b\nc\n", "a\nb c\n", [](eo::checker& c) { c.tokens(); }).code, 0);
    eot::outcome const differs = check_run("1\n", "a x\n", "a b\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(differs.text, std::string("wrong answer token 2 is \"x\", expected \"b\""));
    eot::outcome const shorter = check_run("1\n", "a\n", "a b\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_IN(shorter.text, "the output ended after 1 tokens");
    eot::outcome const longer = check_run("1\n", "a b\n", "a\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_IN(longer.text, "the answer has 1 tokens, the output has more");

    EO_CHECK_EQ(check_run("1\n", "one  \ntwo\n", "one\ntwo\n", [](eo::checker& c) { c.lines(); }).code, 0);
    std::string const carriage = "one\r\r\ntwo\r\n";
    EO_CHECK_EQ(check_run("1\n", carriage, carriage, [](eo::checker& c) { c.lines(); }).code, 0);
    EO_CHECK_EQ(check_run("1\n", "one\r\t\r\n", "one\n", [](eo::checker& c) { c.lines(); }).code, 0);
    eot::outcome const inside = check_run("1\n", "o\rne\n", "one\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_IN(inside.text, "line 1 is \"o\\x0dne\", expected \"one\"");
    eot::outcome const nul = check_run("1\n", std::string("one\0\n", 5), "one\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_EQ(nul.code, 1);
    eot::outcome const line_differs =
        check_run("1\n", "one\nxxx\n", "one\ntwo\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_IN(line_differs.text, "line 2 is \"xxx\", expected \"two\"");
    EO_CHECK_IN(check_run("1\n", "one\n", "one\ntwo\n", [](eo::checker& c) { c.lines(); }).text,
                "the output ended after 1 lines");
    EO_CHECK_IN(check_run("1\n", "one\ntwo\n", "one\n", [](eo::checker& c) { c.lines(); }).text,
                "the answer has 1 lines, the output has more");

    EO_CHECK_EQ(check_run("1\n", "1.0000001 ok\n", "1.0 ok\n", [](eo::checker& c) { c.reals(1e-6); }).code, 0);
    EO_CHECK_IN(check_run("1\n", "2.0\n", "1.0\n", [](eo::checker& c) { c.reals(1e-6); }).text,
                "value 1 is 2, expected 1");
    EO_CHECK_IN(check_run("1\n", "no\n", "yes\n", [](eo::checker& c) { c.reals(1e-6); }).text,
                "token 1 is \"no\", expected \"yes\"");
    EO_CHECK_IN(check_run("1\n", "1.0\n", "1.0 2.0\n", [](eo::checker& c) { c.reals(1e-6); }).text,
                "the output ended after 1 tokens");
    EO_CHECK_IN(check_run("1\n", "1.0 2.0\n", "1.0\n", [](eo::checker& c) { c.reals(1e-6); }).text,
                "the answer has 1 tokens, the output has more");
}

EO_TEST(the_ready_made_comparisons_shorten_and_escape_what_they_echo) {
    std::string const huge = std::string(100000, 'z') + "\n";
    eot::outcome const tokens =
        check_run("1\n", huge, std::string(100000, 'y') + "\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(tokens.text, "wrong answer token 1 is \"" + std::string(40, 'z') + "...\", expected \"" +
                                 std::string(40, 'y') + "...\"");
    std::string const bytes = std::string("a\x01\x7f\xd0\xbf\xbf") + "\n";
    eot::outcome const binary = check_run("1\n", bytes, "abcdef\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(binary.text, std::string("wrong answer token 1 is \"a\\x01\\x7fп\\xbf\", expected \"abcdef\""));
    eot::outcome const cyrillic =
        check_run("1\n", "привіт\n", "вітаємо\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(cyrillic.text, std::string("wrong answer token 1 is \"привіт\", expected \"вітаємо\""));
    std::string const split = std::string(39, 'a') + "їїї\n";
    eot::outcome const cut =
        check_run("1\n", split, std::string(50, 'b') + "\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(cut.text, "wrong answer token 1 is \"" + std::string(39, 'a') + "...\", expected \"" +
                              std::string(40, 'b') + "...\"");
    eot::outcome const line = check_run("1\n", "a\tb\n", "a b\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_IN(line.text, "line 1 is \"a\\x09b\", expected \"a b\"");
}

EO_TEST(the_ready_made_comparisons_read_no_more_of_a_token_than_they_need) {
    std::string const huge = std::string(2 * 1024 * 1024, 'z') + "\n";
    eot::outcome const tokens = check_run("1\n", huge, "abc\n", [](eo::checker& c) { c.tokens(); });
    EO_CHECK_EQ(tokens.text,
                std::string("wrong answer token 1 is longer than the expected \"abc\"; it starts \"zzzz\""));
    EO_CHECK(checker_warnings().find("EO111") == std::string::npos);
    eot::outcome const reals = check_run("1\n", huge, "1.0\n", [](eo::checker& c) { c.reals(1e-6); });
    EO_CHECK_EQ(reals.text,
                "wrong answer token 1 is longer than 4096 characters: \"" + std::string(40, 'z') + "...\"");
    std::string const zeros = "0.5" + std::string(5000, '0') + "\n";
    EO_CHECK_IN(check_run("1\n", zeros, "0.5\n", [](eo::checker& c) { c.reals(1e-6); }).text,
                "token 1 is longer than 4096 characters");
    std::string const fits = "0.5" + std::string(4093, '0') + "\n";
    EO_CHECK_EQ(check_run("1\n", fits, "0.5\n", [](eo::checker& c) { c.reals(1e-6); }).code, 0);
    std::string const long_answer = "0.5" + std::string(5000, '0') + "\n";
    EO_CHECK_EQ(check_run("1\n", long_answer, long_answer, [](eo::checker& c) { c.reals(1e-6); }).code, 0);
}

EO_TEST(a_checker_reads_the_interactors_summary) {
    std::string const summary = "eolymp-summary 1\nfraction 0.5\nvalue moves 12\nmessage half of it\n";
    eot::outcome const plain = check_run("1\n", summary, "", [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_EQ(plain.code, 7);
    EO_CHECK_EQ(plain.text, std::string("points 20 half of it"));
    eot::outcome const mapped = check_run("1\n", summary, "", [](eo::checker& c) {
        c.from_interactor([](eo::summary const& said) { return said.value("moves") / 16; });
    });
    EO_CHECK_EQ(mapped.text, std::string("points 30 half of it"));
    eot::outcome const unknown = check_run("1\n", summary, "", [](eo::checker& c) {
        c.from_interactor([](eo::summary const& said) { return said.value("turns"); });
    });
    EO_CHECK_EQ(unknown.code, 3);
    EO_CHECK_IN(unknown.text, "eolymp.h: the interactor recorded no value called \"turns\"");
    eot::outcome const phase =
        check_run("1\n", "eolymp-phase 2 3\n", "", [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_EQ(phase.code, 3);
    EO_CHECK_IN(phase.text, "the interactor stopped at phase 1 of 3; set run_count to 3");
    eot::outcome const other = check_run("1\n", "42\n", "", [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_IN(other.text, "this is not an interactor's summary: it starts with \"42\"");
    eot::outcome const odd = check_run("1\n", "eolymp-summary 1\nbonus 1\n", "", [](eo::checker& c) {
        c.from_interactor();
    });
    EO_CHECK_IN(odd.text, "the summary has a field called \"bonus\"");
}

EO_TEST(a_summary_with_a_field_twice_is_a_jury_error) {
    eot::outcome const fraction = check_run("1\n", "eolymp-summary 1\nfraction 0.00001\nfraction 1\n", "",
                                            [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_EQ(fraction.code, 3);
    EO_CHECK_IN(fraction.text, "line 3: the summary has a second fraction field");
    eot::outcome const message = check_run("1\n", "eolymp-summary 1\nmessage a\nmessage b\n", "",
                                           [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_IN(message.text, "the summary has a second message field");
    eot::outcome const value = check_run("1\n", "eolymp-summary 1\nvalue x 1\nvalue y 2\nvalue x 3\n", "",
                                         [](eo::checker& c) { c.from_interactor(); });
    EO_CHECK_EQ(value.code, 3);
    EO_CHECK_IN(value.text, "the summary has a second value called \"x\"");
}

EO_TEST(lines_skip_blank_lines_and_the_blanks_that_start_a_line) {
    auto const lines = [](char const* out, char const* ans) {
        return check_run("1\n", out, ans, [](eo::checker& c) { c.lines(); });
    };
    EO_CHECK_EQ(lines("a\n\n\nb\n", "a\nb\n").code, 0);
    EO_CHECK_EQ(lines("a\nb\n", "a\n\nb\n").code, 0);
    EO_CHECK_EQ(lines("*\n", " *\n").code, 0);
    EO_CHECK_EQ(lines("  x\n\ty\n", "x\ny\n").code, 0);
    EO_CHECK_EQ(lines("\n\na\n", "a\n").code, 0);
    EO_CHECK_EQ(lines("a\n\n\n", "a\n").code, 0);
    EO_CHECK_EQ(lines("a\n\n\n", "a\n").text, std::string("ok 1 lines"));
    eot::outcome const spaced = lines("hello  world\n", "hello world\n");
    EO_CHECK_EQ(spaced.code, 1);
    EO_CHECK_EQ(spaced.text, std::string("wrong answer line 1 is \"hello  world\", expected \"hello world\""));
}

EO_TEST(exact_lines_keep_blank_lines_and_the_blanks_that_start_a_line) {
    auto const exact = [](std::string const& out, std::string const& ans) {
        return check_run("1\n", out, ans, [](eo::checker& c) { c.lines(eo::exact); });
    };
    EO_CHECK_EQ(exact("a\n\nb\n", "a\n\nb\n").text, std::string("ok 3 lines"));
    EO_CHECK_EQ(exact(" *\n**\n", " *\n**\n").code, 0);
    EO_CHECK_EQ(exact("one  \t\r\ntwo\n", "one\ntwo\n").code, 0);
    EO_CHECK_EQ(exact("a\nb", "a\nb\n").code, 0);
    EO_CHECK_EQ(exact("a\nb\n\n\n", "a\nb\n").text, std::string("ok 2 lines"));
    EO_CHECK_EQ(exact("a\nb\n", "a\nb\n\n").text, std::string("ok 2 lines"));
    eot::outcome const extra = exact("a\n\n\nb\n", "a\nb\n");
    EO_CHECK_EQ(extra.code, 1);
    EO_CHECK_EQ(extra.text, std::string("wrong answer line 2 is \"\", expected \"b\""));
    eot::outcome const missing = exact("a\nb\n", "a\n\nb\n");
    EO_CHECK_EQ(missing.text, std::string("wrong answer line 2 is \"b\", expected \"\""));
    eot::outcome const indented = exact("*\n", " *\n");
    EO_CHECK_EQ(indented.text, std::string("wrong answer line 1 is \"*\", expected \" *\""));
    eot::outcome const shorter = exact("a\n", "a\nb\n");
    EO_CHECK_EQ(shorter.text, std::string("wrong answer the output ended after 1 lines, the answer has more"));
    eot::outcome const longer = exact("a\nb\nc\n", "a\nb\n");
    EO_CHECK_EQ(longer.text, std::string("wrong answer the answer has 2 lines, the output has more"));
    std::string const huge = "abc" + std::string(2 * 1024 * 1024, 'z') + "\n";
    EO_CHECK_EQ(exact(huge, "abc\n").text,
                std::string("wrong answer line 1 is longer than the expected \"abc\"; it starts \"abcz\""));
    EO_CHECK_EQ(exact("\n\n", "").text, std::string("ok 0 lines"));
    EO_CHECK_EQ(exact("a\n  c\n", "a\n").text, std::string("wrong answer the answer has 1 lines, the output has more"));
    EO_CHECK_EQ(exact("a\nb\nc" + std::string(70000, 'c') + "\n", "a\nb\n").text,
                std::string("wrong answer the answer has 2 lines, the output has more"));
    EO_CHECK_EQ(exact("a\nb\nc\n", "a\nb\n\n").text, std::string("wrong answer line 3 is \"c\", expected \"\""));
}

EO_TEST(lines_and_yes_no_read_no_more_of_a_line_than_they_need) {
    std::string const huge = "abc" + std::string(2 * 1024 * 1024, 'z') + "\n";
    eot::outcome const lines = check_run("1\n", huge, "abc\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_EQ(lines.text,
                std::string("wrong answer line 1 is longer than the expected \"abc\"; it starts \"abcz\""));
    std::string const padded = "one" + std::string(1000, ' ') + "\t\r\ntwo\n";
    EO_CHECK_EQ(check_run("1\n", padded, "one\ntwo\n", [](eo::checker& c) { c.lines(); }).code, 0);
    std::string const tail = "one" + std::string(50, ' ') + "x\n";
    eot::outcome const late = check_run("1\n", tail, "one\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_EQ(late.text,
                std::string("wrong answer line 1 is longer than the expected \"one\"; it starts \"one \""));
    eot::outcome const carriage = check_run("1\n", "a\rb\n", "a\n", [](eo::checker& c) { c.lines(); });
    EO_CHECK_EQ(carriage.text,
                std::string("wrong answer line 1 is longer than the expected \"a\"; it starts \"a\\x0d\""));
    eot::outcome const verdict = check_run("1\n", "YES" + std::string(2 * 1024 * 1024, 'S') + "\n", "NO\n",
                                           [](eo::checker& c) { c.yes_no([](eo::stream&) {}); });
    EO_CHECK_EQ(verdict.code, 1);
    EO_CHECK_IN(verdict.text, "is not one of YES, NO");
    EO_CHECK(!eot::contains(checker_warnings(), "EO111"));
}

EO_TEST(yes_and_no_with_a_certificate) {
    auto const certificate = [](eo::answer_stream& a) {
        int const k = a.read_int(1, 9, "k");
        if (k == 7) a.wrong("seven is not a certificate");
    };
    EO_CHECK_EQ(check_run("1\n", "no\n", "NO\n", [&](eo::checker& c) { c.yes_no(certificate); }).code, 0);
    EO_CHECK_IN(check_run("1\n", "NO\n", "YES 3\n", [&](eo::checker& c) { c.yes_no(certificate); }).text,
                "a solution exists");
    EO_CHECK_EQ(check_run("1\n", "YES 3\n", "YES 4\n", [&](eo::checker& c) { c.yes_no(certificate); }).code, 0);
    eot::outcome const impossible =
        check_run("1\n", "YES 3\n", "NO\n", [&](eo::checker& c) { c.yes_no(certificate); });
    EO_CHECK_EQ(impossible.code, 3);
    EO_CHECK_IN(impossible.text, "found a solution the jury says does not exist");
    eot::outcome const bad =
        check_run("1\n", "YES 7\n", "NO\n", [&](eo::checker& c) { c.yes_no(certificate); });
    EO_CHECK_EQ(bad.code, 1);
    EO_CHECK_IN(bad.text, "seven is not a certificate");
    eot::outcome const words = check_run("1\n", "IMPOSSIBLE\n", "IMPOSSIBLE\n", [&](eo::checker& c) {
        c.yes_no(certificate, "POSSIBLE", "IMPOSSIBLE");
    });
    EO_CHECK_EQ(words.code, 0);
    EO_CHECK_IN(check_run("1\n", "maybe\n", "NO\n", [&](eo::checker& c) { c.yes_no(certificate); }).text,
                "\"maybe\" is not one of YES, NO");
}

EO_TEST(what_is_left_of_each_file_is_looked_at) {
    eot::outcome const junk = check_run("1\n", "5 junk\n", "5\n", [](eo::checker& c) {
        c.jury.read_int(1, 9, "a");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(junk.code, 1);
    EO_CHECK_IN(junk.text, "extra output after the answer: \"junk\"");
    eot::outcome const allowed = check_run("1\n", "5 junk\n", "5\n", [](eo::checker& c) {
        c.output.trailing(eo::ignore);
        c.jury.read_int(1, 9, "a");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(allowed.code, 0);
    eot::outcome const unread = check_run("1\n", "5\n", "5 OK\n", [](eo::checker& c) {
        c.jury.read_int(1, 9, "a");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(unread.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO203: the answer file still holds \"OK\"");
    eot::outcome const deliberate = check_run("1\n", "5\n", "5 OK\n", [](eo::checker& c) {
        c.jury.skip_rest("the rest is the jury's certificate");
        c.jury.read_int(1, 9, "a");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(deliberate.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO203"));
    eot::outcome const unreasoned = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.jury.skip_rest("");
        eo::accept();
    });
    EO_CHECK_IN(unreasoned.text, "skip_rest needs a reason");
}

EO_TEST(a_checker_that_judges_nothing_is_reported) {
    eot::outcome const blind = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.output.trailing(eo::ignore);
        c.input.read_int(1, 9, "n");
        eo::accept("looks fine");
    });
    EO_CHECK_EQ(blind.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO201: the checker passed the run without reading any of the output");
    eot::outcome const testless = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(testless.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO202: the checker read neither the input nor the answer");
}

EO_TEST(a_verdict_with_nothing_to_say_is_reported) {
    eot::outcome const silent = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::wrong();
    });
    EO_CHECK_EQ(silent.code, 1);
    EO_CHECK_IN(checker_warnings(), "warning EO204: this wrong answer carries no message");
    eot::outcome const quiet = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        eo::jury_error();
    });
    EO_CHECK_EQ(quiet.code, 3);
    EO_CHECK_IN(checker_warnings(), "warning EO204: this jury error carries no message");
}

EO_TEST(a_problem_with_many_answers_wants_more_than_a_comparison) {
    eot::outcome const result = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.answers(eo::many);
        c.input.read_int(1, 9, "n");
        c.tokens();
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO212: the problem declares many answers");
    eot::outcome const single = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.answers(eo::unique);
        c.input.read_int(1, 9, "n");
        c.tokens();
    });
    EO_CHECK_EQ(single.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO212"));
}

EO_TEST(a_checker_that_ends_without_a_verdict_is_a_jury_error) {
    eot::outcome const result = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "the checker ended without a verdict");
}

EO_TEST(messages_inside_cases_are_numbered) {
    eot::outcome const result = check_run("2\n", "1 9\n", "1 2\n", [](eo::checker& c) {
        int const t = c.input.read_int(1, 9, "t");
        c.cases(t, [&] {
            long long const want = c.jury.read_long(eo::any, "answer");
            long long const got = c.output.read_long(eo::any, "answer");
            if (got != want) eo::wrong("expected {}, found {}", want, got);
        });
        eo::accept();
    });
    EO_CHECK_EQ(result.text, std::string("wrong answer expected 2, found 9"));
    eot::outcome const located = check_run("2\n", "1 x\n", "1 2\n", [](eo::checker& c) {
        int const t = c.input.read_int(1, 9, "t");
        c.cases(t, [&] {
            c.jury.read_long(eo::any, "answer");
            c.output.read_long(eo::any, "answer");
        });
        eo::accept();
    });
    EO_CHECK_IN(located.text, "wrong answer: case 2: output.txt");
}

EO_TEST(on_the_judge_the_verdict_line_comes_first) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "5\n", "5\n");
    ::setenv("EOLYMP", "1", 1);
    ::setenv("TEST_COST", "40", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        std::printf("a line that ends in a space \n");
        std::printf("\n");
        eo::log("n = {}", c.input.read_int(1, 9, "n"));
        c.output.read_int(1, 9, "a");
        eo::score(0.5, "half");
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 7);
    EO_CHECK(result.printed.rfind("points 20 half\n", 0) == 0);
    EO_CHECK_IN(result.printed, "a line that ends in a space \n");
    EO_CHECK_IN(result.printed, "n = 1");
    EO_CHECK_IN(result.printed, "eolymp.h " EOLYMP_H_VERSION);
}

namespace {

class descriptor_limit {
public:
    descriptor_limit() { ::getrlimit(RLIMIT_NOFILE, &before_); }
    descriptor_limit(descriptor_limit const&) = delete;

    void leave(int free) {
        rlim_t top = 0;
        for (int seen = 0; seen < free || ::fcntl(static_cast<int>(top), F_GETFD) >= 0; top++)
            if (::fcntl(static_cast<int>(top), F_GETFD) < 0) seen++;
        rlimit capped = before_;
        capped.rlim_cur = top;
        ::setrlimit(RLIMIT_NOFILE, &capped);
    }

    ~descriptor_limit() { ::setrlimit(RLIMIT_NOFILE, &before_); }

private:
    rlimit before_{};
};

std::string round_trip(std::FILE* file) {
    if (file == nullptr) return "no file";
    std::fputs("held", file);
    std::rewind(file);
    char buffer[8] = {};
    std::size_t const got = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);
    return std::string(buffer, got);
}

}  // namespace

EO_TEST(a_checker_finds_a_scratch_file_where_it_can) {
#ifdef __linux__
    EO_CHECK_EQ(round_trip(eo::detail::scratch_in_memory()), std::string("held"));
#endif
    EO_CHECK_EQ(round_trip(eo::detail::scratch_in_the_temporary_directory()), std::string("held"));
    EO_CHECK_EQ(round_trip(eo::detail::scratch_in_the_workspace()), std::string("held"));
    EO_CHECK_EQ(round_trip(eo::detail::scratch_file()), std::string("held"));
    std::array<eo::detail::scratch_maker, 3> const makers = eo::detail::scratch_makers();
    EO_CHECK(makers[0] == &eo::detail::scratch_in_the_temporary_directory);
    EO_CHECK(makers[1] == &eo::detail::scratch_in_memory);
    EO_CHECK(makers[2] == &eo::detail::scratch_in_the_workspace);
    eo::detail::scratch_maker const refuse = [] { return static_cast<std::FILE*>(nullptr); };
    EO_CHECK_EQ(round_trip(eo::detail::first_scratch(std::array<eo::detail::scratch_maker, 2>{
                    refuse, &eo::detail::scratch_in_the_workspace})),
                std::string("held"));
    EO_CHECK_EQ(eo::detail::opened_scratch(-1), nullptr);
    {
        descriptor_limit none;
        none.leave(0);
        EO_CHECK_EQ(eo::detail::scratch_file(), nullptr);
    }
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "5\n", "5\n");
    ::setenv("EOLYMP", "1", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        descriptor_limit five;
        five.leave(5);
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        eo::accept();
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "eolymp.h: the checker cannot open a scratch file for its own output");
}

EO_TEST(on_the_judge_a_checker_without_a_verdict_is_a_jury_error) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "5\n", "5\n");
    ::setenv("EOLYMP", "1", 1);
    ::setenv("TEST_COST", "40", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        std::printf("said before leaving\n");
        c.input.read_int(1, 9, "n");
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK(result.printed.rfind("jury error the checker ended without a verdict\n", 0) == 0);
    EO_CHECK(!eot::contains(result.printed, "said before leaving"));
}

EO_TEST(a_very_noisy_checker_is_noted) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "5\n", "5\n");
    ::setenv("EOLYMP", "1", 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        std::string const noise(70000, 'x');
        std::fwrite(noise.data(), 1, noise.size(), stdout);
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO210: the checker printed 70000 bytes before its verdict");
}

EO_TEST(the_files_may_come_from_the_environment_and_may_be_swapped) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    three_files files("1\n", "5\n", "7\n");
    ::setenv("INPUT_FILE", files.input.c_str(), 1);
    ::setenv("OUTPUT_FILE", files.output.c_str(), 1);
    ::setenv("ANSWER_FILE", files.answer.c_str(), 1);
    std::string program = "checker.cpp";
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.answer.c_str()),
                                 const_cast<char*>(files.output.c_str())};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(arguments.size()), arguments.data());
        EO_CHECK_EQ(c.output.read_int(1, 9, "a"), 5);
        EO_CHECK_EQ(c.jury.read_int(1, 9, "a"), 7);
        c.input.read_int(1, 9, "n");
        eo::accept();
    });
    forget_the_environment();
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO211: this checker runs as the legacy type");
}

EO_TEST(a_checker_needs_three_files_and_only_one_role) {
    forget_the_environment();
    std::string program = "checker.cpp";
    std::vector<char*> two{program.data(), program.data()};
    eot::outcome const missing = eot::run_until_stop([&] {
        eo::checker c(static_cast<int>(two.size()), two.data());
        (void)c.cost();
    });
    EO_CHECK_IN(missing.text, "the checker was given no output file");
    three_files files("1\n", "5\n", "5\n");
    std::vector<char*> arguments{program.data(), const_cast<char*>(files.input.c_str()),
                                 const_cast<char*>(files.output.c_str()),
                                 const_cast<char*>(files.answer.c_str())};
    eot::outcome const twice = eot::run_until_stop([&] {
        eo::checker first(static_cast<int>(arguments.size()), arguments.data());
        eo::checker second(static_cast<int>(arguments.size()), arguments.data());
        (void)second.cost();
        eo::accept();
    });
    EO_CHECK_IN(twice.text, "this program already has a checker");
    eot::outcome const nobody = eot::run_until_stop([] { eo::accept("no checker here"); });
    EO_CHECK_IN(nobody.text, "this verdict needs an eo::checker");
    eot::outcome const divided = eot::run_until_stop([] { (void)eo::ratio(1, 0); });
    EO_CHECK_IN(divided.text, "eo::ratio divides by zero");
}

EO_TEST(the_contestant_may_be_read_loosely_or_strictly) {
    eot::outcome const strict = check_run("1\n", "+5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(strict.code, 1);
    EO_CHECK_IN(strict.text, "expected an integer, found \"+5\"");
    eot::outcome const loose = check_run("1\n", "+5 007 -0\n", "5\n", [](eo::checker& c) {
        c.output.numbers(eo::lenient);
        c.input.read_int(1, 9, "n");
        EO_CHECK_EQ(c.output.read_int(1, 9, "a"), 5);
        EO_CHECK_EQ(c.output.read_int(1, 9, "b"), 7);
        EO_CHECK_EQ(c.output.read_int(eo::any, "c"), 0);
        eo::accept();
    });
    EO_CHECK_EQ(loose.code, 0);
    eot::outcome const scientific = check_run("1\n", "1.5e2\n", "150\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK(c.output.read_real(0.0, 1000.0, "x") == 150.0);
        eo::accept();
    });
    EO_CHECK_EQ(scientific.code, 0);
    eot::outcome const decimal = check_run("1\n", "1.5e2\n", "150\n", [](eo::checker& c) {
        c.output.reals(eo::plain);
        c.input.read_int(1, 9, "n");
        c.output.read_real(0.0, 1000.0, "x");
        eo::accept();
    });
    EO_CHECK_EQ(decimal.code, 1);
    EO_CHECK_IN(decimal.text, "expected a number, found \"1.5e2\"");
}

EO_TEST(a_contestant_may_print_negative_zero) {
    eot::outcome const read = check_run("1\n", "-0.000000 -0 -1e-400\n", "0\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK(c.output.read_real(-1.0, 1.0, "x") == 0);
        EO_CHECK(c.output.read_real(-1.0, 1.0, "y") == 0);
        EO_CHECK(c.output.read_real(-1.0, 1.0, "z") == 0);
        c.jury.skip_rest("compared above");
        eo::accept();
    });
    EO_CHECK_EQ(read.code, 0);
    EO_CHECK_EQ(check_run("1\n", "-0.000000 -1e-400 -0\n", "0.000000 0 0.0\n", [](eo::checker& c) {
                    c.reals(1e-6);
                }).code,
                0);
}

EO_TEST(a_lenient_read_takes_a_whole_token_and_parses_it) {
    eot::outcome const glued = check_run("1\n", "1-2\n", "1 -2\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_long(eo::any, "a");
        c.output.read_long(eo::any, "b");
        eo::accept();
    });
    EO_CHECK_EQ(glued.code, 1);
    EO_CHECK_IN(glued.text, "expected an integer, found \"1-2\"");

    eot::outcome const trailing = check_run("1\n", "3abc 5\n", "3 5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_long(eo::any, "a");
        eo::accept();
    });
    EO_CHECK_EQ(trailing.code, 1);
    EO_CHECK_IN(trailing.text, "expected an integer, found \"3abc\"");

    eot::outcome const spaced = check_run("1\n", "1 -2\n", "1 -2\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK_EQ(c.output.read_long(eo::any, "a"), 1LL);
        EO_CHECK_EQ(c.output.read_long(eo::any, "b"), -2LL);
        c.jury.skip_rest("compared above");
        eo::accept();
    });
    EO_CHECK_EQ(spaced.code, 0);

    std::string const split = "0.5" + std::string(4093, '0') + "0.25\n";
    eot::outcome const endless = check_run("1\n", split, "0.5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_real(0.0, 1.0, "a");
        c.output.read_real(0.0, 1.0, "b");
        eo::accept();
    });
    EO_CHECK_EQ(endless.code, 1);
    EO_CHECK_IN(endless.text, "line 1, a: expected a number, found a token longer than 4096 characters: \"0.5000");

    std::string const longest = "0.5" + std::string(4093, '0') + " 0.25\n";
    eot::outcome const fits = check_run("1\n", longest, "0.5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK(c.output.read_real(0.0, 1.0, "a") == 0.5);
        EO_CHECK(c.output.read_real(0.0, 1.0, "b") == 0.25);
        c.jury.skip_rest("compared above");
        eo::accept();
    });
    EO_CHECK_EQ(fits.code, 0);
}

EO_TEST(text_reads_on_a_checker_stream) {
    eot::outcome const result = check_run("1\n", "abc YES a line of words\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK_EQ(c.output.read_token(1, 5, eo::charset("a-z"), "w"), std::string("abc"));
        EO_CHECK_EQ(c.output.read_choice({"YES", "NO"}, eo::any_case, "verdict"), std::string("YES"));
        EO_CHECK(!c.output.at_eoln());
        EO_CHECK_EQ(c.output.read_line(eo::any, "rest"), std::string("a line of words"));
        EO_CHECK(c.output.at_eof());
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
    EO_CHECK_EQ(result.code, 0);
    eot::outcome const cased = check_run("1\n", "yes\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK_EQ(c.output.read_choice({"YES", "NO"}, eo::any_case, "verdict"), std::string("YES"));
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
    EO_CHECK_EQ(cased.code, 0);
    eot::outcome const exact = check_run("1\n", "yes\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_choice({"YES", "NO"}, "verdict");
        eo::accept();
    });
    EO_CHECK_EQ(exact.code, 1);
    eot::outcome const bounded = check_run("1\n", "1 2 3\n0.5 1.5\nab cd\nzzz\n", "x\n",
                                           [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        std::vector<int> const values = c.output.read_ints(3, 1, 9, "a");
        EO_CHECK_EQ(values.size(), static_cast<std::size_t>(3));
        std::vector<double> const reals = c.output.read_reals(2, 0.0, 2.0, "r");
        EO_CHECK(reals[1] == 1.5);
        std::vector<std::string> const words = c.output.read_tokens(2, 1, 4, eo::charset("a-z"), "w");
        EO_CHECK_EQ(words[0], std::string("ab"));
        EO_CHECK_EQ(c.output.read_line(0, 0, eo::charset("z"), "gap"), std::string(""));
        EO_CHECK_EQ(c.output.read_line(1, 9, eo::charset("z"), "tail"), std::string("zzz"));
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
    EO_CHECK_EQ(bounded.code, 0);
}

EO_TEST(a_checker_stream_reads_an_edge_list_and_blames_the_side_it_came_from) {
    eot::outcome const read = check_run("3\n", "1 2\n2\n3\n", "1 2 5 2 3 6\n", [](eo::checker& c) {
        int const n = c.input.read_int(1, 9, "n");
        std::vector<eo::weighted_edge> const path = c.jury.read_edges(n - 1, n, eo::weighted(1, 9), "edge");
        EO_CHECK_EQ(path[1].w, 6);
        std::vector<eo::edge> const found = c.output.read_edges(n - 1, n, "edge");
        EO_CHECK_EQ(found[1].u, 2);
        EO_CHECK_EQ(found[1].v, 3);
        eo::accept();
    });
    EO_CHECK_EQ(read.code, 0);
    eot::outcome const outside = check_run("3\n", "1 4\n", "x\n", [](eo::checker& c) {
        c.output.read_edges(1, 3, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(outside.code, 1);
    EO_CHECK_IN(outside.text, "wrong answer: output.txt, line 1, edge[1]: 4 is above 3");
    eot::outcome const heavy = check_run("3\n", "x\n", "1 2 10\n", [](eo::checker& c) {
        c.jury.read_edges(1, 3, eo::weighted(1, 9), "edge");
        eo::accept();
    });
    EO_CHECK_EQ(heavy.code, 3);
    EO_CHECK_IN(heavy.text, "jury error: answer.txt, line 1, edge.w[1]: 10 is above 9");
    eot::outcome const negative = check_run("3\n", "x\n", "x\n", [](eo::checker& c) {
        c.output.read_edges(-1, 3, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(negative.code, 1);
    EO_CHECK_IN(negative.text, "edge: a count of -1 cannot be read");
}

EO_TEST(a_checker_stream_reads_a_grid_as_tokens) {
    eot::outcome const good = check_run("2 3\n", ".#.\n##.\n", "x\n", [](eo::checker& c) {
        std::vector<std::string> const grid = c.output.read_grid(2, 3, eo::charset(".#"), "grid");
        EO_CHECK_EQ(grid[0], std::string(".#."));
        c.jury.skip_rest("nothing to compare");
        eo::accept();
    });
    EO_CHECK_EQ(good.code, 0);
    eot::outcome const narrow = check_run("2 3\n", ".#.\n#.\n", "x\n", [](eo::checker& c) {
        c.output.read_grid(2, 3, eo::charset(".#"), "grid");
        eo::accept();
    });
    EO_CHECK_EQ(narrow.code, 1);
    EO_CHECK_IN(narrow.text, "wrong answer: output.txt, line 2, grid[2]: \"#.\" is 2 characters long, not 3..3");
    eot::outcome const flat = check_run("2 0\n", "\n", "x\n", [](eo::checker& c) {
        c.output.read_grid(2, 0, eo::charset(".#"), "grid");
        eo::accept();
    });
    EO_CHECK_EQ(flat.code, 3);
    EO_CHECK_IN(flat.text, "a grid on a stream has rows of at least one character, not 0");
}

EO_TEST(a_checker_stream_reads_a_tree_or_a_graph_as_a_validator_does) {
    eot::outcome const tree = check_run("3\n", "1 2 2 3\n", "2 1 5\n3 1 6\n", [](eo::checker& c) {
        int const n = c.input.read_int(1, 9, "n");
        std::vector<eo::weighted_edge> const star = c.jury.read_tree(n, eo::weighted(1, 9), "edge");
        EO_CHECK_EQ(star[1].w, 6);
        std::vector<eo::edge> const path = c.output.read_tree(n, "edge");
        EO_CHECK_EQ(path[1].v, 3);
        eo::accept();
    });
    EO_CHECK_EQ(tree.code, 0);
    eot::outcome const cycle = check_run("3\n", "1 2\n2 1\n", "x\n", [](eo::checker& c) {
        c.output.read_tree(3, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(cycle.code, 1);
    EO_CHECK_IN(cycle.text, "wrong answer: output.txt, line 2: edge: edges 1 and 2 are both (1, 2)");
    eot::outcome const outside = check_run("3\n", "x\n", "1 9 1\n", [](eo::checker& c) {
        c.jury.read_tree(2, eo::weighted(1, 9), "edge");
        eo::accept();
    });
    EO_CHECK_EQ(outside.code, 3);
    EO_CHECK_IN(outside.text, "jury error: answer.txt, line 1: edge: edge 1 is (1, 9), outside 1..2");
    eot::outcome const graph = check_run("3\n", "1 2 2 3 1 3\n", "1 2 4\n", [](eo::checker& c) {
        EO_CHECK_EQ(c.output.read_graph(3, 3, eo::simple | eo::connected, "edge").size(), static_cast<std::size_t>(3));
        EO_CHECK_EQ(c.jury.read_graph(3, 1, eo::any_graph, eo::weighted(1, 9), "edge")[0].w, 4);
        eo::accept();
    });
    EO_CHECK_EQ(graph.code, 0);
    eot::outcome const split = check_run("3\n", "1 2\n", "x\n", [](eo::checker& c) {
        c.output.read_graph(3, 1, eo::connected, eo::unnamed);
        eo::accept();
    });
    EO_CHECK_EQ(split.code, 1);
    EO_CHECK_IN(split.text, "wrong answer: output.txt, line 1: vertex 3 cannot be reached from vertex 1");
    eot::outcome const huge = check_run("3\n", "1 2\n", "x\n", [](eo::checker& c) {
        c.output.read_graph(5, 2000000000, eo::any_graph, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(huge.code, 1);
    EO_CHECK_IN(huge.text, "wrong answer: output.txt");
    eot::outcome const negative = check_run("3\n", "1 2\n", "x\n", [](eo::checker& c) {
        c.output.read_graph(5, -1, eo::any_graph, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(negative.code, 1);
    EO_CHECK_IN(negative.text, "edge: a count of -1 cannot be read");
    eot::outcome const empty = check_run("3\n", "\n", "x\n", [](eo::checker& c) {
        c.output.read_tree(0, "edge");
        eo::accept();
    });
    EO_CHECK_EQ(empty.code, 3);
    EO_CHECK_IN(empty.text, "read_tree needs a tree of at least one vertex, not 0");
    EO_CHECK_EQ(check_run("3\n", "\n", "x\n", [](eo::checker& c) {
                    c.output.read_tree(0, eo::weighted(1, 2), "edge");
                }).code, 3);
    eot::outcome const doubled = check_run("3\n", "1 2 5 2 1 5\n", "x\n", [](eo::checker& c) {
        c.output.read_graph(3, 2, eo::simple, eo::weighted(1, 9), "edge");
        eo::accept();
    });
    EO_CHECK_IN(doubled.text, "edge: edges 1 and 2 are both (1, 2)");
}

EO_TEST(an_exit_before_the_verdict_is_a_jury_error) {
    eot::outcome const result = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        (void)c.output.read_int(eo::any, "x");
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("jury error the checker ended without a verdict: exit() was called, "
                                         "or the checker was never destroyed"));
    EO_CHECK(eo::detail::unfinished() == nullptr);
    eo::detail::finish_what_exit_left();
}

EO_TEST(a_checker_stream_reads_a_token_or_a_line_by_its_length_alone) {
    eot::outcome const result = check_run("1\n", "a line, of words\nab!c\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        EO_CHECK_EQ(c.output.read_line(1, 20, "line"), std::string("a line, of words"));
        EO_CHECK_EQ(c.output.read_token(1, 4, "w"), std::string("ab!c"));
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
    EO_CHECK_EQ(result.code, 0);
    eot::outcome const token = check_run("1\n", "abcde\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_token(1, 4, "w");
        eo::accept();
    });
    EO_CHECK_EQ(token.code, 1);
    EO_CHECK_EQ(token.text, std::string("wrong answer: output.txt, line 1, w: \"abcde\" is longer than 4 characters"));
    eot::outcome const line = check_run("1\n", "abcde\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_line(6, 9, "rest");
        eo::accept();
    });
    EO_CHECK_EQ(line.code, 1);
    EO_CHECK_EQ(line.text,
                std::string("wrong answer: output.txt, line 1, rest: the line is 5 characters long, not 6..9"));
}

EO_TEST(loose_reads_on_a_checker_stream_are_warned_about) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    eot::outcome const result = check_run("1\n", "1 2 3 4.5 4 200001 6\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int("a");
        c.output.read_int(1, 9);
        c.output.read_real(eo::any, "r");
        c.output.read_real("loose");
        c.output.read_int(-2147483648LL, 2147483647LL, "whole");
        c.output.read_int(1, 200001, "nearly");
        c.output.read_int(1, 8, "nearly");
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
    EO_CHECK_EQ(result.code, 0);
    std::string const said = checker_warnings();
    EO_CHECK_IN(said, "warning EO103: this value is read without bounds");
    EO_CHECK_IN(said, "warning EO101: this value is read without a name");
    EO_CHECK_IN(said, "warning EO104: the bounds are the whole range of an int");
    EO_CHECK_IN(said, "note EO106: the bounds 1..200001 are one away from a round number");
    EO_CHECK_IN(said, "warning EO107: \"nearly\" is read as 1..8 here and as 1..200001 elsewhere");
}

EO_TEST(a_huge_token_from_the_contestant_is_noted) {
    std::string const huge(1024 * 1024 + 5, 'z');
    eot::outcome const result = check_run("1\n", huge + "\n", "x\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_token(eo::any, "w");
        c.jury.read_token(eo::any, "x");
        eo::accept();
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(checker_warnings(), "note EO111: a token of 1048581 bytes was held in memory");
}

EO_TEST(cases_that_finish_put_the_numbering_back) {
    eot::outcome const result = check_run("2\n", "1 2\n", "1 2\n", [](eo::checker& c) {
        int const t = c.input.read_int(1, 9, "t");
        c.cases(t, [&] {
            long long const want = c.jury.read_long(eo::any, "answer");
            long long const got = c.output.read_long(eo::any, "answer");
            if (got != want) eo::wrong("case mismatch");
        });
        EO_CHECK_EQ(eo::detail::current_case(), 0LL);
        eo::accept("{} cases", t);
    });
    EO_CHECK_EQ(result.code, 0);
}

EO_TEST(a_checker_near_its_time_limit_is_reported) {
    eot::outcome const quick = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(quick.code, 0);
    EO_CHECK(!eot::contains(checker_warnings(), "EO209"));

    eot::outcome const slow = check_run("1\n", "5\n", "5\n", [](eo::checker& c) {
        c.input.read_int(1, 9, "n");
        c.output.read_int(1, 9, "a");
        eo::detail::diagnostics::shared().clock().started -= std::chrono::seconds(6);
        eo::accept();
    });
    EO_CHECK_EQ(slow.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO209: the checker ran for ");
    EO_CHECK_IN(checker_warnings(), " ms of its 10000 ms limit");

    eot::outcome const garbled = check_run("1\n", "x\n", "5\n", [](eo::checker& c) {
        eo::detail::diagnostics::shared().clock().started -= std::chrono::seconds(6);
        c.output.read_int(1, 9, "a");
        eo::accept();
    });
    EO_CHECK_EQ(garbled.code, 1);
    EO_CHECK_IN(checker_warnings(), "warning EO209: the checker ran for ");
}

EO_TEST(a_stream_reads_what_matches_a_pattern_and_blames_its_side) {
    eo::pattern const word("[a-z]{1,5}");
    eot::outcome const good = check_run("1\n", "  hello  \nab cd\nsome words here\r\n", "yes\n", [&](eo::checker& c) {
        c.input.read_int(1, 1, "n");
        c.jury.read_token(eo::pattern("yes|no"), "answer");
        EO_CHECK_EQ(c.output.read_token(word, "w"), std::string("hello"));
        EO_CHECK(c.output.read_tokens(2, word, "pair") == std::vector<std::string>({"ab", "cd"}));
        EO_CHECK_EQ(c.output.read_line(eo::pattern("( ?[a-z]+)*"), "rest"), std::string(""));
        EO_CHECK_EQ(c.output.read_line(eo::pattern("[a-z]+( [a-z]+)*"), "line"), std::string("some words here"));
        eo::accept();
    });
    EO_CHECK_EQ(good.code, 0);
    eot::outcome const theirs = check_run("1\n", "heLLo\n", "yes\n", [&](eo::checker& c) {
        c.output.read_token(word, "w");
        eo::accept();
    });
    EO_CHECK_EQ(theirs.code, 1);
    EO_CHECK_EQ(theirs.text, std::string("wrong answer: output.txt, line 1, w: \"heLLo\" does not match \"[a-z]{1,5}\""));
    eot::outcome const ours = check_run("1\n", "yes\n", "nay\n", [&](eo::checker& c) {
        c.jury.read_token(eo::pattern("yes|no"), "answer");
        eo::accept();
    });
    EO_CHECK_EQ(ours.code, 3);
    EO_CHECK_EQ(ours.text, std::string("jury error: answer.txt, line 1, answer: \"nay\" does not match \"yes|no\""));
    eot::outcome const longer = check_run("1\n", "abcdefgh\n", "yes\n", [&](eo::checker& c) {
        c.output.read_line(word, "line");
        eo::accept();
    });
    EO_CHECK_EQ(longer.text, std::string("wrong answer: output.txt, line 1, line: a line that starts \"abcdef\" is "
                                         "longer than the 5 characters \"[a-z]{1,5}\" allows"));
    eot::outcome const unended = check_run("1\n", "abc", "yes\n", [&](eo::checker& c) {
        EO_CHECK_EQ(c.output.read_line(word, "line"), std::string("abc"));
        eo::accept();
    });
    EO_CHECK_EQ(unended.code, 0);
}

namespace {

std::string compared_by(std::string const& out, std::string const& ans, void (*compare)(eo::checker&)) {
    return check_run("1\n", out, ans, [&](eo::checker& c) {
               c.input.skip_rest("not needed");
               compare(c);
           })
        .text;
}

}  // namespace

EO_TEST(tokens_in_any_case_ignore_the_case_of_letters_only) {
    auto const folded = [](eo::checker& c) { c.tokens(eo::any_case); };
    EO_CHECK_EQ(compared_by("Hello  WORLD\nx9_\n", "hello world X9_", folded), std::string("ok 3 tokens"));
    EO_CHECK_EQ(compared_by("hello wurld", "hello world", folded),
                std::string("wrong answer token 2 is \"wurld\", expected \"world\" in any case"));
    EO_CHECK_EQ(compared_by("\xc3\xa9", "\xc3\x89", folded),
                std::string("wrong answer token 1 is \"\xc3\xa9\", expected \"\xc3\x89\" in any case"));
    EO_CHECK_EQ(compared_by("ab abc", "AB ab", folded),
                std::string("wrong answer token 2 is longer than the expected \"ab\"; it starts \"abc\""));
    EO_CHECK_EQ(compared_by("a", "a b", folded),
                std::string("wrong answer the output ended after 1 tokens, the answer has more"));
    EO_CHECK_EQ(compared_by("a b c", "A B", folded), std::string("wrong answer the answer has 2 tokens, the output has more"));
    EO_CHECK_EQ(compared_by("[@", "{`", folded), std::string("wrong answer token 1 is \"[@\", expected \"{`\" in any case"));
}

EO_TEST(tokens_in_any_order_compare_the_answer_and_the_output_as_multisets) {
    auto const unordered = [](eo::checker& c) { c.tokens(eo::any_order); };
    EO_CHECK_EQ(compared_by("3 1\n2\n", "1 2 3", unordered), std::string("ok 3 tokens in any order"));
    EO_CHECK_EQ(compared_by("", "\n", unordered), std::string("ok 0 tokens in any order"));
    EO_CHECK_EQ(compared_by("1 1 2", "2 1 2", unordered),
                std::string("wrong answer \"1\" is in the answer once and in the output 2 times"));
    EO_CHECK_EQ(compared_by("1 2 4", "3 2 1", unordered),
                std::string("wrong answer \"3\" is in the answer once and in the output 0 times"));
    EO_CHECK_EQ(compared_by("b a", "a b c", unordered), std::string("wrong answer the output has 2 tokens, the answer has 3"));
    EO_CHECK_EQ(compared_by("a b c d", "c b a", unordered),
                std::string("wrong answer the answer has 3 tokens, the output has more"));
    EO_CHECK_EQ(compared_by("x", "", unordered), std::string("wrong answer the answer has 0 tokens, the output has more"));
    EO_CHECK_EQ(compared_by("ab abcd", "ab abc", unordered),
                std::string("wrong answer token 2 is longer than every token of the answer; it starts \"abcd\""));
    EO_CHECK_EQ(compared_by("A", "a", unordered), std::string("wrong answer \"A\" is in the answer 0 times and in the output once"));
}

EO_TEST(integers_are_compared_as_numbers_one_by_one) {
    auto const numeric = [](eo::checker& c) { c.integers(); };
    EO_CHECK_EQ(compared_by("1 -2\n3\n", "1 -2 3", numeric), std::string("ok 3 integers"));
    EO_CHECK_EQ(compared_by("", "", numeric), std::string("ok 0 integers"));
    EO_CHECK_EQ(compared_by("1 5 3", "1 2 3", numeric), std::string("wrong answer integer 2 is 5, expected 2"));
    EO_CHECK_EQ(compared_by("1", "1 2", numeric),
                std::string("wrong answer the output ended after 1 integers, the answer has more"));
    EO_CHECK_EQ(compared_by("1 2 x", "1 2", numeric),
                std::string("wrong answer the answer has 2 integers, the output has more"));
    EO_CHECK_EQ(compared_by("1 07", "1 7", numeric),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"07\": it has a leading "
                            "zero"));
    EO_CHECK_EQ(compared_by("1 99999999999999999999", "1 7", numeric),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"99999999999999999999\": "
                            "it does not fit the type it is read into"));
    eot::outcome const jury = check_run("1\n", "1\n", "one\n", [](eo::checker& c) {
        c.input.skip_rest("not needed");
        c.integers();
    });
    EO_CHECK_EQ(jury.code, 3);
    EO_CHECK_EQ(jury.text, std::string("jury error: answer.txt, line 1: expected an integer, found \"one\": it has a "
                                       "character that cannot be part of the number"));
    eot::outcome const loose = check_run("1\n", "+5 007 -0\n", "5 7 0\n", [](eo::checker& c) {
        c.input.skip_rest("not needed");
        c.output.numbers(eo::lenient);
        c.integers();
    });
    EO_CHECK_EQ(loose.text, std::string("ok 3 integers"));
}

EO_TEST(big_integers_are_compared_exactly_at_any_length) {
    auto const huge = [](eo::checker& c) { c.integers(eo::big); };
    std::string const googol = "1" + std::string(100, '0');
    EO_CHECK_EQ(compared_by(googol + "\n-" + googol + " 0\n", googol + " -" + googol + " 0", huge),
                std::string("ok 3 integers"));
    EO_CHECK_EQ(compared_by("123456789012345678901234567890 5", "123456789012345678901234567890 6", huge),
                std::string("wrong answer integer 2 is 5, expected 6"));
    EO_CHECK_EQ(compared_by(googol + "1", googol + "2", huge),
                std::string("wrong answer integer 1 is 102 characters long and the answer's 102; they differ first "
                            "at character 102"));
    EO_CHECK_EQ(compared_by("-" + googol, googol, huge),
                std::string("wrong answer integer 1 is 102 characters long and the answer's 101; they differ first "
                            "at character 1"));
    EO_CHECK_EQ(compared_by("1", "1 2", huge),
                std::string("wrong answer the output ended after 1 integers, the answer has more"));
    EO_CHECK_EQ(compared_by("1 2", "1", huge), std::string("wrong answer the answer has 1 integers, the output has more"));
    EO_CHECK_EQ(compared_by("-0", "0", huge),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"-0\": it is zero written "
                            "with a minus"));
    EO_CHECK_EQ(compared_by("1 012", "1 12", huge),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"012\": it has a leading zero"));
    EO_CHECK_EQ(compared_by("1 1x", "1 12", huge),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"1x\": it has a character "
                            "that cannot be part of the number"));
    EO_CHECK_EQ(compared_by("+", "1", huge),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"+\": it has a character "
                            "that cannot be part of the number"));
    EO_CHECK_EQ(compared_by("-", "1", huge),
                std::string("wrong answer: output.txt, line 1: expected an integer, found \"-\": it has no digits "
                            "where digits are required"));
    EO_CHECK_EQ(compared_by(std::string(5000, '7'), "7", huge),
                "wrong answer token 1 is longer than 4096 characters: \"" + std::string(40, '7') + "...\"");
    EO_CHECK_EQ(compared_by("1", "0x1", huge),
                std::string("jury error: answer.txt, line 1: expected an integer, found \"0x1\": it has a character "
                            "that cannot be part of the number"));
    eot::outcome const loose = check_run("1\n", "+5 0007 -000 -01\n", "5 7 0 -1\n", [](eo::checker& c) {
        c.input.skip_rest("not needed");
        c.output.numbers(eo::lenient);
        c.integers(eo::big);
    });
    EO_CHECK_EQ(loose.text, std::string("ok 4 integers"));
}

EO_TEST(yes_no_without_a_certificate_compares_every_word_in_any_case) {
    auto const words = [](eo::checker& c) { c.yes_no(); };
    EO_CHECK_EQ(compared_by("Yes\nno\nYES\n", "YES NO yes", words), std::string("ok 3 answers, 2 of them YES"));
    EO_CHECK_EQ(compared_by("no", "NO", words), std::string("ok 1 answers, 0 of them YES"));
    EO_CHECK_EQ(compared_by("yes yes", "YES NO", words), std::string("wrong answer answer 2 is YES, expected NO"));
    EO_CHECK_EQ(compared_by("yes maybe", "YES NO", words),
                std::string("wrong answer: output.txt, line 1: \"maybe\" is not one of YES, NO"));
    EO_CHECK_EQ(compared_by("yes", "YES NO", words),
                std::string("wrong answer the output ended after 1 answers, the answer has more"));
    EO_CHECK_EQ(compared_by("yes no no", "YES NO", words),
                std::string("wrong answer the answer has 2 answers, the output has more"));
    EO_CHECK_EQ(compared_by("yes", "Perhaps", words),
                std::string("jury error: answer.txt, line 1: \"Perhaps\" is not one of YES, NO"));
}

EO_TEST(reals_within_an_absolute_error_ignore_the_relative_one) {
    auto const absolute = [](eo::checker& c) { c.reals(1.5e-6, eo::absolute); };
    EO_CHECK_EQ(compared_by("1.0000014 2 x", "1 2 x", absolute), std::string("ok 3 values"));
    EO_CHECK_EQ(compared_by("1000000.5", "1000000", absolute),
                std::string("wrong answer value 1 is 1000000.5, expected 1000000"));
    EO_CHECK_EQ(compared_by("1000000.5", "1000000", [](eo::checker& c) { c.reals(1.5e-6); }), std::string("ok 1 values"));
    EO_CHECK_EQ(compared_by("1 2", "1", absolute), std::string("wrong answer the answer has 1 tokens, the output has more"));
    EO_CHECK_EQ(compared_by("y", "x", absolute), std::string("wrong answer token 1 is \"y\", expected \"x\""));
    auto const micro = [](eo::checker& c) { c.reals(1e-6, eo::absolute); };
    EO_CHECK_EQ(compared_by("0.500001 0.499999", "0.5 0.5", micro), std::string("ok 2 values"));
    EO_CHECK_EQ(compared_by("0.5000011", "0.5", micro), std::string("wrong answer value 1 is 0.5000011, expected 0.5"));
}

namespace {

int& replaced_handler_calls() {
    static int count = 0;
    return count;
}

void replaced_handler() { replaced_handler_calls()++; }

}  // namespace

EO_TEST(an_exception_nothing_caught_ends_a_role_as_a_jury_error) {
    eot::outcome const thrown = check_run("1\n", "1\n", "1\n", [](eo::checker&) {
        try {
            throw std::out_of_range("vector::at: 7 >= 3");
        } catch (...) {
            eo::detail::scorer::ended_by_terminate();
        }
    });
    EO_CHECK_EQ(thrown.code, 3);
    EO_CHECK_EQ(thrown.text, std::string("jury error an exception nothing caught ended the checker: vector::at: 7 >= 3"));
    eot::outcome const odd = check_run("1\n", "1\n", "1\n", [](eo::checker&) {
        try {
            throw 7;
        } catch (...) {
            eo::detail::scorer::ended_by_terminate();
        }
    });
    EO_CHECK_EQ(odd.text, std::string("jury error an exception nothing caught ended the checker, and it is not a "
                                      "std::exception"));
    eot::outcome const called = check_run("1\n", "1\n", "1\n", [](eo::checker&) {
        eo::detail::scorer::ended_by_terminate();
    });
    EO_CHECK_EQ(called.text, std::string("jury error std::terminate ended the checker before its verdict"));
    eot::outcome const long_winded = check_run("1\n", "1\n", "1\n", [](eo::checker&) {
        try {
            throw std::runtime_error(std::string(300, 'w'));
        } catch (...) {
            eo::detail::scorer::ended_by_terminate();
        }
    });
    EO_CHECK_EQ(long_winded.text, "jury error an exception nothing caught ended the checker: " + std::string(200, 'w') + "...");
    void (*const kept)() = eo::detail::terminate_before();
    eo::detail::terminate_before() = &replaced_handler;
    eo::detail::scorer::ended_by_terminate();
    eo::detail::terminate_before() = kept;
    EO_CHECK_EQ(replaced_handler_calls(), 1);
    EO_CHECK(std::get_terminate() == &eo::detail::scorer::ended_by_terminate);
}
