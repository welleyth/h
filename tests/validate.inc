namespace {

struct scratch_file {
    std::string path;

    explicit scratch_file(std::string const& text) : path(temporary_path()) {
        std::FILE* const writing = std::fopen(path.c_str(), "wb");
        std::fwrite(text.data(), 1, text.size(), writing);
        std::fclose(writing);
    }

    scratch_file(scratch_file const&) = delete;
    ~scratch_file() { ::unlink(path.c_str()); }
};

template <class Body>
eot::outcome validate(std::string const& text, Body&& body, std::vector<char const*> extra = {}) {
    eo::detail::diagnostics::shared().forget_everything();
    eo::detail::sums_ever_made() = 0;
    scratch_file input(text);
    std::vector<char*> arguments;
    std::string program = "validator.cpp";
    arguments.push_back(program.data());
    arguments.push_back(const_cast<char*>(input.path.c_str()));
    for (char const* one : extra) arguments.push_back(const_cast<char*>(one));
    eot::outcome result = eot::run_until_stop([&] {
        eo::validator v(static_cast<int>(arguments.size()), arguments.data());
        body(v);
    });
    if (std::getenv("EOT_TRACE") != nullptr && result.stopped)
        std::fprintf(stderr, "   stopped: %s\n", result.text.c_str());
    return result;
}

std::string warnings_of_last_run() { return eo::detail::diagnostics::shared().local_block(); }

}  // namespace

EO_TEST(a_valid_test_ends_without_a_word) {
    eot::outcome const result = validate("3\n1 2 3\n", [](eo::validator& v) {
        int const n = v.read_int(1, 10, "n");
        v.read_eoln();
        std::vector<int> const a = v.read_ints(n, 1, 1000, "a");
        v.read_eoln();
        EO_CHECK_EQ(a.size(), static_cast<std::size_t>(3));
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.printed, std::string(""));
}

EO_TEST(a_value_outside_its_bounds_names_the_line_and_the_value) {
    eot::outcome const low = validate("0\n", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_EQ(low.code, 3);
    EO_CHECK_EQ(low.text, std::string("line 1, n: 0 is below 1"));
    eot::outcome const high = validate("11\n", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_EQ(high.text, std::string("line 1, n: 11 is above 10"));
}

EO_TEST(a_value_that_is_not_a_number_says_what_is_wrong_with_it) {
    eot::outcome const text = validate("abc\n", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_EQ(text.text, std::string("line 1, n: expected an integer, found \"abc\""));
    eot::outcome const suffix = validate("9a\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_int(1, 10, "n"), 9);
        v.read_eoln();
    });
    EO_CHECK_EQ(suffix.text, std::string("line 1: expected a line break after n, found \"a\""));
    eot::outcome const plus = validate("+5\n", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_EQ(plus.text, std::string("line 1, n: expected an integer, found \"+5\""));
    eot::outcome const zeroes = validate("007\n", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_IN(zeroes.text, "it has a leading zero");
    eot::outcome const empty = validate("", [](eo::validator& v) { v.read_int(1, 10, "n"); });
    EO_CHECK_EQ(empty.text, std::string("line 1, n: expected an integer, found the end of the input"));
    eot::outcome const wide = validate("3000000000\n", [](eo::validator& v) {
        v.read_long(1, 4000000000LL, "n");
        v.read_eoln();
    });
    EO_CHECK(!wide.stopped);
    eot::outcome const narrow =
        validate("3000000000\n", [](eo::validator& v) { v.read_int(eo::any, "n"); });
    EO_CHECK_EQ(narrow.text, std::string("line 1, n: 3000000000 does not fit an int"));
}

EO_TEST(every_separator_is_read_by_the_validator) {
    eot::outcome const good = validate("1 2\n12:30\n", [](eo::validator& v) {
        v.read_int(1, 9, "a");
        v.read_space();
        v.read_int(1, 9, "b");
        v.read_eoln();
        v.read_int(1, 99, "hour");
        v.read_char(':');
        v.read_int(0, 59, "minute");
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const wrong = validate("1\n", [](eo::validator& v) {
        v.read_int(1, 9, "a");
        v.read_space();
    });
    EO_CHECK_EQ(wrong.text, std::string("line 1: expected a space after a, found a line break"));
    eot::outcome const gone = validate("1", [](eo::validator& v) {
        v.read_int(1, 9, "a");
        v.read_eoln();
    });
    EO_CHECK_EQ(gone.text, std::string("line 1: expected a line break after a, found the end of the input"));
    eot::outcome const other = validate("12x30\n", [](eo::validator& v) {
        v.read_int(1, 99, "hour");
        v.read_char(':');
    });
    EO_CHECK_EQ(other.text, std::string("line 1: expected \":\" after hour, found \"x\""));
    eot::outcome const nul = validate(std::string("12\0" "30\n", 6), [](eo::validator& v) {
        v.read_int(1, 99, "hour");
        v.read_char(':');
    });
    EO_CHECK_EQ(nul.text, std::string("line 1: expected \":\" after hour, found \"\\x00\""));
}

EO_TEST(a_warning_repeated_at_one_line_is_counted_once) {
    eot::outcome const counted = validate("1 2 3\n", [](eo::validator& v) {
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_int(1, 200001, "x");
        }
        v.read_eoln();
    });
    EO_CHECK(!counted.stopped);
    EO_CHECK_IN(warnings_of_last_run(),
                "note EO106: the bounds 1..200001 are one away from a round number (3 times)");
    eot::outcome const silenced = validate("1 2 3\n", [](eo::validator& v) {
        eo::allow quiet("EO106", "the statement says 200001");
        for (int at = 0; at < 3; at++) {
            if (at > 0) v.read_space();
            v.read_int(1, 200001, "x");
        }
        v.read_eoln();
    });
    EO_CHECK(!silenced.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "silenced EO106 (3 times): the statement says 200001");
}

EO_TEST(an_element_of_an_element_is_named_in_full) {
    eot::outcome const nested = validate("1 2 30\n", [](eo::validator& v) {
        v.read_ints(3, 1, 9, eo::element("m", 4));
    });
    EO_CHECK_EQ(nested.text, std::string("line 1, m[4][3]: 30 is above 9"));
    eot::outcome const glued = validate("1 2\n", [](eo::validator& v) {
        v.read_int(1, 9, eo::element("a", 7));
        v.read_int(1, 9, "b");
    });
    EO_CHECK_IN(glued.text, "line 1, b: a space follows a[7]; read it with read_space()");
    eot::outcome const empty = validate("1 2\n", [](eo::validator& v) {
        v.read_int(1, 9, "");
        v.read_int(1, 9, "b");
    });
    EO_CHECK_EQ(empty.text, std::string("line 1, b: expected an integer, found a space"));
}

EO_TEST(a_separator_left_out_of_the_validator_blames_the_validator) {
    eot::outcome const space = validate("5 7\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_int(1, 9, "k");
    });
    EO_CHECK_IN(space.text, "line 1, k: a space follows n; read it with read_space()");
    EO_CHECK_IN(space.text, "validate.inc:");
    eot::outcome const line = validate("5\n7\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_int(1, 9, "k");
    });
    EO_CHECK_IN(line.text, "line 1, k: a line break follows n; read it with read_eoln()");
}

EO_TEST(the_end_of_the_input_is_checked_with_or_without_read_eof) {
    eot::outcome const extra = validate("1\n7\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
    });
    EO_CHECK_EQ(extra.text, std::string("line 2: expected the end of the input, found \"7\""));
    eot::outcome const asked = validate("1\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
        v.read_eof();
    });
    EO_CHECK(!asked.stopped);
    eot::outcome const blank = validate("1\n\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
    });
    EO_CHECK_IN(blank.text, "expected the end of the input");
}

EO_TEST(looking_ahead_consumes_nothing) {
    eot::outcome const result = validate("1 2\n", [](eo::validator& v) {
        std::vector<int> values;
        values.push_back(v.read_int(1, 9, "x"));
        while (!v.at_eoln()) {
            v.read_space();
            values.push_back(v.read_int(1, 9, "x"));
        }
        v.read_eoln();
        EO_CHECK_EQ(values.size(), static_cast<std::size_t>(2));
        EO_CHECK(v.at_eof());
    });
    EO_CHECK(!result.stopped);
}

EO_TEST(numbers_are_read_with_their_digits_declared) {
    eot::outcome const good = validate("0.50 1.25\n", [](eo::validator& v) {
        std::vector<double> const prices = v.read_reals(2, 0.0, 10.0, 2, 2, "price");
        EO_CHECK(prices[0] == 0.5);
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const digits = validate("0.5\n", [](eo::validator& v) { v.read_real(0.0, 1.0, 2, 2, "p"); });
    EO_CHECK_EQ(digits.text, std::string("line 1, p: 0.5 has 1 digits after the point, not 2..2"));
    eot::outcome const negative_zero =
        validate("-0.0\n", [](eo::validator& v) { v.read_real(-1.0, 1.0, 1, 1, "p"); });
    EO_CHECK_EQ(negative_zero.text,
                std::string("line 1, p: expected a number, found \"-0.0\": it is zero written with a minus"));
    eot::outcome const many = validate("0." + std::string(100, '5') + "\n", [](eo::validator& v) {
        v.read_real(0.0, 1.0, 2, 2, "p");
    });
    EO_CHECK_EQ(many.text, "line 1, p: 0." + std::string(38, '5') + "... has 100 digits after the point, not 2..2");
    eot::outcome const below = validate("2.5\n", [](eo::validator& v) { v.read_real(0.0, 1.0, 1, 1, "p"); });
    EO_CHECK_EQ(below.text, std::string("line 1, p: 2.5 is above 1"));
    eot::outcome const under = validate("-2.5\n", [](eo::validator& v) { v.read_real(0.0, 1.0, 1, 1, "p"); });
    EO_CHECK_EQ(under.text, std::string("line 1, p: -2.5 is below 0"));
    eot::outcome const broken = validate("1e5\n", [](eo::validator& v) {
        v.read_real(0.0, 1e9, 0, 6, "p");
        v.read_eoln();
    });
    EO_CHECK_EQ(broken.text, std::string("line 1: expected a line break after p, found \"e\""));
    eot::outcome const nothing = validate("x\n", [](eo::validator& v) { v.read_real(0.0, 1.0, 1, 1, "p"); });
    EO_CHECK_EQ(nothing.text, std::string("line 1, p: expected a number, found \"x\""));
}

EO_TEST(tokens_are_read_with_a_length_and_a_charset) {
    eot::outcome const good = validate("abc\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_token(1, 5, eo::charset("a-z"), "s"), std::string("abc"));
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const long_one =
        validate("abcdef\n", [](eo::validator& v) { v.read_token(1, 3, eo::charset("a-z"), "s"); });
    EO_CHECK_EQ(long_one.text, std::string("line 1, s: \"abcd\" is longer than 3 characters"));
    eot::outcome const stranger =
        validate("abQ\n", [](eo::validator& v) { v.read_token(1, 5, eo::charset("a-z"), "s"); });
    EO_CHECK_EQ(stranger.text, std::string("line 1, s: \"abQ\" holds \"Q\", which is not in \"a-z\""));
    eot::outcome const control =
        validate("ab\x7f\n", [](eo::validator& v) { v.read_token(1, 5, eo::charset("a-z"), "s"); });
    EO_CHECK_EQ(control.text, std::string("line 1, s: \"ab\\x7f\" holds \"\\x7f\", which is not in \"a-z\""));
    eot::outcome const loose = validate("abc\n", [](eo::validator& v) {
        v.read_token(eo::any, "s");
        v.read_eoln();
    });
    EO_CHECK(!loose.stopped);
}

EO_TEST(a_line_is_read_with_its_line_break) {
    eot::outcome const good = validate("hello world\n7\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_line(1, 20, eo::charset("a-z "), "title"), std::string("hello world"));
        v.read_int(1, 9, "n");
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const unterminated =
        validate("hello", [](eo::validator& v) { v.read_line(1, 20, eo::charset("a-z "), "title"); });
    EO_CHECK_EQ(unterminated.text, std::string("line 1, title: the line has no line break at its end"));
    eot::outcome const too_long =
        validate("hello\n", [](eo::validator& v) { v.read_line(1, 3, eo::charset("a-z"), "title"); });
    EO_CHECK_EQ(too_long.text, std::string("line 1, title: the line is longer than 3 characters"));
    eot::outcome const stranger =
        validate("helLo\n", [](eo::validator& v) { v.read_line(1, 9, eo::charset("a-z"), "title"); });
    EO_CHECK_IN(stranger.text, "the line holds \"L\"");
    eot::outcome const bell =
        validate("he\x07o\n", [](eo::validator& v) { v.read_line(1, 9, eo::charset("a-z"), "title"); });
    EO_CHECK_IN(bell.text, "the line holds \"\\x07\"");
}

EO_TEST(a_choice_is_one_of_the_words_given) {
    eot::outcome const good = validate("erase\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_choice({"insert", "erase"}, "command"), std::string("erase"));
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const other =
        validate("delete\n", [](eo::validator& v) { v.read_choice({"insert", "erase"}, "command"); });
    EO_CHECK_EQ(other.text, std::string("line 1, command: \"delete\" is not one of insert, erase"));
}

EO_TEST(bulk_reads_put_one_space_between_their_values) {
    eot::outcome const good = validate("1 2\n10 20\nab cd\n", [](eo::validator& v) {
        v.read_ints(2, 1, 9, "a");
        v.read_eoln();
        v.read_longs(2, 1, 99, "b");
        v.read_eoln();
        v.read_tokens(2, 1, 4, eo::charset("a-z"), "w");
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    eot::outcome const doubled = validate("1  2\n", [](eo::validator& v) { v.read_ints(2, 1, 9, "a"); });
    EO_CHECK_IN(doubled.text, "expected an integer, found a space");
    eot::outcome const named = validate("1 22\n", [](eo::validator& v) { v.read_ints(2, 1, 9, "a"); });
    EO_CHECK_EQ(named.text, std::string("line 1, a[2]: 22 is above 9"));
    eot::outcome const loose = validate("1 2\n3 4\n", [](eo::validator& v) {
        v.read_ints(2, eo::any, "a");
        v.read_eoln();
        v.read_longs(2, eo::any, "b");
        v.read_eoln();
    });
    EO_CHECK(!loose.stopped);
}

EO_TEST(structural_readers_read_and_check_in_one_call) {
    eot::outcome const tree = validate("1 2\n2 3\n", [](eo::validator& v) {
        std::vector<eo::edge> const edges = v.read_tree(3, "edge");
        EO_CHECK_EQ(edges.size(), static_cast<std::size_t>(2));
    });
    EO_CHECK(!tree.stopped);
    eot::outcome const cycle =
        validate("1 2\n2 1\n", [](eo::validator& v) { v.read_tree(3, "edge"); });
    EO_CHECK_IN(cycle.text, "edge: edges 1 and 2 are both (1, 2)");
    eot::outcome const graph = validate("1 2\n2 3\n1 3\n", [](eo::validator& v) {
        v.read_graph(3, 3, eo::simple | eo::connected, "edge");
    });
    EO_CHECK(!graph.stopped);
    eot::outcome const loose = validate("1 2\n", [](eo::validator& v) { v.read_graph(3, 1, eo::any_graph, "edge"); });
    EO_CHECK(!loose.stopped);
    eot::outcome const split =
        validate("1 2\n", [](eo::validator& v) { v.read_graph(3, 1, eo::connected, "edge"); });
    EO_CHECK_IN(split.text, "vertex 3 cannot be reached from vertex 1");
    eot::outcome const outside =
        validate("1 9\n", [](eo::validator& v) { v.read_graph(3, 1, eo::simple, "edge"); });
    EO_CHECK_IN(outside.text, "edge 1 is (1, 9), outside 1..3");
    eot::outcome const perm = validate("2 3 1\n", [](eo::validator& v) {
        v.read_permutation(3, "p");
        v.read_eoln();
    });
    EO_CHECK(!perm.stopped);
    eot::outcome const repeated = validate("2 2 1\n", [](eo::validator& v) { v.read_permutation(3, "p"); });
    EO_CHECK_IN(repeated.text, "p: elements 1 and 2 are both 2");
}

EO_TEST(tree_and_graph_readers_refuse_in_a_fixed_order_with_fixed_words) {
    auto const graph = [](char const* text, int n, int m, eo::graph_shape shape) {
        return validate(text, [n, m, shape](eo::validator& v) { v.read_graph(n, m, shape, "edge"); }).text;
    };
    EO_CHECK_EQ(graph("1 9\n1 1\n", 3, 2, eo::simple | eo::connected),
                std::string("line 3: edge: edge 1 is (1, 9), outside 1..3"));
    EO_CHECK_EQ(graph("1 2\n1 1\n", 3, 2, eo::simple), std::string("line 3: edge: edge 2 is a loop at vertex 1"));
    EO_CHECK_EQ(graph("1 2\n2 1\n", 3, 2, eo::simple | eo::connected),
                std::string("line 3: edge: edges 1 and 2 are both (1, 2)"));
    EO_CHECK_EQ(graph("1 2\n2 1\n", 3, 2, eo::connected),
                std::string("line 3: edge: vertex 3 cannot be reached from vertex 1"));
    EO_CHECK_EQ(graph("1 1\n", 1, 1, eo::any_graph), std::string(""));
    EO_CHECK_EQ(graph("", 3, 0, eo::connected), std::string("line 1: edge: vertex 2 cannot be reached from vertex 1"));
    EO_CHECK_EQ(graph("", 3, -1, eo::any_graph), std::string(""));
    EO_CHECK_EQ(graph("1 2 \n", 3, 1, eo::any_graph),
                std::string("line 1: expected a line break after edge[1], found a space"));
    auto const tree = [](char const* text, int n) {
        return validate(text, [n](eo::validator& v) { v.read_tree(n, eo::unnamed); }).text;
    };
    EO_CHECK_EQ(tree("", 0), std::string("line 1: a tree on 0 vertices has -1 edges, not 0"));
    EO_CHECK_EQ(tree("", 1), std::string(""));
    EO_CHECK_EQ(tree("1 2\n1 2\n", 3), std::string("line 3: edges 1 and 2 are both (1, 2)"));
    EO_CHECK_EQ(tree("1 x\n", 2), std::string("line 1: expected an integer, found \"x\""));
}

EO_TEST(an_edge_list_is_read_line_by_line_with_its_vertices_bounded) {
    eot::outcome const plain = validate("1 2\n3 2\n", [](eo::validator& v) {
        std::vector<eo::edge> const edges = v.read_edges(2, 3, "edge");
        EO_CHECK_EQ(edges.size(), static_cast<std::size_t>(2));
        EO_CHECK_EQ(edges[1].u, 3);
        EO_CHECK_EQ(edges[1].v, 2);
    }, {"--eo-describe"});
    EO_CHECK(!plain.stopped);
    EO_CHECK_IN(plain.printed, "eo-describe value edge int 1 3 low=yes high=yes");
    eot::outcome const weighted = validate("1 2 7\n2 3 100\n", [](eo::validator& v) {
        std::vector<eo::weighted_edge> const edges = v.read_edges(2, 3, eo::weighted(1, 100), "edge");
        EO_CHECK_EQ(edges[0].w, 7);
        EO_CHECK_EQ(edges[1].w, 100);
    }, {"--eo-describe"});
    EO_CHECK(!weighted.stopped);
    EO_CHECK_IN(weighted.printed, "eo-describe value edge.w int 1 100 low=no high=yes");
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO107"));
    eot::outcome const outside = validate("1 2\n2 9\n", [](eo::validator& v) { v.read_edges(2, 3, "edge"); });
    EO_CHECK_EQ(outside.code, 3);
    EO_CHECK_IN(outside.text, "line 2, edge[2]: 9 is above 3");
    eot::outcome const light =
        validate("1 2 0\n", [](eo::validator& v) { v.read_edges(1, 3, eo::weighted(1, 100), "edge"); });
    EO_CHECK_IN(light.text, "line 1, edge.w[1]: 0 is below 1");
    eot::outcome const unweighted =
        validate("1 2 5\n", [](eo::validator& v) { v.read_edges(1, 3, "edge"); });
    EO_CHECK_IN(unweighted.text, "expected a line break after edge[1], found a space");
    eot::outcome const quiet = validate("1 2 5\n", [](eo::validator& v) {
        v.read_edges(1, 3, eo::weighted(1, 9), eo::unnamed);
    });
    EO_CHECK(!quiet.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO101"));
    eot::outcome const none = validate("", [](eo::validator& v) { EO_CHECK(v.read_edges(0, 3, "edge").empty()); });
    EO_CHECK(!none.stopped);
}

EO_TEST(a_grid_is_read_row_by_row_with_its_width_and_charset) {
    eot::outcome const good = validate("2 3\n.#.\n##.\n", [](eo::validator& v) {
        int const rows = v.read_int(1, 9, "rows");
        v.read_space();
        int const cols = v.read_int(1, 9, "cols");
        v.read_eoln();
        std::vector<std::string> const grid = v.read_grid(rows, cols, eo::charset(".#"), "grid");
        EO_CHECK_EQ(grid.size(), static_cast<std::size_t>(2));
        EO_CHECK_EQ(grid[1], std::string("##."));
    }, {"--eo-describe"});
    EO_CHECK(!good.stopped);
    EO_CHECK_IN(good.printed, "eo-describe value grid length 3 3 low=yes high=yes");
    eot::outcome const narrow =
        validate(".#.\n#.\n", [](eo::validator& v) { v.read_grid(2, 3, eo::charset(".#"), "grid"); });
    EO_CHECK_IN(narrow.text, "line 2, grid[2]: the line is 2 characters long, not 3..3");
    eot::outcome const stray =
        validate(".x.\n", [](eo::validator& v) { v.read_grid(1, 3, eo::charset(".#"), "grid"); });
    EO_CHECK_IN(stray.text, "line 1, grid[1]: the line holds \"x\", which is not in \".#\"");
}

EO_TEST(a_validator_that_exits_early_still_checks_the_end) {
    eot::outcome const early = validate("1\ngarbage\n", [](eo::validator& v) {
        (void)v.read_int(1, 9, "n");
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK_EQ(early.code, 3);
    EO_CHECK_IN(early.text, "expected the end of the input");
    eot::outcome const summed = validate("7\n", [](eo::validator& v) {
        eo::sum_limit total(5, "sum of n");
        total += v.read_int(1, 9, "n");
        v.read_eoln();
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK_EQ(summed.code, 3);
    EO_CHECK_EQ(summed.text, std::string("sum of n is 7, above 5"));
    eot::outcome const whole = validate("7\n", [](eo::validator& v) {
        (void)v.read_int(1, 9, "n");
        v.read_eoln();
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK(!whole.stopped);
}

EO_TEST(a_weighted_tree_or_graph_is_read_and_checked_like_an_unweighted_one) {
    eot::outcome const tree = validate("1 2 5\n3 2 9\n", [](eo::validator& v) {
        std::vector<eo::weighted_edge> const edges = v.read_tree(3, eo::weighted(1, 9), "edge");
        EO_CHECK_EQ(edges[1].u, 3);
        EO_CHECK_EQ(edges[1].w, 9);
    });
    EO_CHECK(!tree.stopped);
    eot::outcome const cycle =
        validate("1 2 1\n2 1 1\n", [](eo::validator& v) { v.read_tree(3, eo::weighted(1, 9), "edge"); });
    EO_CHECK_IN(cycle.text, "edge: edges 1 and 2 are both (1, 2)");
    eot::outcome const graph = validate("1 2 1\n2 3 2\n1 3 3\n", [](eo::validator& v) {
        v.read_graph(3, 3, eo::simple | eo::connected, eo::weighted(1, 3), "edge");
    });
    EO_CHECK(!graph.stopped);
    eot::outcome const split = validate("1 2 1\n", [](eo::validator& v) {
        v.read_graph(3, 1, eo::connected, eo::weighted(1, 9), "edge");
    });
    EO_CHECK_IN(split.text, "vertex 3 cannot be reached from vertex 1");
    eot::outcome const doubled = validate("1 2 1\n2 1 1\n", [](eo::validator& v) {
        v.read_graph(3, 2, eo::simple, eo::weighted(1, 9), "edge");
    });
    EO_CHECK_IN(doubled.text, "edge: edges 1 and 2 are both (1, 2)");
    eot::outcome const heavy = validate("1 2 10\n", [](eo::validator& v) {
        v.read_graph(3, 1, eo::any_graph, eo::weighted(1, 9), "edge");
    });
    EO_CHECK_IN(heavy.text, "line 1, edge.w[1]: 10 is above 9");
    eot::outcome const outside = validate("1 9 1\n", [](eo::validator& v) {
        v.read_graph(3, 1, eo::any_graph, eo::weighted(1, 9), "edge");
    });
    EO_CHECK_IN(outside.text, "edge 1 is (1, 9), outside 1..3");
}

namespace {

struct limits {
    int n;
    long long a;
    bool distinct;
};

bool operator==(limits const& left, limits const& right) {
    return left.n == right.n && left.a == right.a && left.distinct == right.distinct;
}

}  // namespace

EO_TEST(a_subtask_table_picks_the_row_of_this_test) {
    eot::outcome const first = validate("5\n", [](eo::validator& v) {
        limits const& chosen = v.subtasks<limits>({{0, {200000, 1000000000, false}},
                                                   {1, {10, 100, false}}})
                                   .without_group({200000, 1000000000, false});
        EO_CHECK_EQ(chosen.n, 10);
        v.read_int(1, chosen.n, "n");
        v.read_eoln();
        EO_CHECK(v.group().has_value());
        EO_CHECK_EQ(*v.group(), 1);
    }, {"--group", "1"});
    EO_CHECK(!first.stopped);

    eot::outcome const none = validate("5\n", [](eo::validator& v) {
        limits const& chosen = v.subtasks<limits>({{0, {200000, 1000000000, false}},
                                                   {1, {10, 100, false}}})
                                   .without_group({7, 7, true});
        EO_CHECK_EQ(chosen.n, 7);
        EO_CHECK(!v.group().has_value());
        v.read_int(1, chosen.n, "n");
        v.read_eoln();
    });
    EO_CHECK(!none.stopped);

    eot::outcome const stranger = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({{0, {200000, 1000000000, false}}, {1, {10, 100, false}}})
            .without_group({200000, 1000000000, false});
    }, {"--group=7"});
    EO_CHECK_IN(stranger.text, "no subtask 7; known: 0, 1");
}

EO_TEST(a_table_that_lists_a_subtask_twice_is_a_library_error) {
    eot::outcome const twice = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({{1, {10, 100, false}}, {1, {20, 200, false}}}).without_group({1, 1, false});
    });
    EO_CHECK_IN(twice.text, "subtask 1 is listed twice");
}

EO_TEST(two_rows_with_the_same_limits_are_a_copy_and_paste) {
    EO_CHECK(eo::detail::comparable<limits>::value);
    EO_CHECK(limits{1, 2, false} == limits{1, 2, false});
    eot::outcome const same = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({{0, {10, 100, false}}, {1, {10, 100, false}}}).without_group({10, 100, false});
        v.read_int(1, 10, "n");
        v.read_eoln();
    });
    EO_CHECK(!same.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO302"));
    eot::outcome const copied = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({{0, {10, 100, false}}, {1, {10, 100, false}}, {2, {10, 100, false}}})
            .without_group({10, 100, false});
        v.read_int(1, 10, "n");
        v.read_eoln();
    });
    EO_CHECK(!copied.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO302: subtasks 1 and 2 have the same limits");
}

EO_TEST(the_examples_row_may_have_the_full_limits) {
    eot::outcome const documented = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({
            {0, {200000, 1000000000, false}},
            {1, {10, 100, false}},
            {2, {200000, 1000000000, false}},
        }).without_group({200000, 1000000000, false});
        v.read_int(1, 10, "n");
        v.read_eoln();
    });
    EO_CHECK(!documented.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO302"));
    eot::outcome const last = validate("5\n", [](eo::validator& v) {
        v.subtasks<limits>({
            {1, {200000, 1000000000, false}},
            {2, {10, 100, false}},
            {0, {200000, 1000000000, false}},
        }).without_group({200000, 1000000000, false});
        v.read_int(1, 10, "n");
        v.read_eoln();
    });
    EO_CHECK(!last.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO302"));
}

EO_TEST(a_problem_with_subtasks_and_no_table_is_reported) {
    eot::outcome const untable = validate("5\n", [](eo::validator& v) {
        v.read_int(1, 10, "n");
        v.read_eoln();
    }, {"--group", "2"});
    EO_CHECK(!untable.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO301: this test is in subtask 2");
}

EO_TEST(cases_number_the_messages_inside_them) {
    eot::outcome const result = validate("2\n5\n99\n", [](eo::validator& v) {
        int const t = v.read_int(1, 9, "t");
        v.read_eoln();
        v.cases(t, [&] {
            v.read_int(1, 9, "n");
            v.read_eoln();
        });
    });
    EO_CHECK_EQ(result.text, std::string("case 2: line 3, n: 99 is above 9"));
    eot::outcome const whole = validate("2\n5\n6\n", [](eo::validator& v) {
        int const t = v.read_int(1, 9, "t");
        v.read_eoln();
        v.cases(t, [&] {
            v.read_int(1, 9, "n");
            v.read_eoln();
        });
    });
    EO_CHECK(!whole.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "note EO304: cases is used with no sum_limit");
}

EO_TEST(a_sum_limit_is_checked_once_at_the_end) {
    eot::outcome const inside = validate("2\n3\n4\n", [](eo::validator& v) {
        int const t = v.read_int(1, 9, "t");
        v.read_eoln();
        eo::sum_limit total(10, "sum of n");
        v.cases(t, [&] {
            total += v.read_int(1, 9, "n");
            v.read_eoln();
        });
        EO_CHECK_EQ(total.total(), 7LL);
    });
    EO_CHECK(!inside.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO304"));

    eot::outcome const over = validate("2\n8\n9\n", [](eo::validator& v) {
        int const t = v.read_int(1, 9, "t");
        v.read_eoln();
        eo::sum_limit total(10, "sum of n");
        v.cases(t, [&] {
            total += v.read_int(1, 9, "n");
            v.read_eoln();
        });
    });
    EO_CHECK_EQ(over.text, std::string("sum of n is 17, above 10"));
}

EO_TEST(a_sum_limit_still_holds_when_the_validator_finishes_first) {
    eot::outcome const result = validate("9\n", [](eo::validator& v) {
        static eo::sum_limit* escaped = nullptr;
        eo::sum_limit total(2, "sum of n");
        escaped = &total;
        total += v.read_int(1, 9, "n");
        v.read_eoln();
        v.complete();
        EO_CHECK(escaped != nullptr);
    });
    EO_CHECK_EQ(result.text, std::string("sum of n is 9, above 2"));
}

EO_TEST(a_condition_of_your_own_carries_its_message) {
    eot::outcome const plain = validate("5 3\n", [](eo::validator& v) {
        int const l = v.read_int(1, 9, "l");
        v.read_space();
        int const r = v.read_int(1, 9, "r");
        v.require(l <= r, "l = {} is greater than r = {}", l, r);
    });
    EO_CHECK_EQ(plain.text, std::string("line 1: l = 5 is greater than r = 3"));
    eot::outcome const structural = validate("1 1 2\n", [](eo::validator& v) {
        std::vector<int> const a = v.read_ints(3, 1, 9, "a");
        v.require(eo::all_distinct(a), "a");
    });
    EO_CHECK_EQ(structural.text, std::string("line 1: a: elements 1 and 2 are both 1"));
    eot::outcome const nameless = validate("1 1\n", [](eo::validator& v) {
        std::vector<int> const a = v.read_ints(2, 1, 9, "a");
        v.require(eo::all_distinct(a), eo::unnamed);
    });
    EO_CHECK_EQ(nameless.text, std::string("line 1: elements 1 and 2 are both 1"));
    eot::outcome const sorted = validate("1 2\n", [](eo::validator& v) {
        std::vector<int> const a = v.read_ints(2, 1, 9, "a");
        v.require(eo::is_sorted(a), "a");
        v.read_eoln();
    });
    EO_CHECK(!sorted.stopped);
    eot::outcome const unsorted = validate("2 1\n", [](eo::validator& v) {
        std::vector<int> const a = v.read_ints(2, 1, 9, "a");
        v.require(eo::is_sorted(a), "a");
    });
    EO_CHECK_IN(unsorted.text, "element 1 is 2 and element 2 is 1");
}

EO_TEST(features_are_declared_and_marked) {
    eot::outcome const result = validate("1 2\n", [](eo::validator& v) {
        v.feature("pair");
        std::vector<int> const a = v.read_ints(2, 1, 9, "a");
        if (a.size() == 2) v.saw("pair");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    eot::outcome const unknown = validate("1\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
        v.saw("never declared");
    });
    EO_CHECK_IN(unknown.text, "saw(\"never declared\") names a feature that was never declared");
}

EO_TEST(describe_prints_the_table_the_bounds_and_the_features) {
    eot::outcome const result = validate("5\n2 1 9\n1.000 abc\nhello\n", [](eo::validator& v) {
        v.feature("small");
        v.saw("small");
        v.read_int(1, 5, "n");
        v.read_eoln();
        v.read_ints(3, 1, 9, "a");
        v.read_eoln();
        v.read_real(0.0, 1.0, 1, 3, "p");
        v.read_space();
        v.read_token(3, 5, eo::charset("a-z"), "s");
        v.read_eoln();
        v.read_line(1, 5, eo::charset("a-z"), "t");
    }, {"--eo-describe", "--group", "1"});
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(result.printed, "eo-describe group 1");
    EO_CHECK_IN(result.printed, "eo-describe value n int 1 5 low=no high=yes");
    EO_CHECK_IN(result.printed, "eo-describe value a int 1 9 low=yes high=yes");
    EO_CHECK_IN(result.printed, "eo-describe value p real 0 1 low=no high=yes");
    EO_CHECK_IN(result.printed, "eo-describe value s length 3 5 low=yes high=no");
    EO_CHECK_IN(result.printed, "eo-describe value t length 1 5 low=no high=yes");
    EO_CHECK_IN(result.printed, "eo-describe feature small seen=yes");
}

EO_TEST(a_bound_computed_from_another_value_is_not_an_inconsistency) {
    eot::outcome const result = validate("2\n2 5\n1 2\n3 9\n4 5 6\n", [](eo::validator& v) {
        int const t = v.read_int(1, 10000, "t");
        v.read_eoln();
        v.cases(t, [&] {
            int const n = v.read_int(1, 200000, "n");
            v.read_space();
            long long const k = v.read_long(1, 1000000000, "k");
            v.read_eoln();
            v.read_longs(n, 1, k, "a");
            v.read_eoln();
        });
    }, {"--eo-describe"});
    EO_CHECK(!result.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO107"));
    EO_CHECK_IN(result.printed, "eo-describe value a int * * low=yes high=no");
    EO_CHECK_IN(result.printed, "eo-describe value n int 1 200000 low=no high=no");
}

EO_TEST(the_same_name_with_two_bounds_at_two_places_is_still_pointed_out) {
    eot::outcome const result = validate("3 4\n", [](eo::validator& v) {
        v.read_int(1, 9, "twice");
        v.read_space();
        v.read_int(1, 8, "twice");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO107: \"twice\" is read as 1..8 here and as 1..9 elsewhere");
}

EO_TEST(a_name_read_as_two_different_kinds_is_pointed_out) {
    eot::outcome const mixed = validate("3 0.5\n", [](eo::validator& v) {
        v.read_int(1, 9, "both");
        v.read_space();
        v.read_real(0.0, 1.0, 1, 1, "both");
        v.read_eoln();
    });
    EO_CHECK(!mixed.stopped);
    EO_CHECK_IN(warnings_of_last_run(),
                "warning EO107: \"both\" is read as a number here and as an integer elsewhere");
    eot::outcome const sized = validate("3 abc\n", [](eo::validator& v) {
        v.read_int(1, 9, "both");
        v.read_space();
        v.read_token(1, 5, eo::charset("a-z"), "both");
        v.read_eoln();
    });
    EO_CHECK(!sized.stopped);
    EO_CHECK_IN(warnings_of_last_run(),
                "warning EO107: \"both\" is read as a length here and as an integer elsewhere");
}

EO_TEST(a_length_or_a_real_read_with_two_bounds_is_pointed_out) {
    eot::outcome const lengths = validate("abc abcd\n", [](eo::validator& v) {
        v.read_token(1, 5, eo::charset("a-z"), "s");
        v.read_space();
        v.read_token(1, 6, eo::charset("a-z"), "s");
        v.read_eoln();
    });
    EO_CHECK(!lengths.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO107: \"s\" is read as 1..6 here and as 1..5 elsewhere");
    eot::outcome const reals = validate("0.5 0.5\n", [](eo::validator& v) {
        v.read_real(0.0, 1.0, 1, 1, "p");
        v.read_space();
        v.read_real(0.0, 2.0, 1, 1, "p");
        v.read_eoln();
    });
    EO_CHECK(!reals.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO107: \"p\" is read as 0..2 here and as 0..1 elsewhere");
}

EO_TEST(the_validator_reads_stdin_when_no_file_is_named) {
    scratch_file input("42\n");
    int const saved = ::dup(0);
    int const opened = ::open(input.path.c_str(), O_RDONLY);
    ::dup2(opened, 0);
    char program[] = "validator.cpp";
    char* arguments[] = {program};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::validator v(1, arguments);
        EO_CHECK_EQ(v.read_int(1, 99, "n"), 42);
        v.read_eoln();
    });
    ::dup2(saved, 0);
    ::close(saved);
    ::close(opened);
    EO_CHECK(!result.stopped);
}

EO_TEST(one_program_holds_one_validator) {
    char program[] = "validator.cpp";
    char* arguments[] = {program};
    eot::outcome const result = eot::run_until_stop([&] {
        eo::validator first(1, arguments);
        eo::validator second(1, arguments);
        (void)second.at_eof();
    });
    EO_CHECK_IN(result.text, "this program already has a validator");
}

EO_TEST(a_group_that_is_not_a_number_is_a_library_error) {
    eot::outcome const text = validate("1\n", [](eo::validator&) {}, {"--group", "two"});
    EO_CHECK_IN(text.text, "--group two is not a testset index");
    eot::outcome const missing = validate("1\n", [](eo::validator&) {}, {"--group"});
    EO_CHECK_IN(missing.text, "--group needs the testset index after it");
    eot::outcome const ignored = validate("1\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
    }, {"-x"});
    EO_CHECK(!ignored.stopped);
}

EO_TEST(crlf_line_endings_are_folded_and_noted) {
    eot::outcome const result = validate("1\r\n", [](eo::validator& v) {
        v.read_int(1, 9, "n");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "note EO110: this input has CRLF line endings");
}

EO_TEST(a_read_without_a_name_or_bounds_still_works_and_warns) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    eot::outcome const result = validate("1 2 3 4 5 6\n", [](eo::validator& v) {
        v.read_int(1, 9);
        v.read_space();
        v.read_int("b");
        v.read_space();
        v.read_int();
        v.read_space();
        v.read_long(1, 9);
        v.read_space();
        v.read_long("e");
        v.read_space();
        v.read_token("f");
        v.read_eoln();
    });
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
    EO_CHECK(!result.stopped);
    std::string const said = warnings_of_last_run();
    EO_CHECK_IN(said, "warning EO101: this value is read without a name");
    EO_CHECK_IN(said, "warning EO102: this value is read without bounds");
    EO_CHECK_IN(said, "warning EO108: this token is read with no length and no charset");
}

EO_TEST(a_bound_computed_from_a_value_read_before_is_not_called_nearly_round) {
    eot::outcome const above = validate("200000 200001\n", [](eo::validator& v) {
        int const n = v.read_int(1, 200000, "n");
        v.read_space();
        v.read_int(1, n + 1, "m");
        v.read_eoln();
    });
    EO_CHECK(!above.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO106"));
    eot::outcome const result = validate("200000 199999 199999\n100001 7\n", [](eo::validator& v) {
        int const n = v.read_int(1, 200000, "n");
        v.read_space();
        v.read_int(1, n - 1, "m");
        v.read_space();
        v.read_int(n - 1, n, "low");
        v.read_eoln();
        int const q = v.read_int(1, 1000000, "q");
        v.read_space();
        v.read_int(1, q, "x");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO106"));
}

EO_TEST(bounds_that_look_wrong_are_pointed_out) {
    eot::outcome const result = validate("1 2 3 4 5\n", [](eo::validator& v) {
        v.read_int(-2147483648LL, 2147483647LL, "whole");
        v.read_space();
        v.read_int(1, 3000000000LL, "wide");
        v.read_space();
        v.read_int(1, 200001, "nearly");
        v.read_space();
        v.read_int(1, 9, "twice");
        v.read_space();
        v.read_int(1, 8, "twice");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    std::string const said = warnings_of_last_run();
    EO_CHECK_IN(said, "warning EO104: the bounds are the whole range of an int");
    EO_CHECK_IN(said, "warning EO105: the bounds 1..3000000000 do not fit an int");
    EO_CHECK_IN(said, "note EO106: the bounds 1..200001 are one away from a round number");
    EO_CHECK_IN(said, "warning EO107: \"twice\" is read as 1..8 here and as 1..9 elsewhere");
}

EO_TEST(bounds_at_the_ends_of_a_long_long_are_not_near_a_round_number) {
    EO_CHECK(!eo::detail::nearly_round(std::numeric_limits<long long>::max()));
    EO_CHECK(!eo::detail::nearly_round(std::numeric_limits<long long>::min()));
    EO_CHECK(eo::detail::nearly_round(1000001));
    eot::outcome const result = validate("5 -5\n", [](eo::validator& v) {
        v.read_long(1, std::numeric_limits<long long>::max(), "top");
        v.read_space();
        v.read_long(std::numeric_limits<long long>::min(), 0, "bottom");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO106"));
}

EO_TEST(a_length_bound_may_be_the_largest_long_long) {
    eot::outcome const result = validate("abc\nhello world\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_token(1, std::numeric_limits<long long>::max(), eo::charset("a-z"), "word"),
                    std::string("abc"));
        v.read_eoln();
        EO_CHECK_EQ(v.read_line(1, std::numeric_limits<long long>::max(), eo::charset("a-z "), "line"),
                    std::string("hello world"));
    });
    EO_CHECK(!result.stopped);
}

EO_TEST(a_token_or_a_number_read_loosely_is_pointed_out) {
    eot::outcome const result = validate("abc 0.5 hello\n", [](eo::validator& v) {
        v.read_token(1, 5, "s");
        v.read_space();
        v.read_real(0.0, 1.0, "p");
        v.read_space();
        v.read_line(1, 20, "title");
    });
    EO_CHECK(!result.stopped);
    std::string const said = warnings_of_last_run();
    EO_CHECK_IN(said, "warning EO108: this token is read with no charset");
    EO_CHECK_IN(said, "warning EO109: this number is read without a rule on its digits");
    EO_CHECK_IN(said, "warning EO108: this line is read with no charset");
}

EO_TEST(a_charset_range_that_runs_backwards_is_a_library_error) {
    eot::outcome const result = eot::run_until_stop([] { eo::charset backwards("z-a"); });
    EO_CHECK_IN(result.text, "the character range \"z-a\" in charset(\"z-a\") runs backwards");
    eo::charset const dash("a-");
    EO_CHECK(dash.has('a'));
    EO_CHECK(dash.has('-'));
    EO_CHECK(!dash.has('b'));
}

EO_TEST(a_charset_can_be_built_from_a_string) {
    std::string const spec = std::string("0-9") + "_";
    eo::charset const digits(spec);
    EO_CHECK(digits.has('7'));
    EO_CHECK(digits.has('_'));
    EO_CHECK(!digits.has('a'));
    EO_CHECK_EQ(digits.text(), std::string("0-9_"));
}

EO_TEST(a_validator_reads_any_long_when_it_waives_the_bounds) {
    eot::outcome const extremes = validate("-9223372036854775808 9223372036854775807\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_long(eo::any, "low"), std::numeric_limits<long long>::min());
        v.read_space();
        EO_CHECK_EQ(v.read_long(eo::any, "high"), std::numeric_limits<long long>::max());
        v.read_eoln();
    });
    EO_CHECK(!extremes.stopped);
    EO_CHECK_EQ(extremes.printed, std::string(""));
    eot::outcome const beyond =
        validate("9223372036854775808\n", [](eo::validator& v) { v.read_long(eo::any, "x"); });
    EO_CHECK_EQ(beyond.code, 3);
    EO_CHECK_EQ(beyond.text, std::string("line 1, x: expected an integer, found \"9223372036854775808\": it does not "
                                         "fit the type it is read into"));
}

EO_TEST(a_warning_points_at_the_line_that_raised_it) {
    int wanted = 0;
    eot::outcome const result = validate("5\n", [&wanted](eo::validator& v) {
        wanted = __LINE__ + 1;
        v.read_int(1, 200001, "n");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(warnings_of_last_run(), eo::fmt("validate.inc:{}: note EO106", wanted).c_str());
}

EO_TEST(a_message_keeps_valid_utf8_and_escapes_the_rest) {
    using eo::detail::escaped;
    EO_CHECK_EQ(escaped("привіт, €5 😀"), std::string("привіт, €5 😀"));
    EO_CHECK_EQ(escaped("\x7f\t\x1f "), std::string("\\x7f\\x09\\x1f "));
    EO_CHECK_EQ(escaped("\x80"), std::string("\\x80"));
    EO_CHECK_EQ(escaped("\xc1\xbf"), std::string("\\xc1\\xbf"));
    EO_CHECK_EQ(escaped("\xc2\x80"), std::string("\xc2\x80"));
    EO_CHECK_EQ(escaped("\xdf\xbf"), std::string("\xdf\xbf"));
    EO_CHECK_EQ(escaped("\xe0\x80\xaf"), std::string("\\xe0\\x80\\xaf"));
    EO_CHECK_EQ(escaped("\xe0\xa0\x80"), std::string("\xe0\xa0\x80"));
    EO_CHECK_EQ(escaped("\xed\xa0\x80"), std::string("\\xed\\xa0\\x80"));
    EO_CHECK_EQ(escaped("\xed\x9f\xbf"), std::string("\xed\x9f\xbf"));
    EO_CHECK_EQ(escaped("\xef\xbf\xbf"), std::string("\xef\xbf\xbf"));
    EO_CHECK_EQ(escaped("\xf0\x8f\xbf\xbf"), std::string("\\xf0\\x8f\\xbf\\xbf"));
    EO_CHECK_EQ(escaped("\xf0\x90\x80\x80"), std::string("\xf0\x90\x80\x80"));
    EO_CHECK_EQ(escaped("\xf4\x8f\xbf\xbf"), std::string("\xf4\x8f\xbf\xbf"));
    EO_CHECK_EQ(escaped("\xf4\x90\x80\x80"), std::string("\\xf4\\x90\\x80\\x80"));
    EO_CHECK_EQ(escaped("\xf5\x80\x80\x80"), std::string("\\xf5\\x80\\x80\\x80"));
    EO_CHECK_EQ(escaped("\xd0"), std::string("\\xd0"));
    EO_CHECK_EQ(escaped("\xe2\x82"), std::string("\\xe2\\x82"));
    EO_CHECK_EQ(escaped("\xe2\x82x"), std::string("\\xe2\\x82x"));
    EO_CHECK_EQ(eo::detail::shorten("\x80\x80\x80\x80\x80", 2), std::string("\\x80\\x80..."));
    EO_CHECK_EQ(eo::detail::shorten("ab\x80\x80\x80\x80", 3), std::string("ab\\x80..."));
    EO_CHECK_EQ(eo::detail::shorten("abcd\xf0\x9f\x98\x80", 7), std::string("abcd..."));
    EO_CHECK_EQ(eo::detail::shorten("abcd\xf0\x9f\x98\x80z", 8), std::string("abcd😀..."));
}

EO_TEST(a_long_value_is_shortened_in_the_message) {
    eot::outcome const result = validate(std::string(60, '7') + "x\n", [](eo::validator& v) {
        v.read_int(1, 3, "n");
    });
    EO_CHECK_IN(result.text, "...");
    EO_CHECK(result.text.size() < 160);
    eot::outcome const capped = validate(std::string(60, 'a') + "\n", [](eo::validator& v) {
        v.read_token(1, 3, eo::charset("a-z"), "s");
    });
    EO_CHECK_IN(capped.text, "is longer than 3 characters");
    eot::outcome const line = validate(std::string(60, 'a') + "\n", [](eo::validator& v) {
        v.read_line(1, 3, eo::charset("a-z"), "title");
    });
    EO_CHECK_IN(line.text, "the line is longer than 3 characters");
}

EO_TEST(structural_checks_pass_as_well_as_fail) {
    eot::outcome const result = validate("1 2 3\n5 1 2\n1 2\n2 3\n1 3\n", [](eo::validator& v) {
        std::vector<int> const a = v.read_ints(3, 1, 9, "a");
        v.read_eoln();
        v.require(eo::all_distinct(a), "a");
        std::vector<int> const p = v.read_ints(3, eo::any, "p");
        v.read_eoln();
        EO_CHECK(!eo::is_permutation(p));
        EO_CHECK_IN(eo::is_permutation(p).message(), "element 1 is 5, outside 1..3");
        std::vector<eo::edge> const edges = v.read_graph(3, 3, eo::any_graph, "edge");
        EO_CHECK(!eo::is_tree(3, edges));
        EO_CHECK_IN(eo::is_tree(3, edges).message(), "a tree on 3 vertices has 2 edges, not 3");
    });
    EO_CHECK(!result.stopped);
}

EO_TEST(a_number_whose_token_cannot_be_parsed_is_reported) {
    eot::outcome const zeroes = validate("00.5\n", [](eo::validator& v) { v.read_real(0.0, 1.0, 1, 1, "p"); });
    EO_CHECK_IN(zeroes.text, "line 1, p: expected a number, found \"00.5\"");
    EO_CHECK_IN(zeroes.text, "leading zero");
    eot::outcome const point = validate("5.\n", [](eo::validator& v) { v.read_real(0.0, 9.0, 0, 2, "p"); });
    EO_CHECK_IN(point.text, "expected a number, found \"5.\"");
}

EO_TEST(a_value_read_with_any_bounds_still_works) {
    eot::outcome const result = validate("7 abc 0.5\n", [](eo::validator& v) {
        EO_CHECK_EQ(v.read_int(eo::any, "n"), 7);
        v.read_space();
        EO_CHECK_EQ(v.read_token(eo::any, "s"), std::string("abc"));
        v.read_space();
        v.read_real(0.0, 1.0, 1, 1, "p");
        v.read_eoln();
    });
    EO_CHECK(!result.stopped);
}

EO_TEST(a_real_or_a_token_read_without_a_name_warns_too) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    eot::outcome const result = validate("0.5 abc\n", [](eo::validator& v) {
        v.read_real(0.0, 1.0, 1, 1);
        v.read_space();
        v.read_token(1, 5, eo::charset("a-z"));
        v.read_eoln();
    });
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
    EO_CHECK(!result.stopped);
    std::string const said = warnings_of_last_run();
    EO_CHECK_IN(said, "warning EO101: this value is read without a name");
}

EO_TEST(a_validator_near_its_time_limit_is_reported) {
    eot::outcome const quick = validate("5\n", [](eo::validator& v) {
        v.read_int(1, 10, "n");
        v.read_eoln();
    });
    EO_CHECK(!quick.stopped);
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO303"));

    eot::outcome const slow = validate("5\n", [](eo::validator& v) {
        v.read_int(1, 10, "n");
        v.read_eoln();
        eo::detail::diagnostics::shared().clock().started -= std::chrono::seconds(16);
    });
    EO_CHECK(!slow.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO303: the validator ran for ");
    EO_CHECK_IN(warnings_of_last_run(), " ms of its 30000 ms limit");
}

EO_TEST(an_element_is_named_by_index_and_covered_under_its_sequence) {
    eot::outcome const result = validate("3\n4 9 1\n", [](eo::validator& v) {
        int const n = v.read_int(1, 5, "n");
        v.read_eoln();
        for (int at = 1; at <= n; at++) {
            v.read_int(1, 9, eo::element("a", at));
            if (at < n) v.read_space();
        }
        v.read_eoln();
    }, {"--eo-describe", "--group", "1"});
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(result.printed, "eo-describe value a int 1 9 low=yes high=yes");
    EO_CHECK(!eot::contains(result.printed, "value a[1]"));
    EO_CHECK(!eot::contains(result.printed, "value a[3]"));

    eot::outcome const refused = validate("2\n4 12\n", [](eo::validator& v) {
        int const n = v.read_int(1, 5, "n");
        v.read_eoln();
        for (int at = 1; at <= n; at++) {
            v.read_int(1, 9, eo::element("a", at));
            if (at < n) v.read_space();
        }
        v.read_eoln();
    });
    EO_CHECK(refused.stopped);
    EO_CHECK_IN(refused.text, "a[2]");
}

EO_TEST(a_validator_reads_a_token_that_matches_a_pattern) {
    eo::pattern const name("[A-Z][a-z]{0,9}");
    eot::outcome const good = validate("Anna Bo\nYES\n", [&](eo::validator& v) {
        EO_CHECK_EQ(v.read_token(name, "first"), std::string("Anna"));
        v.read_space();
        EO_CHECK_EQ(v.read_token(name, "last"), std::string("Bo"));
        v.read_eoln();
        EO_CHECK_EQ(v.read_token(eo::pattern("YES|NO"), "answer"), std::string("YES"));
        v.read_eoln();
    });
    EO_CHECK(!good.stopped);
    EO_CHECK_EQ(warnings_of_last_run(), std::string(""));
    eot::outcome const lower = validate("anna\n", [&](eo::validator& v) { v.read_token(name, "first"); });
    EO_CHECK_EQ(lower.code, 3);
    EO_CHECK_EQ(lower.text, std::string("line 1, first: \"anna\" does not match \"[A-Z][a-z]{0,9}\""));
    eot::outcome const longer = validate("Annabellelee\n", [&](eo::validator& v) { v.read_token(name, "first"); });
    EO_CHECK_EQ(longer.text, std::string("line 1, first: a token that starts \"Annabellele\" is longer than the 10 "
                                         "characters \"[A-Z][a-z]{0,9}\" allows"));
    eot::outcome const missing = validate("\n", [&](eo::validator& v) { v.read_token(name, "first"); });
    EO_CHECK_EQ(missing.text, std::string("line 1, first: expected a token, found a line break"));
    eot::outcome const loose = validate("5\n", [&](eo::validator& v) {
        v.read_token(eo::pattern("[0-9]+"), "k");
        v.read_eoln();
    });
    EO_CHECK(!loose.stopped);
    eot::outcome const unnamed = validate("x\n", [&](eo::validator& v) {
        v.read_token(eo::pattern("y"), eo::unnamed);
    });
    EO_CHECK_EQ(unnamed.text, std::string("line 1: \"x\" does not match \"y\""));
}

EO_TEST(a_token_pattern_that_only_matches_with_a_blank_is_pointed_out) {
    eot::outcome const spaced = validate("ab\n", [](eo::validator& v) { v.read_token(eo::pattern("[a-z] {1,5}"), "s"); });
    EO_CHECK_EQ(spaced.code, 3);
    EO_CHECK_IN(warnings_of_last_run(), "warning EO113: \"[a-z] {1,5}\" matches only text with a blank in it, and a "
                                        "token holds none");
    for (char const* fine : {"[a-z ]+", "( |x)", "a|\\ b", "[^ ]", "( a)?", "(a|b)( c)*"}) {
        validate("x\n", [&](eo::validator& v) { (void)eot::run_until_stop([&] { v.read_token(eo::pattern(fine), "s"); }); });
        EO_CHECK(!eot::contains(warnings_of_last_run(), "EO113"));
    }
    for (char const* blank : {"a b", "(\\ |\t)", "[ \t]+", "x( y)+"}) {
        validate("x\n", [&](eo::validator& v) { (void)eot::run_until_stop([&] { v.read_token(eo::pattern(blank), "s"); }); });
        EO_CHECK_IN(warnings_of_last_run(), "EO113");
    }
    validate("a b\n", [](eo::validator& v) { v.read_line(eo::pattern("a b"), "s"); });
    EO_CHECK(!eot::contains(warnings_of_last_run(), "EO113"));
}

EO_TEST(a_validator_reads_tokens_and_a_line_that_match_a_pattern) {
    eo::pattern const word("[a-z]{1,3}");
    eot::outcome const good = validate("ab c def\nhello world 42\n", [&](eo::validator& v) {
        std::vector<std::string> const words = v.read_tokens(3, word, "w");
        EO_CHECK(words == std::vector<std::string>({"ab", "c", "def"}));
        v.read_eoln();
        EO_CHECK_EQ(v.read_line(eo::pattern("[a-z]+( [a-z0-9]+)*"), "line"), std::string("hello world 42"));
    });
    EO_CHECK(!good.stopped);
    eot::outcome const third = validate("ab c dxf1\n", [&](eo::validator& v) { v.read_tokens(3, word, "w"); });
    EO_CHECK_EQ(third.text,
                std::string("line 1, w[3]: a token that starts \"dxf1\" is longer than the 3 characters \"[a-z]{1,3}\" "
                            "allows"));
    eot::outcome const line = validate("hello  world\n", [&](eo::validator& v) {
        v.read_line(eo::pattern("[a-z]+( [a-z]+)*"), "line");
    });
    EO_CHECK_EQ(line.text, std::string("line 1, line: the line \"hello  world\" does not match \"[a-z]+( [a-z]+)*\""));
    eot::outcome const wide = validate("abcdef\n", [&](eo::validator& v) { v.read_line(word, "line"); });
    EO_CHECK_EQ(wide.text, std::string("line 1, line: a line that starts \"abcd\" is longer than the 3 characters "
                                       "\"[a-z]{1,3}\" allows"));
    eot::outcome const empty = validate("\n", [&](eo::validator& v) { v.read_line(eo::pattern("(x)?"), "line"); });
    EO_CHECK(!empty.stopped);
    eot::outcome const unended = validate("ab", [&](eo::validator& v) { v.read_line(word, "line"); });
    EO_CHECK_EQ(unended.text, std::string("line 1, line: the line has no line break at its end"));
    eot::outcome const crlf = validate("ab\r\ncd\r\n", [&](eo::validator& v) {
        EO_CHECK_EQ(v.read_line(word, "line"), std::string("ab"));
        EO_CHECK_EQ(v.read_token(word, "w"), std::string("cd"));
        v.read_eoln();
    });
    EO_CHECK(!crlf.stopped);
    for (char const* one : {"(a*){0}b", "(){3}b", "(|b)", "(b|a*){0}b", "(ab){0,5}|b"}) {
        eot::outcome const capped = validate("bbbbbbbbbbbb\n", [&](eo::validator& v) {
            v.read_token(eo::pattern(one), "t");
        });
        EO_CHECK_IN(capped.text, "a token that starts \"b");
    }
    for (char const* one : {"(ab)*b*", "(ab|b)+", "b{2,}", "(b{3}|a)*"}) {
        eot::outcome const endless = validate("bbbbbbbbbbbb\n", [&](eo::validator& v) {
            v.read_token(eo::pattern(one), "t");
            v.read_eoln();
        });
        EO_CHECK(!endless.stopped);
    }
    eot::outcome const empty_pattern = validate("x\n", [&](eo::validator& v) { v.read_line(eo::pattern(""), "line"); });
    EO_CHECK_EQ(empty_pattern.text,
                std::string("line 1, line: a line that starts \"x\" is longer than the 0 characters \"\" allows"));
}

EO_TEST(a_huge_token_read_against_an_endless_pattern_is_noted) {
    std::string const huge(eo::detail::mebibyte + 3, 'a');
    eot::outcome const read = validate(huge + "\n", [&](eo::validator& v) {
        EO_CHECK_EQ(v.read_token(eo::pattern("a+"), "s").size(), huge.size());
        v.read_eoln();
    });
    EO_CHECK(!read.stopped);
    EO_CHECK_IN(warnings_of_last_run(), "note EO111: a token of 1048579 bytes was held in memory");
}
