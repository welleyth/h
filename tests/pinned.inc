namespace {

std::size_t const every_chunk[] = {64, 65, 66, 67, 70, 71, 77, 90, 100, 127, 128, 1u << 20};

std::string padded(std::string const& text, std::size_t at) {
    std::string pad;
    for (std::size_t one = 0; one < at; one++) pad += "1 ";
    return pad + text;
}

std::string strict_integers(std::string const& text, std::size_t chunk) {
    std::string trace;
    eot::outcome const ended = eot::run_until_stop([&] {
        eo::detail::reader from(eo::detail::source::over_text(text, true, chunk), eo::detail::fault::invalid_test,
                                "", false, "EO102");
        while (from.peek() >= 0) {
            trace += eo::fmt("{} ", from.whole_long(0, 0, eo::detail::stated::deliberate, "a",
                                                    eo::detail::site{"pinned.inc", 1}));
            if (from.peek() == ' ' || from.peek() == '\n') {
                from.take();
                from.mark_separated();
            }
        }
        trace += eo::fmt("at line {}", from.line());
    });
    if (ended.stopped) trace += eo::fmt("| {} {}", ended.code, ended.text);
    return trace;
}

std::string lenient_integers(std::string const& text, std::size_t chunk, bool normalize, bool relaxed) {
    std::string trace;
    eot::outcome const ended = eot::run_until_stop([&] {
        eo::stream from(eo::detail::source::over_text(text, normalize, chunk), eo::detail::fault::wrong_answer,
                        "output.txt");
        if (relaxed) from.numbers(eo::lenient);
        while (!from.at_eof()) trace += eo::fmt("{} ", from.read_long(eo::any, "a"));
    });
    if (ended.stopped) trace += eo::fmt("| {} {}", ended.code, ended.text);
    return trace;
}

using reading = std::string (*)(std::string const&, std::size_t);

void pin_everywhere(std::string const& expected, std::string const& text, reading read) {
    for (std::size_t lead = 0; lead < 40; lead += 13) {
        std::string ones;
        for (std::size_t one = 0; one < lead; one++) ones += "1 ";
        for (std::size_t const chunk : every_chunk) EO_CHECK_EQ(read(padded(text, lead), chunk), ones + expected);
    }
}

void pin_lines_everywhere(std::string const& expected, std::string const& text, reading read) {
    for (std::size_t lead = 0; lead < 130; lead += 29)
        for (std::size_t const chunk : every_chunk)
            EO_CHECK_EQ(read(std::string(lead, 'p') + "\n" + text, chunk), expected);
}

std::string lenient_plain(std::string const& text, std::size_t chunk) {
    return lenient_integers(text, chunk, false, false);
}

std::string lenient_normalized(std::string const& text, std::size_t chunk) {
    return lenient_integers(text, chunk, true, false);
}

std::string lenient_relaxed(std::string const& text, std::size_t chunk) {
    return lenient_integers(text, chunk, false, true);
}

}  // namespace

EO_TEST(pin_a_validator_reading_integers_wherever_the_buffer_ends) {
    pin_everywhere("7 -12 0 123456789012345678 -123456789012345678 999999999999999999 1000000000000000000 "
                   "-1000000000000000000 9223372036854775807 -9223372036854775808 at line 2",
                   "7 -12 0 123456789012345678 -123456789012345678 999999999999999999 1000000000000000000 "
                   "-1000000000000000000 9223372036854775807 -9223372036854775808\n",
                   strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"007\": it has a leading zero", "007\n",
                   strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"-0\": it is zero written with a minus", "-0\n",
                   strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"+5\"", "+5\n", strict_integers);
    pin_everywhere("12 | 3 line 1, a: expected an integer, found \"a\"", "12a\n", strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"-\"", "-\n", strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"9223372036854775808\": it does not fit the type it "
                   "is read into",
                   "9223372036854775808\n", strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"-9223372036854775809\": it does not fit the type "
                   "it is read into",
                   "-9223372036854775809\n", strict_integers);
    pin_everywhere("| 3 line 1, a: expected an integer, found \"12345678901234567890123\": it does not fit the "
                   "type it is read into",
                   "12345678901234567890123\n", strict_integers);
    pin_everywhere("1 | 3 line 1, a: expected an integer, found a space", "1  2\n", strict_integers);
    pin_everywhere("5 6 at line 3", "5\r\n6\r\n", strict_integers);
    pin_everywhere("5 | 3 pinned.inc:1: line 1, a: a carriage return follows a; read it with read_space()",
                   "5\r6\n", strict_integers);
    pin_everywhere("1 2 3 at line 1", "1 2 3", strict_integers);
    pin_everywhere("5 | 3 line 1, a: expected an integer, found \".5\"", "5.5\n", strict_integers);
}

EO_TEST(pin_a_stream_reading_integers_wherever_the_buffer_ends) {
    pin_everywhere("7 -12 0 123456789012345678 5 999999999999999999 1000000000000000000 -5 ",
                   "7\t-12  0\r\n123456789012345678\n\n5 999999999999999999 1000000000000000000 -5",
                   lenient_plain);
    pin_everywhere("7 | 1 wrong answer: output.txt, line 1, a: expected an integer, found \"007\": it has a "
                   "leading zero",
                   "7 007 8\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"-0\": it is zero written "
                   "with a minus",
                   "-0\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"+5\": it has a character "
                   "that cannot be part of the number",
                   "+5\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"12abc\": it has a "
                   "character that cannot be part of the number",
                   "12abc 5\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"-\": it has no digits "
                   "where digits are required",
                   "- 5\n", lenient_plain);
    pin_everywhere("5 6 ", "5\r\n6\r\n", lenient_normalized);
    pin_everywhere("5 6 ", "5\r\n6\r\n", lenient_plain);
    pin_everywhere("5 7 0 0 -7 ", "+5 007 -0 +0 -007\n", lenient_relaxed);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"9223372036854775808\": it "
                   "does not fit the type it is read into",
                   "9223372036854775808\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found a token longer than 4096 "
                   "characters: \"1111111111111111111111111111111111111111...\"",
                   std::string(4097, '1') + "\n", lenient_plain);
    pin_everywhere("| 1 wrong answer: output.txt, line 1, a: expected an integer, found \"" + std::string(40, '1') +
                       "...\": it does not fit the type it is read into",
                   std::string(4096, '1') + "\n", lenient_plain);
}

EO_TEST(pin_the_warnings_and_counts_of_bounds_read_again_and_again) {
    eot::outcome const result = validate("1 2 3\n4 5 6\n7 8 9\n1 2 3\n5 6 7\n8 9 1\n", [](eo::validator& v) {
        v.read_ints(3, 1, 999999, "a", eo::detail::site{"pinned.inc", 10});
        v.read_eoln();
        v.read_ints(3, 1, 1000000, "b", eo::detail::site{"pinned.inc", 11});
        v.read_eoln();
        v.read_ints(3, -2147483647 - 1, 2147483647, "c", eo::detail::site{"pinned.inc", 12});
        v.read_eoln();
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_long(1, 3000000000LL, "d", eo::detail::site{"pinned.inc", 13});
        }
        v.read_eoln();
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_int(1, 3000000000LL, "e", eo::detail::site{"pinned.inc", 14});
            v.read_int(1, 1000000, "b", eo::detail::site{"pinned.inc", 15});
            if (at < 2) v.read_space();
        }
        v.read_eoln();
    });
    EO_CHECK_EQ(result.text, std::string("pinned.inc:15: line 5, b: a space follows e; read it with read_space()"));
    eot::outcome const kept = validate("1 2 3\n4 5 6\n7 8 9\n1 2 3\n5 6 7 8 9 1\nxy\n", [](eo::validator& v) {
        v.read_ints(3, 1, 999999, "a", eo::detail::site{"pinned.inc", 10});
        v.read_eoln();
        v.read_ints(3, 1, 1000000, "b", eo::detail::site{"pinned.inc", 11});
        v.read_eoln();
        v.read_ints(3, -2147483647 - 1, 2147483647, "c", eo::detail::site{"pinned.inc", 12});
        v.read_eoln();
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_long(1, 3000000000LL, "d", eo::detail::site{"pinned.inc", 13});
        }
        v.read_eoln();
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_int(1, 3000000000LL, "e", eo::detail::site{"pinned.inc", 14});
            v.read_space();
            v.read_int(1, 1000000, "b", eo::detail::site{"pinned.inc", 15});
        }
        v.read_eoln();
        v.read_ints(0, 1, 999999, "a", eo::detail::site{"pinned.inc", 16});
        v.read_token(1, 5, eo::charset("a-z"), "b", eo::detail::site{"pinned.inc", 17});
        v.read_eoln();
    });
    EO_CHECK_EQ(kept.text, std::string(""));
    EO_CHECK_EQ(warnings_of_last_run(),
                std::string("pinned.inc:12: warning EO104: the bounds are the whole range of an int (3 times)\n"
                            "  say eo::any if any value is allowed\n"
                            "pinned.inc:14: warning EO105: the bounds 1..3000000000 do not fit an int (3 times)\n"
                            "  read a wider type\n"
                            "pinned.inc:17: warning EO107: \"b\" is read as a length here and as an integer elsewhere\n"
                            "  read it one way\n"
                            "pinned.inc:10: note EO106: the bounds 1..999999 are one away from a round number (3 "
                            "times)\n"
                            "  compare them with the statement\n"));
}

EO_TEST(pin_a_value_read_as_an_integer_and_then_as_a_token) {
    EO_CHECK_EQ(validate("5 ab\n", [](eo::validator& v) {
                    v.read_int(1, 10, "x", eo::detail::site{"pinned.inc", 60});
                    v.read_space();
                    v.read_token(1, 10, eo::charset("a-z"), "x", eo::detail::site{"pinned.inc", 61});
                    v.read_eoln();
                }).text,
                std::string(""));
    EO_CHECK_EQ(warnings_of_last_run(),
                std::string("pinned.inc:61: warning EO107: \"x\" is read as a length here and as an integer "
                            "elsewhere\n  read it one way\n"));
}

EO_TEST(pin_a_count_below_zero) {
    EO_CHECK_EQ(validate("1\n", [](eo::validator& v) { v.read_ints(-1, 1, 9, "a"); }).text,
                std::string("line 1, a: a count of -1 cannot be read"));
    EO_CHECK_EQ(check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
                    c.output.read_longs(-1, eo::any, "a");
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 1, a: a count of -1 cannot be read"));
}

EO_TEST(pin_the_note_on_a_token_of_a_mebibyte) {
    std::size_t const mebibyte = std::size_t{1} << 20;
    for (std::size_t const length : {mebibyte, mebibyte + 1}) {
        EO_CHECK_EQ(validate(std::string(length, 'z') + "\n", [](eo::validator& v) {
                        v.read_token(1, 2000000, eo::charset("a-z"), "s", eo::detail::site{"pinned.inc", 62});
                        v.read_eoln();
                    }).text,
                    std::string(""));
        EO_CHECK_EQ(warnings_of_last_run(),
                    length == mebibyte ? std::string("")
                                       : std::string("pinned.inc:62: note EO111: a token of 1048577 bytes was held "
                                                     "in memory\n  bound its length if the format allows\n"));
    }
}

EO_TEST(pin_the_near_round_note_when_only_the_low_bound_is_near) {
    EO_CHECK_EQ(validate("3000\n3000 3001\n", [](eo::validator& v) {
                    v.read_int(999, 5000, "i", eo::detail::site{"pinned.inc", 80});
                    v.read_eoln();
                    v.read_ints(2, 999, 5000, "x", eo::detail::site{"pinned.inc", 81});
                    v.read_eoln();
                }).text,
                std::string(""));
    EO_CHECK_EQ(warnings_of_last_run(),
                std::string("pinned.inc:80: note EO106: the bounds 999..5000 are one away from a round number\n"
                            "  compare them with the statement\n"
                            "pinned.inc:81: note EO106: the bounds 999..5000 are one away from a round number (2 "
                            "times)\n  compare them with the statement\n"));
}

EO_TEST(pin_the_near_round_note_on_bounds_computed_from_the_input) {
    EO_CHECK_EQ(validate("100001\n1 2\n5\n3 4\n", [](eo::validator& v) {
                    int const n = v.read_int(1, 200000, "n", eo::detail::site{"pinned.inc", 70});
                    v.read_eoln();
                    v.read_ints(2, 1, n, "a", eo::detail::site{"pinned.inc", 71});
                    v.read_eoln();
                    v.read_int(1, 200000, "n", eo::detail::site{"pinned.inc", 70});
                    v.read_eoln();
                    v.read_ints(2, 1, 100001, "b", eo::detail::site{"pinned.inc", 72});
                    v.read_eoln();
                }).text,
                std::string(""));
    EO_CHECK_EQ(warnings_of_last_run(),
                std::string("pinned.inc:72: note EO106: the bounds 1..100001 are one away from a round number (2 "
                            "times)\n  compare them with the statement\n"));
}

EO_TEST(pin_the_names_of_edges_grids_and_weights_in_messages) {
    EO_CHECK_EQ(validate("1 2 7\n2 3 0\n", [](eo::validator& v) { v.read_edges(2, 3, eo::weighted(1, 9), "e"); })
                    .text,
                std::string("line 2, e.w[2]: 0 is below 1"));
    EO_CHECK_EQ(validate("1 2\n3 4\n", [](eo::validator& v) { v.read_edges(2, 3, eo::element("e", 5)); }).text,
                std::string("line 2, e[5][2]: 4 is above 3"));
    EO_CHECK_EQ(validate("ab\nc\n", [](eo::validator& v) { v.read_grid(2, 2, eo::charset("a-z"), "g"); }).text,
                std::string("line 2, g[2]: the line is 1 characters long, not 2..2"));
    EO_CHECK_EQ(check_run("1\n", "1 2 3\n2 3 x\n", "1\n", [](eo::checker& c) {
                    c.output.read_tree(3, eo::weighted(1, 9), "t");
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 2, t.w[2]: expected an integer, found \"x\": it has a "
                            "character that cannot be part of the number"));
    EO_CHECK_EQ(check_run("1\n", "1 2\n1 2\n", "1\n", [](eo::checker& c) {
                    c.output.read_graph(3, 2, eo::simple, "g");
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 2: g: edges 1 and 2 are both (1, 2)"));
    EO_CHECK_EQ(check_run("1\n", "ab\ncde\n", "1\n", [](eo::checker& c) {
                    c.output.read_grid(2, 2, eo::charset("a-z"), "g");
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 2, g[2]: \"cde\" is longer than 2 characters"));
}

EO_TEST(pin_what_an_exact_comparison_of_lines_says) {
    auto compared = [](std::string const& out, std::string const& ans) {
        return check_run("1\n", out, ans, [](eo::checker& c) {
                   c.input.skip_rest("not needed");
                   c.lines(eo::exact);
               })
            .text;
    };
    std::string const huge_line = std::string(5000, 'w');
    EO_CHECK_EQ(compared("a\n\nb  \r\n", "a\n\nb\n"), std::string("ok 3 lines"));
    EO_CHECK_EQ(compared("a\nb\n", "a\n\nb\n"), std::string("wrong answer line 2 is \"b\", expected \"\""));
    EO_CHECK_EQ(compared(huge_line + "x\n", huge_line + "\n"),
                "wrong answer line 1 is \"" + std::string(40, 'w') + "...\", expected \"" + std::string(40, 'w') +
                    "...\"");
    EO_CHECK_EQ(compared(huge_line + "xy\n", huge_line + "\n"),
                "wrong answer line 1 is longer than the expected \"" + std::string(40, 'w') + "...\"; it starts \"" +
                    std::string(40, 'w') + "...\"");
    EO_CHECK_EQ(compared("a\n\n c\n", "a\n"), std::string("wrong answer the answer has 1 lines, the output has more"));
}

EO_TEST(pin_the_round_trips_an_interactor_counts_after_each_kind_of_read) {
    chat const number = talk("1\n", "5\n6\n", [](eo::interactor& it) {
        it.input.read_int(1, 1, "n");
        it.send(1);
        it.contestant.read_int(1, 9, "x");
        it.send(2);
        it.flush();
        it.contestant.read_int(1, 9, "y");
        eo::accept("{} round trips", it.round_trips());
    });
    EO_CHECK_EQ(number.text, std::string("ok 2 round trips"));
    chat const real = talk("1\n", "5\n6\n", [](eo::interactor& it) {
        it.input.read_int(1, 1, "n");
        it.send(1);
        it.contestant.read_real(0, 9, "x");
        it.send(2);
        it.flush();
        it.contestant.read_real(0, 9, "y");
        eo::accept("{} round trips", it.round_trips());
    });
    EO_CHECK_EQ(real.text, std::string("ok 2 round trips"));
    chat const token = talk("1\n", "5\n6\n", [](eo::interactor& it) {
        it.input.read_int(1, 1, "n");
        it.send(1);
        it.contestant.read_token(eo::any, "x");
        it.send(2);
        it.flush();
        it.contestant.read_token(eo::any, "y");
        eo::accept("{} round trips", it.round_trips());
    });
    EO_CHECK_EQ(token.text, std::string("ok 1 round trips"));
    chat const lenient = talk("1\n", "007\n6\n", [](eo::interactor& it) {
        it.input.read_int(1, 1, "n");
        it.contestant.numbers(eo::lenient);
        it.send(1);
        it.contestant.read_int(1, 9, "x");
        it.send(2);
        it.flush();
        it.contestant.read_int(1, 9, "y");
        eo::accept("{} round trips", it.round_trips());
    });
    EO_CHECK_EQ(lenient.text, std::string("ok 2 round trips"));
}

EO_TEST(pin_the_names_of_elements_in_messages) {
    EO_CHECK_EQ(validate("3\n1 0 3\n", [](eo::validator& v) {
                    int const n = v.read_int(1, 9, "n");
                    v.read_eoln();
                    v.read_ints(n, 1, 9, "a");
                }).text,
                std::string("line 2, a[2]: 0 is below 1"));
    EO_CHECK_EQ(validate("1 0\n", [](eo::validator& v) { v.read_ints(2, 1, 9, eo::element("m", 4)); }).text,
                std::string("line 1, m[4][2]: 0 is below 1"));
    EO_CHECK_EQ(validate("1 2\n", [](eo::validator& v) {
                    v.read_ints(1, 1, 9, "a", eo::detail::site{"pinned.inc", 20});
                    v.read_int(1, 9, "b", eo::detail::site{"pinned.inc", 21});
                }).text,
                std::string("pinned.inc:21: line 1, b: a space follows a[1]; read it with read_space()"));
    EO_CHECK_EQ(validate("1 2\n3\n", [](eo::validator& v) { v.read_ints(3, 1, 9, "a"); }).text,
                std::string("line 1: expected a space after a[2], found a line break"));
    EO_CHECK_EQ(validate("1 2\n3 3\n", [](eo::validator& v) { v.read_tree(3, "edge"); }).text,
                std::string("line 3: edge: edge 2 is a loop at vertex 3"));
    EO_CHECK_EQ(validate("1 2\n2 x\n", [](eo::validator& v) { v.read_tree(3, "edge"); }).text,
                std::string("line 2, edge[2]: expected an integer, found \"x\""));
    EO_CHECK_EQ(validate("1 2 3\n", [](eo::validator& v) {
                    v.read_ints(2, 1, 9, "a", eo::detail::site{"pinned.inc", 22});
                    v.read_space();
                    v.read_ints(1, 1, 8, "a", eo::detail::site{"pinned.inc", 23});
                    v.read_eoln();
                }).text,
                std::string(""));
    EO_CHECK_EQ(warnings_of_last_run(),
                std::string("pinned.inc:23: warning EO107: \"a\" is read as 1..8 here and as 1..9 elsewhere\n"
                            "  constrain it one way\n"));
    EO_CHECK_EQ(validate("ab cd\n", [](eo::validator& v) {
                    v.read_tokens(2, 1, 1, eo::charset("a-z"), "w");
                }).text,
                std::string("line 1, w[1]: \"ab\" is longer than 1 characters"));
    EO_CHECK_EQ(check_run("1\n", "1 2 x\n", "1 2 3\n", [](eo::checker& c) {
                    c.output.read_ints(3, 1, 9, "a");
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 1, a[3]: expected an integer, found \"x\": it has a "
                            "character that cannot be part of the number"));
    EO_CHECK_EQ(check_run("1\n", "1 2 x\n", "1 2 3\n", [](eo::checker& c) {
                    c.output.read_reals(3, 0, 9, eo::element("r", 7));
                    eo::accept();
                }).text,
                std::string("wrong answer: output.txt, line 1, r[7][3]: expected a number, found \"x\": it has no "
                            "digits where digits are required"));
}

EO_TEST(pin_what_the_stock_comparisons_say) {
    auto compared = [](std::string const& out, std::string const& ans, int how) {
        return check_run("1\n", out, ans, [&](eo::checker& c) {
                   c.input.skip_rest("not needed");
                   if (how == 0) c.tokens();
                   if (how == 1) c.reals(1e-6);
                   c.lines();
               })
            .text;
    };
    std::string const long_word = std::string(30, 'q') + "z";
    std::string const huge_word = std::string(5000, 'w');
    EO_CHECK_EQ(compared("a bb\nccc\n", "a  bb ccc", 0), std::string("ok 3 tokens"));
    EO_CHECK_EQ(compared("a bx ccc", "a bb ccc", 0), std::string("wrong answer token 2 is \"bx\", expected \"bb\""));
    EO_CHECK_EQ(compared("a bbb", "a bb", 0),
                std::string("wrong answer token 2 is longer than the expected \"bb\"; it starts \"bbb\""));
    EO_CHECK_EQ(compared("a", "a b", 0),
                std::string("wrong answer the output ended after 1 tokens, the answer has more"));
    EO_CHECK_EQ(compared("a b c", "a b", 0), std::string("wrong answer the answer has 2 tokens, the output has more"));
    EO_CHECK_EQ(compared(long_word + "x " + long_word, long_word + "y " + long_word, 0),
                "wrong answer token 1 is \"" + std::string(30, 'q') + "zx\", expected \"" + std::string(30, 'q') +
                    "zy\"");
    EO_CHECK_EQ(compared(huge_word + "v", huge_word + "u", 0),
                "wrong answer token 1 is \"" + std::string(40, 'w') + "...\", expected \"" + std::string(40, 'w') +
                    "...\"");
    EO_CHECK_EQ(compared(huge_word + " " + huge_word, huge_word + " " + huge_word, 0), std::string("ok 2 tokens"));
    EO_CHECK_EQ(compared(huge_word + "w", huge_word, 0),
                "wrong answer token 1 is longer than the expected \"" + std::string(40, 'w') + "...\"; it starts \"" +
                    std::string(40, 'w') + "...\"");
    EO_CHECK_EQ(compared("1.0000001 2 x", "1 2 x", 1), std::string("ok 3 values"));
    EO_CHECK_EQ(compared("1 2.5", "1 2", 1), std::string("wrong answer value 2 is 2.5, expected 2"));
    EO_CHECK_EQ(compared("1 y", "1 x", 1), std::string("wrong answer token 2 is \"y\", expected \"x\""));
    EO_CHECK_EQ(compared("1 " + huge_word, "1 2", 1),
                "wrong answer token 2 is longer than 4096 characters: \"" + std::string(40, 'w') + "...\"");
    EO_CHECK_EQ(compared("0." + std::string(4094, '1'), "0.1111111111111111", 1), std::string("ok 1 values"));
    EO_CHECK_EQ(compared("0." + std::string(4095, '1'), "0.1", 1),
                "wrong answer token 1 is longer than 4096 characters: \"0." + std::string(38, '1') + "...\"");
    EO_CHECK_EQ(compared("-0.0 1e-7", "0 0", 1), std::string("ok 2 values"));
    EO_CHECK_EQ(compared("a b  \r\nc\t\n", "a b\nc\n", 2), std::string("ok 2 lines"));
    EO_CHECK_EQ(compared("a b\nc d\n", "a b\nc\n", 2),
                std::string("wrong answer line 2 is longer than the expected \"c\"; it starts \"c \""));
    EO_CHECK_EQ(compared("a b\nd\n", "a b\nc\n", 2), std::string("wrong answer line 2 is \"d\", expected \"c\""));
    EO_CHECK_EQ(compared("a b\n", "a b\nc\n", 2),
                std::string("wrong answer the output ended after 1 lines, the answer has more"));
    EO_CHECK_EQ(compared("a\nb\nc\n", "a\nb\n", 2),
                std::string("wrong answer the answer has 2 lines, the output has more"));
    EO_CHECK_EQ(compared(huge_word + " \t \r\n", huge_word + "\n", 2), std::string("ok 1 lines"));
    EO_CHECK_EQ(compared(huge_word + " x\n", huge_word + "\n", 2),
                "wrong answer line 1 is longer than the expected \"" + std::string(40, 'w') + "...\"; it starts \"" +
                    std::string(40, 'w') + "...\"");
    EO_CHECK_EQ(compared("a\r\n\r\n", "a\n\n", 2), std::string("ok 1 lines"));
}

namespace {

std::uint64_t bits_of(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::string spelled_number(eo::rng& dice) {
    std::string text = dice.chance(0.4) ? "-" : "";
    long long const whole = dice.uniform(1, dice.chance(0.1) ? 90 : 12);
    for (long long at = 0; at < whole; at++)
        text.push_back(static_cast<char>('0' + dice.uniform(at == 0 && whole > 1 ? 1 : 0, 9)));
    if (dice.chance(0.8)) {
        text.push_back('.');
        long long const fraction = dice.uniform(1, dice.chance(0.1) ? 90 : 18);
        for (long long at = 0; at < fraction; at++) text.push_back(static_cast<char>('0' + dice.uniform(0, 9)));
    }
    if (dice.chance(0.3)) {
        text += dice.chance(0.5) ? "e" : "E-";
        text += std::to_string(dice.uniform(0, 330));
    }
    return text;
}

std::string printf_real(double value) {
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    if (std::strtod(buffer, nullptr) != value) std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

std::string printf_fixed(double value, int digits) {
    std::vector<char> buffer(static_cast<std::size_t>(std::snprintf(nullptr, 0, "%.*f", digits, value)) + 1);
    std::snprintf(buffer.data(), buffer.size(), "%.*f", digits, value);
    return buffer.data();
}

double drawn_real(eo::rng& dice) {
    switch (dice.uniform(0, 5)) {
        case 0: {
            std::uint64_t const bits = dice.next();
            double value = 0;
            std::memcpy(&value, &bits, sizeof(value));
            return std::isfinite(value) ? value : 1.5;
        }
        case 1: return static_cast<double>(dice.uniform(-1000000000, 1000000000)) / 1000.0;
        case 2: return dice.real(-1e9, 1e9);
        case 3: return static_cast<double>(dice.uniform(0, 1000)) / 8.0;
        case 4: {
            double const mantissa = static_cast<double>(dice.uniform(1, 1 << 20));
            return std::ldexp(mantissa, static_cast<int>(dice.uniform(-1074, 1000)));
        }
        default: {
            double const mantissa = static_cast<double>(dice.uniform(-99999, 99999));
            return mantissa * std::pow(10.0, static_cast<double>(dice.uniform(-20, 20)));
        }
    }
}

}  // namespace

EO_TEST(pin_a_real_as_strtod_reads_it_at_every_length) {
    eo::rng dice(20260929);
    for (int one = 0; one < 20000; one++) {
        std::string const text = spelled_number(dice);
        eo::detail::real_read const read = eo::detail::parse_real(text, true, true);
        double const expected = std::strtod(text.c_str(), nullptr);
        if (!std::isfinite(expected)) {
            EO_CHECK(read.problem == eo::detail::number_problem::out_of_range);
            continue;
        }
        EO_CHECK(read.problem == eo::detail::number_problem::none);
        EO_CHECK_EQ(bits_of(read.value), bits_of(expected == 0 ? 0.0 : expected));
    }
    std::string const long_fraction = "0." + std::string(70, '3');
    EO_CHECK_EQ(bits_of(eo::detail::parse_real(long_fraction, false).value),
                bits_of(std::strtod(long_fraction.c_str(), nullptr)));
    EO_CHECK_EQ(eo::detail::parse_real(long_fraction, false).decimals, 70);
    std::string const long_whole = "-" + std::string(80, '9') + ".5";
    EO_CHECK_EQ(bits_of(eo::detail::parse_real(long_whole, false).value),
                bits_of(std::strtod(long_whole.c_str(), nullptr)));
    std::string const too_big = "1" + std::string(400, '0');
    EO_CHECK(eo::detail::parse_real(too_big, false).problem == eo::detail::number_problem::out_of_range);
    std::string const tiny = "0." + std::string(400, '0') + "1";
    EO_CHECK(eo::detail::parse_real(tiny, false).value == 0);
    EO_CHECK_EQ(eo::detail::parse_real(tiny, false).decimals, 401);
    std::string const negative_tiny = "-" + tiny;
    EO_CHECK(eo::detail::parse_real(negative_tiny, false).problem == eo::detail::number_problem::redundant_minus);
    EO_CHECK(!std::signbit(eo::detail::parse_real(negative_tiny, false, true).value));
    std::string const exact_63 = "1." + std::string(61, '0');
    std::string const exact_64 = "1." + std::string(62, '0');
    EO_CHECK(eo::detail::parse_real(exact_63, false).value == 1.0);
    EO_CHECK(eo::detail::parse_real(exact_64, false).value == 1.0);
    for (char const* edge : {"2.2250738585072011e-308", "4.9406564584124654e-324", "2.4703282292062328e-324"})
        EO_CHECK_EQ(bits_of(eo::detail::parse_real(edge, true).value), bits_of(std::strtod(edge, nullptr)));
    EO_CHECK(eo::detail::parse_real("1.7976931348623159e308", true).problem ==
             eo::detail::number_problem::out_of_range);
    EO_CHECK_EQ(validate("0." + std::string(70, '5') + "\n", [](eo::validator& v) {
                    v.read_real(0, 1, 0, 80, "r");
                    v.read_eoln();
                }).text,
                std::string(""));
    EO_CHECK_EQ(validate("0." + std::string(70, '5') + "\n",
                         [](eo::validator& v) { v.read_real(0, 1, 0, 6, "r"); })
                    .text,
                "line 1, r: 0." + std::string(38, '5') + "... has 70 digits after the point, not 0..6");
}

EO_TEST(pin_a_real_written_as_printf_writes_it) {
    eo::rng dice(20260930);
    for (int one = 0; one < 20000; one++) {
        double const value = drawn_real(dice);
        EO_CHECK_EQ(eo::fmt("{}", value), printf_real(value));
        int const digits = static_cast<int>(dice.chance(0.9) ? dice.uniform(0, 45) : dice.uniform(46, 340));
        EO_CHECK_EQ(eo::fmt("{}", eo::fixed(value, digits)), printf_fixed(value, digits));
    }
    EO_CHECK_EQ(eo::fmt("{}", -0.0), std::string("-0"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(-0.0, 2)), std::string("-0.00"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(-0.001, 2)), std::string("-0.00"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(0.125, 2)), std::string("0.12"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(0.375, 2)), std::string("0.38"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(2.5, 0)), std::string("2"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(3.5, 0)), std::string("4"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(2.675, 2)), std::string("2.67"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(2.5, -1)), std::string("2.500000"));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(5e-324, 40)), "0." + std::string(40, '0'));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(1e22, 40)), "10000000000000000000000." + std::string(40, '0'));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(1e300, 40)), printf_fixed(1e300, 40));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(0.1, 41)), printf_fixed(0.1, 41));
    EO_CHECK_EQ(eo::fmt("{}", eo::fixed(123456789.123456789f, 3)), std::string("123456792.000"));
    EO_CHECK_EQ(eo::fmt("{} {}", 1e21, 123456789012345678.0), std::string("1e+21 1.2345678901234568e+17"));
    EO_CHECK_EQ(eo::fmt("{} {}", 0.1, 1.0 / 3), std::string("0.1 0.33333333333333331"));
}

EO_TEST(pin_the_complaint_about_a_graph_that_is_not_a_tree) {
    EO_CHECK_EQ(eo::is_tree(4, {{1, 2}, {2, 3}, {3, 1}}).message(),
                std::string("vertex 4 cannot be reached from vertex 1"));
    EO_CHECK_EQ(eo::is_tree(5, {{4, 5}, {1, 2}, {2, 3}, {3, 1}}).message(),
                std::string("vertex 4 cannot be reached from vertex 1"));
    EO_CHECK_EQ(eo::is_tree(4, {{1, 2}, {2, 1}, {3, 4}}).message(), std::string("edges 1 and 2 are both (1, 2)"));
    EO_CHECK_EQ(eo::is_tree(4, {{1, 2}, {3, 3}, {2, 4}}).message(), std::string("edge 2 is a loop at vertex 3"));
    EO_CHECK_EQ(eo::is_tree(4, {{1, 2}, {2, 5}, {3, 4}}).message(), std::string("edge 2 is (2, 5), outside 1..4"));
    EO_CHECK_EQ(eo::is_tree(4, {{1, 2}, {2, 3}}).message(), std::string("a tree on 4 vertices has 3 edges, not 2"));
    EO_CHECK_EQ(eo::is_tree(0, {}).message(), std::string("a tree on 0 vertices has -1 edges, not 0"));
    EO_CHECK(static_cast<bool>(eo::is_tree(1, {})));
    EO_CHECK(static_cast<bool>(eo::is_tree(4, {{4, 3}, {1, 3}, {2, 1}})));
    eo::rng dice(7);
    for (int one = 0; one < 20000; one++) {
        int const n = static_cast<int>(dice.uniform(1, dice.chance(0.1) ? 40 : 7));
        std::vector<eo::edge> edges;
        for (int at = 2; at <= n; at++) {
            int const other = static_cast<int>(dice.chance(0.85) ? dice.uniform(1, at - 1) : dice.uniform(0, n + 1));
            edges.push_back(dice.chance(0.5) ? eo::edge{at, other} : eo::edge{other, at});
        }
        dice.shuffle(edges);
        eo::check_result const simple = eo::is_simple_graph(n, edges);
        std::string const expected = !simple ? simple.message() : eo::is_connected(n, edges).message();
        EO_CHECK_EQ(eo::is_tree(n, edges).message(), expected);
    }
}

namespace {

std::string strict_lines(std::string const& text, std::size_t chunk) {
    std::string trace;
    eot::outcome const ended = eot::run_until_stop([&] {
        eo::detail::reader from(eo::detail::source::over_text(text, true, chunk), eo::detail::fault::invalid_test,
                                "", false, "EO102");
        eo::charset const letters("a-z ");
        from.rest_of_line(0, 0, nullptr, eo::detail::stated::deliberate, "pad", eo::detail::site{"pinned.inc", 29});
        for (int at = 0; from.peek() >= 0; at++) {
            if (at % 3 == 2) trace += "<" + from.rest_of_line(0, 0, nullptr, eo::detail::stated::deliberate, "t",
                                                            eo::detail::site{"pinned.inc", 30}) + ">";
            else trace += "<" + from.rest_of_line(0, 8, at % 3 == 0 ? &letters : nullptr, eo::detail::stated::yes, "t",
                                                  eo::detail::site{"pinned.inc", 31}) + ">";
        }
    });
    if (ended.stopped) trace += eo::fmt("| {} {}", ended.code, ended.text);
    return trace;
}

std::string lenient_lines(std::string const& text, std::size_t chunk) {
    std::string trace;
    eot::outcome const ended = eot::run_until_stop([&] {
        eo::detail::reader from(eo::detail::source::over_text(text, false, chunk), eo::detail::fault::wrong_answer,
                                "output.txt", true, "EO103");
        from.rest_of_line(0, 0, nullptr, eo::detail::stated::deliberate, "pad", eo::detail::site{"pinned.inc", 29});
        for (int at = 0; from.peek() >= 0; at++) {
            if (at % 2 == 0) {
                trace += "<" + from.rest_of_line(0, 0, nullptr, eo::detail::stated::deliberate, "t",
                                                 eo::detail::site{"pinned.inc", 32}) + ">";
            } else {
                bool longer = false;
                std::string const kept = from.line_up_to(4, longer, "t");
                trace += "<" + kept + (longer ? "+" : "") + ">";
            }
        }
    });
    if (ended.stopped) trace += eo::fmt("| {} {}", ended.code, ended.text);
    return trace;
}

}  // namespace

EO_TEST(pin_lines_read_wherever_the_buffer_ends) {
    pin_lines_everywhere("<abc><a\tb><long line of text>", "abc\na\tb\nlong line of text\n", strict_lines);
    pin_lines_everywhere("<ab>| 3 line 3, t: the line is longer than 8 characters", "ab\nabcdefghi\n", strict_lines);
    pin_lines_everywhere("<ab>| 3 line 3, t: the line is longer than 8 characters", "ab\nabcdefgh\r\r\n", strict_lines);
    pin_lines_everywhere("<ab><abcdefg\r>| 3 line 4, t: the line has no line break at its end", "ab\nabcdefg\r\r\nxyz",
                   strict_lines);
    pin_lines_everywhere("| 3 line 2, t: the line holds \"\\x0d\", which is not in \"a-z \"", "a\rb\n", strict_lines);
    pin_lines_everywhere("<a b><><>", "a b\r\n\r\n\n", strict_lines);
    pin_lines_everywhere("| 3 line 2, t: the line holds \"B\", which is not in \"a-z \"", "aB\n", strict_lines);
    pin_lines_everywhere("<abc><abcd><x y ><abcd+><>", "abc\r\nabcd \t\r\nx y \nabcdx  \n\n", lenient_lines);
    pin_lines_everywhere("<><ab c>", "\r\nab c", lenient_lines);
    pin_lines_everywhere("<" + std::string(3000, 'k') + ">", std::string(3000, 'k') + "\r\n", lenient_lines);
}

EO_TEST(pin_the_bytes_a_generator_writes) {
    made const result = generate({"-n=4"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 10);
        eo::rng& dice = g.rng();
        g.out.line(n);
        for (int at = 0; at < n; at++) {
            double const wide = dice.real(-1e9, 1e9);
            double const narrow = dice.real(-1, 1);
            double const whole = dice.real(0, 1e6);
            long long const count = dice.uniform(1, 1000000000);
            g.out.line(wide, eo::fixed(narrow, 6), eo::fixed(whole, 0), count);
        }
        g.out.line(eo::fixed(-0.0, 3), eo::fixed(0.5, 20), 1e-7, -2.5e300);
    });
    EO_CHECK_EQ(result.test, std::string("4\n"
                                         "-573328408.78539562 -0.010385 509083 339616102\n"
                                         "-293751400.63464117 0.922791 103021 475097425\n"
                                         "19180572.649808884 -0.995856 571445 971211752\n"
                                         "804202973.88878155 0.870536 58452 816561419\n"
                                         "-0.000 0.50000000000000000000 1e-07 -2.5e+300\n"));
}

EO_TEST(pin_the_bytes_of_named_streams_and_a_stress_seed_without_a_salt) {
    auto const body = [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 10);
        g.out.line(g.rng("values").ints(n, 1, 1000000000));
        g.out.line(g.rng("order").perm(n, 1));
        g.out.line(g.rng("big").uniform(1, 1000000000000LL));
    };
    made const stored = generate({"-n=4"}, body);
    EO_CHECK_EQ(stored.test, std::string("225460112 143423820 172945383 61074298\n"
                                         "1 3 2 4\n"
                                         "652551573150\n"));
    made const stressed = generate({"-n=4", "9f3a0c41d2e87b60"}, body);
    EO_CHECK_EQ(stressed.test, std::string("790706617 887542728 609220649 846671414\n"
                                           "3 4 1 2\n"
                                           "851663664071\n"));
}

namespace {

char const* comma_locale() {
    for (char const* one : {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "fr_FR.UTF-8", "ru_RU.UTF-8", "uk_UA.UTF-8"})
        if (std::setlocale(LC_NUMERIC, one) != nullptr) return one;
    return nullptr;
}

}  // namespace

EO_TEST(pin_reals_read_and_written_under_another_rounding_mode) {
    std::string const spelled[] = {"0.3", "0.1", "-2.675", "1e23", "123456789.123456789", "0." + std::string(70, '7')};
    double const printed[] = {0.125, 2.675, -0.125, 1e22, 0.3};
    for (int const mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
        int const before = std::fegetround();
        std::fesetround(mode);
        std::vector<std::uint64_t> read;
        std::vector<std::uint64_t> expected;
        for (std::string const& one : spelled) {
            read.push_back(bits_of(eo::detail::parse_real(one, true).value));
            expected.push_back(bits_of(std::strtod(one.c_str(), nullptr)));
        }
        std::vector<std::string> written;
        std::vector<std::string> by_printf;
        for (double const one : printed)
            for (int const digits : {0, 2, 6, 20}) {
                written.push_back(eo::fmt("{}", eo::fixed(one, digits)));
                by_printf.push_back(printf_fixed(one, digits));
            }
        std::fesetround(before);
        EO_CHECK(read == expected);
        EO_CHECK(written == by_printf);
    }
}

EO_TEST(reals_are_read_and_written_with_a_dot_in_a_locale_with_a_decimal_comma) {
    char const* const comma = comma_locale();
    if (comma == nullptr) {
        ::dprintf(eot::complaints_go_to(), "skipped: no de_DE, fr_FR, ru_RU or uk_UA locale, so reals are not read "
                                           "under a decimal comma\n");
        return;
    }
    double const read = eo::detail::parse_real("1.5", false).value;
    std::string const written = eo::fmt("{}", eo::fixed(1.5, 2));
    std::string const wide = eo::fmt("{}", eo::fixed(0.125, 45));
    std::string const points = eo::detail::format_points(20.5);
    int const before = std::fegetround();
    std::fesetround(FE_UPWARD);
    double const read_upward = eo::detail::parse_real("1.5", false).value;
    std::string const written_upward = eo::fmt("{}", eo::fixed(1.5, 2));
    std::fesetround(before);
    eot::outcome const nearly = check_run("1\n", "1\n", "1\n", [](eo::checker& c) {
        c.input.read_int(1, 1, "n");
        c.output.read_int(1, 1, "x");
        c.jury.read_int(1, 1, "x");
        eo::points(39.99999999);
    });
    std::setlocale(LC_NUMERIC, "C");
    EO_CHECK_EQ(read, 1.5);
    EO_CHECK_EQ(written, std::string("1.50"));
    EO_CHECK_EQ(wide, "0.125" + std::string(42, '0'));
    EO_CHECK_EQ(points, std::string("20.5"));
#if defined(_MSC_VER)
    EO_CHECK_EQ(read_upward, std::nextafter(1.5, 2.0));
#else
    EO_CHECK_EQ(read_upward, 1.5);
#endif
    EO_CHECK_EQ(written_upward, std::string("1.50"));
    EO_CHECK_EQ(nearly.text, std::string("points 39.99999999"));
    EO_CHECK_IN(checker_warnings(), "EO206");
}

namespace {

std::string without_header_lines(std::string text) {
    std::string const header = "eolymp.h:";
    for (std::size_t at = text.find(header); at != std::string::npos; at = text.find(header, at + 1)) {
        std::size_t start = at;
        while (start > 0 && text[start - 1] != ' ' && text[start - 1] != '\n') start--;
        std::size_t end = at + header.size();
        while (end < text.size() && text[end] >= '0' && text[end] <= '9') end++;
        text.replace(start, end - start, "eolymp.h:L");
        at = start;
    }
    return text;
}

}  // namespace

EO_TEST(pin_what_each_role_says_when_it_cannot_start) {
    eo::detail::site const here{"pinned.inc", 40};
    forget_the_environment();
    std::string program = "role.cpp";
    std::string first = "/nonexistent/input.txt";
    std::string second = "/nonexistent/output.txt";
    std::vector<char*> none{program.data()};
    std::vector<char*> one{program.data(), first.data()};
    std::vector<char*> two{program.data(), first.data(), second.data()};
    auto started = [](auto&& make) { return eot::run_until_stop([&] { make(); }).text; };
    EO_CHECK_EQ(started([&] { eo::checker c(1, none.data(), here); }),
                std::string("eolymp.h: pinned.inc:40: the checker was given no input file"));
    EO_CHECK_EQ(started([&] { eo::checker c(2, one.data(), here); }),
                std::string("eolymp.h: pinned.inc:40: the checker was given no output file"));
    EO_CHECK_EQ(started([&] { eo::checker c(3, two.data(), here); }),
                std::string("eolymp.h: pinned.inc:40: the checker was given no answer file"));
    EO_CHECK_EQ(started([&] { eo::interactor it(2, one.data(), here); }),
                std::string("eolymp.h: pinned.inc:40: an interactor needs the test and a file for its summary"));
    EO_CHECK_EQ(started([&] { eo::controller ctl(2, one.data(), here); }),
                std::string("eolymp.h: pinned.inc:40: a controller needs the test and a file for its summary"));
    ::setenv("OUTPUT_FILE", "/nonexistent/summary.txt", 1);
    EO_CHECK_EQ(started([&] { eo::interactor it(2, one.data(), here); }),
                std::string("eolymp.h: cannot open /nonexistent/input.txt: No such file or directory"));
    ::setenv("INPUT_FILE", "/nonexistent/from-the-environment.txt", 1);
    EO_CHECK_EQ(started([&] { eo::controller ctl(1, none.data(), here); }),
                std::string("eolymp.h: cannot open /nonexistent/from-the-environment.txt: No such file or directory"));
    forget_the_environment();
    EO_CHECK_EQ(check_run("1\n", "1\n", "1\n", [&](eo::checker&) {
                    char* also[] = {program.data(), first.data(), second.data(), second.data()};
                    eo::checker again(4, also, here);
                }).text,
                std::string("eolymp.h: pinned.inc:40: this program already has a checker"));
    EO_CHECK_EQ(talk("1\n", "", [&](eo::interactor&) { eo::interactor again(3, two.data(), here); }).text,
                std::string("eolymp.h: pinned.inc:40: this program already has an interactor"));
    scratch_file test("1\n");
    std::string const summary = temporary_path();
    std::vector<char*> both{program.data(), const_cast<char*>(test.path.c_str()), const_cast<char*>(summary.c_str())};
    EO_CHECK_EQ(talk("1\n", "", [&](eo::interactor&) { eo::controller beside(3, both.data(), here); }).text,
                std::string("jury error the controller ended without a verdict"));
    ::unlink(summary.c_str());
    EO_CHECK_EQ(run_control("1\n", {}, {}, 1, [&](eo::controller&) { eo::controller again(3, two.data(), here); })
                    .text,
                std::string("eolymp.h: pinned.inc:40: this program already has a controller"));
}

EO_TEST(pin_what_each_role_says_when_it_ends_without_a_verdict) {
    EO_CHECK_EQ(check_run("1\n", "1\n", "1\n", [](eo::checker&) {}).text,
                std::string("jury error the checker ended without a verdict"));
    chat const talked = talk("1\n", "5\n", [](eo::interactor& it) {
        it.send(1);
        it.contestant.read_int(1, 9, "x");
    });
    EO_CHECK_EQ(talked.text, std::string("jury error the interactor ended without a verdict"));
    EO_CHECK_EQ(talked.log, std::string("jury error the interactor ended without a verdict\n"));
    staged const controlled = run_control("1\n", {"ok"}, {"5\n"}, 1, [](eo::controller& ctl) {
        eo::channel& first = ctl.spawn();
        first.send(1);
        first.read_int(1, 9, "x");
    });
    EO_CHECK_EQ(controlled.text, std::string("jury error the controller ended without a verdict"));
    EO_CHECK_EQ(controlled.log, std::string("jury error the controller ended without a verdict\n"));
}

EO_TEST(pin_the_traffic_and_the_warnings_a_dialogue_ends_with) {
    chat const talked = talk("1\n", "5\n", [](eo::interactor& it) {
        eo::budget queries(it, 5, "queries");
        it.send(1);
        it.contestant.read_int(1, 9, "x");
        it.value("x", 5);
        eo::wrong("");
    });
    EO_CHECK_EQ(talked.text, std::string("wrong answer"));
    std::string const wrong_without_words = "eolymp.h:L: warning EO204: this wrong answer carries no "
                                            "message\n  say what the solution did\n";
    EO_CHECK_EQ(without_header_lines(talked.log), "wrong answer\n1 round trips, 2 bytes sent\n" + wrong_without_words);
    EO_CHECK_EQ(without_header_lines(checker_warnings()), wrong_without_words);
    chat const accepted = talk("1\n", "5\n", [](eo::interactor& it) {
        eo::budget queries(it, 5, "queries");
        it.send(1);
        it.contestant.read_token(eo::any, "x");
        eo::accept("{}", it.rng().uniform(1, 1000000000));
    });
    std::string const unspent = "eolymp.h:L: warning EO403: a budget was declared and never spent\n"
                                "  spend it before every reply, or drop it\n";
    EO_CHECK_EQ(accepted.text, std::string("ok 157745677"));
    EO_CHECK_EQ(accepted.summary, std::string("eolymp-summary 1\nfraction 1\nmessage 157745677\n"));
    EO_CHECK_EQ(without_header_lines(checker_warnings()), unspent);
    chat const empty = talk("1\n", "", [](eo::interactor&) { eo::jury_error(""); });
    EO_CHECK_EQ(empty.text, std::string("jury error"));
    EO_CHECK_EQ(checker_warnings(), std::string(""));
    staged const controlled = run_control("1\n", {"ok", "ok"}, {"5\n", ""}, 2, [](eo::controller& ctl) {
        eo::budget queries(ctl, 5, "queries");
        eo::channel& first = ctl.spawn();
        ctl.spawn();
        first.send(1, 2);
        first.read_int(1, 9, "x");
        eo::accept("{} {}", ctl.rng().uniform(1, 1000000000), ctl.has_jury());
    });
    std::string const idle = "eolymp.h:L: warning EO408: instance 2 was started and never talked to\n"
                             "  spawn it where it is needed, or drop it\n";
    EO_CHECK_EQ(controlled.text, std::string("ok 157745677 false"));
    EO_CHECK_EQ(without_header_lines(controlled.log),
                "ok 157745677 false\n2 instances, 1 round trips, 4 bytes sent\n" + idle + unspent);
    EO_CHECK_EQ(without_header_lines(checker_warnings()), idle + unspent);
    staged const silent = run_control("1\n", {}, {}, 1, [](eo::controller&) { eo::jury_error(""); });
    EO_CHECK_EQ(silent.text, std::string("jury error"));
    EO_CHECK_EQ(checker_warnings(), std::string(""));
}

EO_TEST(pin_a_choice_on_a_validator_and_on_a_stream) {
    EO_CHECK_EQ(validate("yes\n", [](eo::validator& v) { v.read_choice({"YES", "NO"}, "verdict"); }).text,
                std::string("line 1, verdict: \"yes\" is not one of YES, NO"));
    EO_CHECK_EQ(validate("YESS\n", [](eo::validator& v) { v.read_choice({"YES", "NO"}, "verdict"); }).text,
                std::string("line 1, verdict: \"YESS\" is not one of YES, NO"));
    EO_CHECK_EQ(check_run("1\n", "yes maybe\n", "1\n", [](eo::checker& c) {
                    std::string const said = c.output.read_choice({"YES", "NO"}, eo::any_case, "verdict");
                    if (said != "YES") eo::wrong("{}", said);
                    c.output.read_choice({"YES", "NO"}, "second");
                }).text,
                std::string("wrong answer: output.txt, line 1, second: \"maybe\" is not one of YES, NO"));
}

EO_TEST(pin_what_a_controller_reads_from_an_instance) {
    auto heard = [](std::string const& said, auto&& read) {
        return run_control("1\n", {"ok"}, {said}, 1, [&](eo::controller& ctl) {
                   eo::channel& first = ctl.spawn();
                   first.send(1);
                   read(first);
                   eo::accept();
               })
            .text;
    };
    EO_CHECK_EQ(heard("x\n", [](eo::channel& one) { one.read_int(1, 9, "x"); }),
                std::string("wrong answer: instance 1, line 1, x: expected an integer, found \"x\": it has a character "
                            "that cannot be part of the number"));
    EO_CHECK_EQ(heard("", [](eo::channel& one) { one.read_long(eo::any, "x"); }),
                std::string("wrong answer: instance 1, line 1, x: instance 1 ended the dialogue early"));
    EO_CHECK_EQ(heard("3 1 2 4\n", [](eo::channel& one) { one.read_longs(4, 1, 3, "a"); }),
                std::string("wrong answer: instance 1, line 1, a[4]: 4 is above 3"));
    EO_CHECK_EQ(heard("maybe\n", [](eo::channel& one) { one.read_choice({"yes", "no"}, "said"); }),
                std::string("wrong answer: instance 1, line 1, said: \"maybe\" is not one of yes, no"));
    EO_CHECK_EQ(heard("2.5 abc\n", [](eo::channel& one) {
                    one.read_real(0, 2, "r");
                }),
                std::string("wrong answer: instance 1, line 1, r: 2.5 is above 2"));
    EO_CHECK_EQ(heard("ab1\n", [](eo::channel& one) { one.read_token(1, 5, eo::charset("a-z"), "w"); }),
                std::string("wrong answer: instance 1, line 1, w: \"ab1\" holds \"1\", which is not in \"a-z\""));
    EO_CHECK_EQ(heard("7\n", [](eo::channel& one) {
                    if (!one.at_eof()) one.read_long(1, 9, "x");
                    if (!one.at_eof()) eo::wrong("more");
                }),
                std::string("ok"));
}

EO_TEST(a_warning_code_is_known_by_its_text_and_not_its_address) {
    eo::detail::diagnostics& shared = eo::detail::diagnostics::shared();
    shared.forget_everything();
    std::string const spelled = "EO101";
    eo::detail::site const here{"pinned.inc", 50};
    eo::detail::warn("EO101", "a value without a name", "name it", here);
    eo::detail::warn(spelled.c_str(), "a value without a name", "name it", here);
    EO_CHECK_EQ(shared.all().size(), static_cast<std::size_t>(1));
    EO_CHECK_EQ(shared.all().front().count, 2LL);
    std::string const again = "EO101";
    EO_CHECK(shared.again(again.c_str(), here));
    shared.forget_everything();
}

namespace {

std::string refused_by_a_validator(std::string const& text, void (*read)(eo::validator&)) {
    return validate(text, [&](eo::validator& v) { read(v); }).text;
}

}  // namespace

EO_TEST(a_number_that_a_token_character_follows_is_named_whole) {
    auto const integer_then_line = [](eo::validator& v) {
        v.read_int(1, 1000000, "n");
        v.read_eoln();
    };
    auto const integer_then_space = [](eo::validator& v) {
        v.read_int(1, 1000000, "n");
        v.read_space();
    };
    auto const real_then_line = [](eo::validator& v) {
        v.read_real(0, 10000, 1, 6, "p");
        v.read_eoln();
    };
    auto const integers_then_line = [](eo::validator& v) {
        v.read_ints(3, 1, 9, "a");
        v.read_eoln();
    };
    auto const integer_alone = [](eo::validator& v) { v.read_int(1, 1000000, "n"); };
    auto const unnamed_then_line = [](eo::validator& v) {
        v.read_int(1, 1000000, eo::unnamed);
        v.read_eoln();
    };
    auto const clock = [](eo::validator& v) {
        v.read_int(0, 23, "hour");
        v.read_char(':');
    };
    EO_CHECK_EQ(refused_by_a_validator("1e5\n", integer_then_line),
                std::string("line 1, n: expected an integer, found \"1e5\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("12abc 5\n", integer_then_space),
                std::string("line 1, n: expected an integer, found \"12abc\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("7.5\n", integer_then_line),
                std::string("line 1, n: expected an integer, found \"7.5\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("1e-3\n", real_then_line),
                std::string("line 1, p: expected a number, found \"1e-3\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("0,5\n", real_then_line),
                std::string("line 1, p: expected a number, found \"0,5\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("1.5e3\n", real_then_line),
                std::string("line 1, p: expected a number, found \"1.5e3\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("1.x\n", real_then_line),
                std::string("line 1, p: expected a number, found \"1.x\": it has no digits where digits are required"));
    EO_CHECK_EQ(refused_by_a_validator("1 2x 3\n", integers_then_line),
                std::string("line 1, a[2]: expected an integer, found \"2x\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("5x\n", integer_alone),
                std::string("line 1, n: expected an integer, found \"5x\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("5x\n", unnamed_then_line),
                std::string("line 1: expected an integer, found \"5x\": it has a character that cannot be part of the number"));
    EO_CHECK_EQ(refused_by_a_validator("0012\n", integer_then_line),
                std::string("line 1, n: expected an integer, found \"0012\": it has a leading zero"));
    EO_CHECK_EQ(refused_by_a_validator("012x\n", integer_then_line),
                std::string("line 1, n: expected an integer, found \"012x\": it has a character that cannot be part of the number"));
    auto const small_then_line = [](eo::validator& v) {
        v.read_int(1, 10, "n");
        v.read_eoln();
    };
    EO_CHECK_EQ(refused_by_a_validator("100e5\n", small_then_line), std::string("line 1, n: 100 is above 10"));
    EO_CHECK_EQ(refused_by_a_validator("12x30\n", clock), std::string("line 1: expected \":\" after hour, found \"x\""));
    EO_CHECK_EQ(refused_by_a_validator("5\r\n", integer_then_line), std::string(""));
    auto const on_a_stream = [](std::string const& output, void (*read)(eo::checker&)) {
        return check_run("1\n", output, "1\n", [&](eo::checker& c) {
                   c.input.skip_rest("not needed");
                   c.jury.skip_rest("not needed");
                   read(c);
                   eo::accept();
               })
            .text;
    };
    auto const stream_integer = [](eo::checker& c) { c.output.read_int(1, 1000000, "n"); };
    auto const stream_real = [](eo::checker& c) { c.output.read_real(0, 10, "p"); };
    EO_CHECK_EQ(on_a_stream("1e5\n", stream_integer),
                std::string("wrong answer: output.txt, line 1, n: expected an integer, found \"1e5\": it has a "
                            "character that cannot be part of the number"));
    EO_CHECK_EQ(on_a_stream("0,5\n", stream_real),
                std::string("wrong answer: output.txt, line 1, p: expected a number, found \"0,5\": it has a "
                            "character that cannot be part of the number"));
    EO_CHECK_EQ(on_a_stream("12abc\n", stream_integer),
                std::string("wrong answer: output.txt, line 1, n: expected an integer, found \"12abc\": it has a "
                            "character that cannot be part of the number"));
}

EO_TEST(a_warning_raised_on_one_line_of_two_files_is_counted_in_each) {
    eo::detail::diagnostics& shared = eo::detail::diagnostics::shared();
    shared.forget_everything();
    eo::detail::warn("EO101", "a value without a name", "name it", eo::detail::site{"validator.cpp", 12});
    eo::detail::warn("EO101", "a value without a name", "name it", eo::detail::site{"common.h", 12});
    std::string const other = "common.h";
    eo::detail::warn("EO101", "a value without a name", "name it", eo::detail::site{other.c_str(), 12});
    EO_CHECK_EQ(shared.all().size(), static_cast<std::size_t>(2));
    EO_CHECK_EQ(shared.all().front().count, 1LL);
    EO_CHECK_EQ(shared.all().back().count, 2LL);
    EO_CHECK_EQ(shared.local_block(), std::string("validator.cpp:12: warning EO101: a value without a name\n"
                                                  "  name it\n"
                                                  "common.h:12: warning EO101: a value without a name (2 times)\n"
                                                  "  name it\n"));
    shared.forget_everything();
}

namespace {

std::string refused_from_a_slow_pipe(char const* first, char const* second, void (*read)(eo::validator&)) {
    int ends[2];
    if (::pipe(ends) != 0) return "no pipe";
#if defined(_WIN32)
    std::thread writer([&] {
        long long const said = ::write(ends[1], first, static_cast<unsigned>(std::strlen(first)));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        long long const more = said < 0 ? said : ::write(ends[1], second, static_cast<unsigned>(std::strlen(second)));
        (void)more;
        ::close(ends[1]);
    });
#else
    pid_t const writer = ::fork();
    if (writer == 0) {
        ::close(ends[0]);
        if (::write(ends[1], first, std::strlen(first)) < 0) ::_exit(1);
        ::usleep(300000);
        if (::write(ends[1], second, std::strlen(second)) < 0) ::_exit(1);
        ::_exit(0);
    }
    ::close(ends[1]);
#endif
    int const kept = ::dup(0);
    ::dup2(ends[0], 0);
    ::close(ends[0]);
    eo::detail::diagnostics::shared().forget_everything();
    std::string program = "validator.cpp";
    std::vector<char*> arguments{program.data()};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::validator v(static_cast<int>(arguments.size()), arguments.data());
        read(v);
    });
    ::dup2(kept, 0);
    ::close(kept);
#if defined(_WIN32)
    writer.join();
#else
    int status = 0;
    ::waitpid(writer, &status, 0);
#endif
    return result.text;
}

}  // namespace

EO_TEST(a_validator_on_a_pipe_that_pauses_looks_ahead_as_far_as_on_a_file) {
    auto const integer_then_line = [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
    };
    EO_CHECK_EQ(refused_from_a_slow_pipe("5x", "yz\n", integer_then_line),
                std::string("line 1, n: expected an integer, found \"5xyz\": it has a character that cannot be part "
                            "of the number"));
    EO_CHECK_EQ(refused_from_a_slow_pipe("5\nx", "yz\n", integer_then_line),
                std::string("line 2: expected the end of the input, found \"xyz\""));
}

EO_TEST(eo_case_refuses_a_count_that_another_integer_before_the_cases_equals) {
    auto const counted_then_k = [](eo::validator& v) {
        int const t = v.read_int(1, 10, "t");
        v.read_space();
        v.read_int(1, 10, "k");
        v.read_eoln();
        v.cases(t, [&] {
            v.read_int(1, 9, "x");
            v.read_eoln();
        });
    };
    eot::outcome const first = validate("2 2\n7\n8\n", counted_then_k, {"--eo-case=1"});
    EO_CHECK_EQ(first.code, 3);
    EO_CHECK_EQ(first.text, std::string("eolymp.h: --eo-case needs the count given to v.cases, 2, to be the one "
                                        "integer of that value before the first case, so that it can be written as "
                                        "1; there are 2, and it cannot tell which"));
    eot::outcome const apart = validate("2 3\n7\n8\n", counted_then_k, {"--eo-case=2"});
    EO_CHECK_EQ(apart.printed, std::string("1 3\n8\n"));
}

EO_TEST(a_real_one_rounding_above_its_tolerance_is_within_it) {
    EO_CHECK(eo::close_enough(0.5, 0.500001, 1e-6));
    EO_CHECK(!eo::close_enough(0.5, 0.5000011, 1e-6));
    EO_CHECK(eo::close_enough(1e9, 1e9 + 1000, 1e-6));
    EO_CHECK(!eo::close_enough(1e9, 1e9 + 1001, 1e-6));
    EO_CHECK(eo::close_enough(0.5, 0.499999, 1e-6));
    EO_CHECK(eo::close_enough(913908.32525340363, 914822.23357865703, 1e-3));
    EO_CHECK(eo::close_enough(1000, 1000.1, 1e-4));
    EO_CHECK(eo::close_enough(1000, 999.9, 1e-4));
    EO_CHECK(eo::close_enough(1000000, 1000000.001, 1e-9));
    EO_CHECK(!eo::close_enough(1000, 1000.11, 1e-4));
    for (double const expected : {0.75, 3.0, 913908.32525340363, 1e45, -7.5e12})
        for (double const epsilon : {1e-3, 1e-5, 1e-9})
            for (int const steps : {-3, -1, 0, 1, 3}) {
                double const found = expected * (1 + epsilon) + steps * std::fabs(expected) * 1e-16;
                bool const by_the_quotient = std::fabs(expected - found) / std::fabs(expected) <= epsilon;
                if (by_the_quotient) EO_CHECK(eo::close_enough(expected, found, epsilon));
            }
    EO_CHECK(eo::compare(0.500001, 0.5, eo::minimize, eo::within(1e-6)) == eo::standing::equal);
    auto const micro = [](eo::checker& c) { c.reals(1e-6); };
    EO_CHECK_EQ(compared_by("0.500001", "0.5", micro), std::string("ok 1 values"));
    EO_CHECK_EQ(compared_by("0.499999", "0.5", micro), std::string("ok 1 values"));
    auto const milli = [](eo::checker& c) { c.reals(1e-3); };
    EO_CHECK_EQ(compared_by("914822.23357865703", "913908.32525340363", milli), std::string("ok 1 values"));
    auto const ten_thousandth = [](eo::checker& c) { c.reals(1e-4); };
    EO_CHECK_EQ(compared_by("1000.1 999.9", "1000 1000", ten_thousandth), std::string("ok 2 values"));
    eot::outcome const optimum = check_run("1\n", "0.500001\n", "0.5\n", [](eo::checker& c) {
        c.input.skip_rest("not needed");
        c.optimum(c.jury.read_real(eo::any, "best"), c.output.read_real(eo::any, "best"), eo::minimize,
                  eo::within(1e-6));
    });
    EO_CHECK_EQ(optimum.text, std::string("ok 0.500001"));
}

EO_TEST(a_hyphen_that_reads_as_a_backwards_range_is_told_where_to_go) {
    eot::outcome const set = eot::run_until_stop([] { eo::charset const made("A-Za-z0-9_.,:;()- "); });
    EO_CHECK_EQ(set.code, 3);
    EO_CHECK_EQ(set.text, std::string("eolymp.h: the character range \")- \" in charset(\"A-Za-z0-9_.,:;()- \") runs "
                                      "backwards; write its low end first, or put a - that stands for itself first "
                                      "or last"));
    eot::outcome const pattern = eot::run_until_stop([] { eo::pattern const made("[A-Za-z0-9_.,:;()- ]+"); });
    EO_CHECK_EQ(pattern.code, 3);
    EO_CHECK_IN(pattern.text, "eo::pattern(\"[A-Za-z0-9_.,:;()- ]+\") does not parse at column 17: this range runs "
                              "backwards; write its low end first, or put a - that stands for itself first or "
                              "last");
}

EO_TEST(pin_the_report_line_of_a_warning_raised_in_a_windows_path) {
    clean_slate fresh;
    raise_warning(eo::detail::site{"C:\\Users\\author\\my \"best\" checker.cpp", 7});
    EO_CHECK_EQ(diagnostics().report_line(),
                std::string("eo-report {\"version\":1,\"warnings\":[{\"code\":\"EO101\",\"at\":\"C:\\\\Users\\\\author"
                            "\\\\my \\\"best\\\" checker.cpp:7\",\"count\":1}]}\n"));
}
