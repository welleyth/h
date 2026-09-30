namespace {

std::string handoff_for(long long which, long long total, std::string const& test,
                        std::string const& payload, std::string const& carried = "") {
    std::string head = eo::fmt("eolymp-phase {} {}", which, total);
    if (!carried.empty()) head += " " + carried;
    return head + eo::fmt("\ninput {}\n", test.size()) + test + "\n" + payload;
}

}  // namespace

EO_TEST(a_first_phase_hands_the_next_one_its_work) {
    chat const result = talk("7\n", "0101\n", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(ph.number(), 1LL);
        long long const x = it.input.read_long(0, 99, "x");
        it.send("alice", x);
        std::string const code = it.contestant.read_token(1, 9, eo::charset("01"), "code");
        ph.handoff([&](eo::writer& w) {
            w.line(code);
            w.line();
        });
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok handed on to the next phase"));
    EO_CHECK_EQ(result.said, std::string("alice 7\n"));
    EO_CHECK_IN(result.summary, "eolymp-phase 2 2\ninput 2\n7\n\n0101\n\n");
}

#if !defined(_WIN32)
EO_TEST(a_handoff_that_cannot_be_written_whole_is_a_jury_error) {
    file_size_limit disk;
    chat const result = talk("7\n", "0101\n", [&](eo::interactor& it) {
        eo::phases ph(it, 2);
        it.input.read_long(0, 99, "x");
        std::string const code = it.contestant.read_token(1, 9, eo::charset("01"), "code");
        disk.cap(20);
        ph.handoff([&](eo::writer& w) { w.line(code); });
    });
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "eolymp.h: the handoff could not be written to ");
    EO_CHECK_EQ(result.summary.size(), static_cast<std::size_t>(20));
}
#endif

EO_TEST(a_later_phase_reads_the_test_and_what_was_handed_on) {
    chat const result = talk(handoff_for(2, 2, "7\n", "0101\n"), "7\n", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(ph.number(), 2LL);
        long long const x = it.input.read_long(0, 99, "x");
        EO_CHECK_EQ(x, 7LL);
        std::string const code = ph.previous().read_token(1, 9, eo::charset("01"), "code");
        EO_CHECK_EQ(code, std::string("0101"));
        it.send("bob", code);
        long long const said = it.contestant.read_long(eo::any, "x");
        if (said != x) eo::wrong("answered {}", said);
        eo::accept("{} characters", code.size());
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_EQ(result.text, std::string("ok 4 characters"));
    EO_CHECK_IN(result.summary, "fraction 1");
}

EO_TEST(every_phase_draws_from_the_same_seed_the_test_gives) {
    long long const expected = eo::rng(eo::detail::seed_of("7\n")).uniform(1, 1000000000);
    long long first = 0;
    long long later = 0;
    talk("7\n", "", [&](eo::interactor& it) {
        eo::phases ph(it, 2);
        first = it.rng().uniform(1, 1000000000);
        it.input.read_long(0, 99, "x");
        ph.handoff([](eo::writer& w) { w.line("rest"); });
    });
    talk(handoff_for(2, 2, "7\n", "rest\n"), "", [&](eo::interactor& it) {
        eo::phases ph(it, 2);
        later = it.rng().uniform(1, 1000000000);
        it.input.read_long(0, 99, "x");
        eo::accept();
    });
    EO_CHECK_EQ(first, expected);
    EO_CHECK_EQ(later, expected);
}

EO_TEST(a_partial_score_from_an_early_phase_is_carried_on) {
    chat const first = talk("7\n", "0101\n", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        it.input.read_long(0, 99, "x");
        it.contestant.read_token(1, 9, eo::charset("01"), "code");
        ph.finish(0.25, "too long to go on");
    });
    EO_CHECK_EQ(first.code, 0);
    EO_CHECK_IN(first.summary, "eolymp-phase 2 2 finished 0.25 too long to go on");

    chat const second = talk(first.summary, "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(ph.number(), 2LL);
        eo::accept("never reached");
    });
    EO_CHECK_EQ(second.code, 0);
    EO_CHECK_EQ(second.text, std::string("ok too long to go on"));
    EO_CHECK_IN(second.summary, "fraction 0.25");
    EO_CHECK(!eot::contains(checker_warnings(), "EO405"));

    chat const straight = talk("7\n", "0101\n", [](eo::interactor& it) {
        eo::phases ph(it, 1);
        it.input.read_long(0, 99, "x");
        it.contestant.read_token(1, 9, eo::charset("01"), "code");
        ph.finish(0.5, "half on the last phase");
    });
    EO_CHECK_EQ(straight.code, 0);
    EO_CHECK_IN(straight.summary, "fraction 0.5");
}

EO_TEST(the_phases_and_run_count_have_to_agree) {
    chat const overrun = talk("eolymp-summary 1\nfraction 1\nmessage done\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(ph.number(), 1LL);
        eo::accept();
    });
    EO_CHECK_EQ(overrun.code, 3);
    EO_CHECK_IN(overrun.text, "run_count is more than the 2 phases the interactor declares");

    chat const last = talk(handoff_for(2, 2, "7\n", "0101\n"), "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        ph.handoff([](eo::writer& w) { w.line("nothing"); });
    });
    EO_CHECK_EQ(last.code, 3);
    EO_CHECK_IN(last.text, "phase 2 of 2 cannot hand off");

    chat const mismatch = talk(handoff_for(2, 3, "7\n", "0101\n"), "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(ph.number(), 2LL);
        eo::accept();
    });
    EO_CHECK_EQ(mismatch.code, 3);
    EO_CHECK_IN(mismatch.text, "the previous run declared 3 phases and this one declares 2");

    chat const none = talk("7\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 0);
        EO_CHECK_EQ(ph.number(), 1LL);
        eo::accept();
    });
    EO_CHECK_IN(none.text, "a run has at least one phase");
}

EO_TEST(a_handoff_this_release_cannot_read_is_a_jury_error) {
    chat const head = talk("eolymp-phase two\ninput 2\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_IN(head.text, "left a header this release cannot read");

    chat const worded = talk("eolymp-phase two 2\ninput 2\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_IN(worded.text, "left a header this release cannot read");

    chat const sized = talk("eolymp-phase 2 2\nsize 2\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_IN(sized.text, "left no test in its handoff");

    chat const counted = talk("eolymp-phase 2 2\ninput two\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_IN(counted.text, "no size on its test");

    chat const paid = talk(handoff_for(2, 2, "7\n", "", "finished half the way"), "",
                           [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_IN(paid.text, "left a header this release cannot read");
}

EO_TEST(a_handoff_cut_short_is_a_jury_error) {
    for (char const* carried : {"eolymp-phase 2 2\ninput 50\n7\n\n", "eolymp-phase 2 2\ninput 2",
                                "eolymp-phase 2 2\ninput 2\n7\n", "eolymp-phase 2 2\ninput 2\n7"}) {
        chat const cut = talk(carried, "", [](eo::interactor& it) {
            eo::phases ph(it, 2);
            (void)ph.number();
            eo::accept();
        });
        EO_CHECK_EQ(cut.code, 3);
        EO_CHECK_IN(cut.text, "the previous phase left a handoff cut short of its ");
    }
    chat const negative = talk("eolymp-phase 2 2\ninput -5\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        (void)ph.number();
        eo::accept();
    });
    EO_CHECK_EQ(negative.code, 3);
    EO_CHECK_IN(negative.text, "the previous phase left a handoff whose test has a size of -5 bytes");
    chat const whole = talk("eolymp-phase 2 2\ninput 2\n7\n\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        EO_CHECK_EQ(it.input.read_long(0, 99, "x"), 7LL);
        EO_CHECK(ph.previous().at_eof());
        eo::accept();
    });
    EO_CHECK_EQ(whole.code, 0);
}

EO_TEST(a_handoff_too_big_to_copy_is_reported) {
    chat const result = talk("7\n", "", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        it.input.read_long(0, 99, "x");
        ph.handoff([](eo::writer& w) { w.line(std::string(64u * 1024 * 1024 + 8, 'x')); });
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(checker_warnings(), "warning EO407: this handoff is");
}

EO_TEST(a_note_with_a_line_break_does_not_split_the_handoff) {
    chat const result = talk("7\n", "0101\n", [](eo::interactor& it) {
        eo::phases ph(it, 2);
        it.input.read_long(0, 99, "x");
        ph.finish(0.5, "stopped early\nand said\rmore");
        ph.handoff([](eo::writer& w) { w.line("rest"); });
    });
    EO_CHECK_EQ(result.code, 0);
    EO_CHECK_IN(result.summary, "eolymp-phase 2 2 finished 0.5 stopped early and said more\ninput ");
    std::size_t const first_break = result.summary.find('\n');
    EO_CHECK(result.summary.substr(0, first_break).find("more") != std::string::npos);
}
