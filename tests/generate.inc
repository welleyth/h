namespace {

struct made {
    bool stopped = false;
    int code = 0;
    std::string text;
    std::string test;
    std::string log;
};

template <class Body>
made generate(std::vector<char const*> given, Body&& body) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    std::string const test_path = temporary_path();
    std::string const log_path = temporary_path();
    made result;
    {
        swapped_descriptors kept;
        int const to = ::open(test_path.c_str(), O_WRONLY | O_TRUNC);
        int const grumbles = ::open(log_path.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(to, 1);
        ::dup2(grumbles, 2);
        std::string program = "gen.cpp";
        std::vector<char*> arguments{program.data()};
        for (char const* one : given) arguments.push_back(const_cast<char*>(one));
        try {
            eo::generator g(static_cast<int>(arguments.size()), arguments.data());
            body(g);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        std::fflush(stdout);
        std::fflush(stderr);
        ::close(to);
        ::close(grumbles);
    }
    result.test = slurp(test_path);
    result.log = slurp(log_path);
    ::unlink(test_path.c_str());
    ::unlink(log_path.c_str());
    forget_the_environment();
    if (std::getenv("EOT_TRACE") != nullptr && result.stopped)
        std::fprintf(stderr, "   generator: %s\n", result.text.c_str());
    return result;
}

}  // namespace

EO_TEST(a_generator_that_cannot_write_its_test_fails) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    std::string const log_path = temporary_path();
    made result;
    {
        swapped_descriptors kept;
        int const grumbles = ::open(log_path.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(grumbles, 2);
        ::close(1);
        std::string program = "gen.cpp";
        std::vector<char*> arguments{program.data()};
        try {
            eo::generator g(static_cast<int>(arguments.size()), arguments.data());
            g.out.line(std::string(100, 'x'));
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        std::clearerr(stdout);
        std::fflush(stderr);
        ::close(grumbles);
    }
    result.log = slurp(log_path);
    ::unlink(log_path.c_str());
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_IN(result.text, "the test could not be written: ");
    EO_CHECK_IN(result.log, "the test could not be written: ");
}

EO_TEST(a_generator_whose_earlier_write_failed_fails_too) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    std::string const test_path = temporary_path();
    std::string const log_path = temporary_path();
    made result;
    {
        swapped_descriptors kept;
        int const to = ::open(test_path.c_str(), O_WRONLY | O_TRUNC);
        int const grumbles = ::open(log_path.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(grumbles, 2);
        std::string program = "gen.cpp";
        std::vector<char*> arguments{program.data()};
        try {
            eo::generator g(static_cast<int>(arguments.size()), arguments.data());
            ::close(1);
            std::fputs("lost", stdout);
            EO_CHECK(std::fflush(stdout) != 0);
            ::dup2(to, 1);
            g.out.line(1);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
            result.code = reached.code;
            result.text = reached.text;
        }
        std::clearerr(stdout);
        std::fflush(stderr);
        ::close(to);
        ::close(grumbles);
    }
    ::unlink(test_path.c_str());
    ::unlink(log_path.c_str());
    EO_CHECK_EQ(result.code, 3);
    EO_CHECK_EQ(result.text, std::string("the test could not be written: an earlier write to stdout failed"));
}

EO_TEST(a_generator_writes_a_test_from_its_options) {
    made const result = generate({"-n=5", "-max=9"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 200000);
        int const top = g.option<int>("max", 1, 1000000000, 1000000000);
        std::vector<long long> const a = g.rng("values").ints(n, 1, top);
        g.out.line(n);
        g.out.line(a);
    });
    EO_CHECK(!result.stopped);
    EO_CHECK(result.test.rfind("5\n", 0) == 0);
    EO_CHECK_EQ(result.test.back(), '\n');
    made const again = generate({"-n=5", "-max=9"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 200000);
        int const top = g.option<int>("max", 1, 1000000000, 1000000000);
        g.out.line(n);
        g.out.line(g.rng("values").ints(n, 1, top));
    });
    EO_CHECK_EQ(again.test, result.test);
    made const other = generate({"-n=5", "-max=9", "-seed=2"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 200000);
        int const top = g.option<int>("max", 1, 1000000000, 1000000000);
        g.out.line(n);
        g.out.line(g.rng("values").ints(n, 1, top));
    });
    EO_CHECK(other.test != result.test);
}

EO_TEST(every_option_is_declared_with_its_range) {
    made const numbers = generate({"-n=7", "-big=900", "-p=0.25", "-flag=true", "-shape=sorted",
                                   "-kind=b"}, [](eo::generator& g) {
        EO_CHECK_EQ(g.option<std::string>("kind", {"a", "b"}), std::string("b"));
        EO_CHECK_EQ(g.option<int>("n", 1, 9), 7);
        EO_CHECK_EQ(g.option<long long>("big", 1, 1000), 900LL);
        EO_CHECK(g.option<double>("p", 0.0, 1.0) == 0.25);
        EO_CHECK(g.option<bool>("flag", false));
        EO_CHECK_EQ(g.option<std::string>("shape", {"random", "sorted"}, "random"), std::string("sorted"));
        g.out.line("ok");
    });
    EO_CHECK(!numbers.stopped);
    made const defaults = generate({"-n=7"}, [](eo::generator& g) {
        EO_CHECK_EQ(g.option<int>("n", 1, 9), 7);
        EO_CHECK_EQ(g.option<int>("max", 1, 99, 50), 50);
        EO_CHECK(!g.option<bool>("flag", false));
        EO_CHECK_EQ(g.option<std::string>("shape", {"random", "sorted"}, "random"), std::string("random"));
        EO_CHECK_EQ(g.option<std::string>("kind", {"a", "b"}), std::string("a"));
        g.out.line("ok");
    });
    EO_CHECK(defaults.stopped);
    EO_CHECK_EQ(defaults.text, std::string("-kind is required"));
}

EO_TEST(an_optional_option_is_nothing_when_it_is_not_given) {
    made const given = generate({"-n=7", "-p=0.5"}, [](eo::generator& g) {
        std::optional<int> const n = g.option<std::optional<int>>("n", 1, 9);
        std::optional<long long> const big = g.option<std::optional<long long>>("big", 1, 1000);
        std::optional<double> const p = g.option<std::optional<double>>("p", 0, 1);
        EO_CHECK(n.has_value() && *n == 7);
        EO_CHECK(!big.has_value());
        EO_CHECK(p.has_value() && *p == 0.5);
        g.out.line("ok");
    });
    EO_CHECK(!given.stopped);
    made const outside = generate({"-n=10"}, [](eo::generator& g) {
        (void)g.option<std::optional<int>>("n", 1, 9);
    });
    EO_CHECK(outside.stopped);
    EO_CHECK_EQ(outside.text, std::string("-n=10 is above 9"));
    made const described = generate({"--eo-describe"}, [](eo::generator& g) {
        EO_CHECK(!g.option<std::optional<int>>("n", 1, 9).has_value());
        g.out.line("this is not printed");
    });
    EO_CHECK(!described.stopped);
    EO_CHECK_IN(described.test, "eo-describe option n an integer 1..9 optional\n");
}

EO_TEST(an_option_the_declaration_refuses_stops_the_generator) {
    auto const with = [](std::vector<char const*> given) {
        return generate(given, [](eo::generator& g) {
            (void)g.option<int>("n", 1, 9);
            (void)g.option<double>("p", 0.0, 1.0, 0.5);
            (void)g.option<bool>("flag", false);
            (void)g.option<std::string>("shape", {"random", "sorted"}, "random");
            g.out.line("ok");
        });
    };
    EO_CHECK_EQ(with({"-n=0"}).text, std::string("-n=0 is below 1"));
    EO_CHECK_EQ(with({"-n=10"}).text, std::string("-n=10 is above 9"));
    EO_CHECK_EQ(with({"-n=ten"}).text, std::string("-n=ten is not an integer"));
    EO_CHECK_EQ(with({}).text, std::string("-n is required"));
    EO_CHECK_EQ(with({"-n=5", "-p=2"}).text, std::string("-p=2 is above 1"));
    EO_CHECK_EQ(with({"-n=5", "-p=-1"}).text, std::string("-p=-1 is below 0"));
    EO_CHECK_EQ(with({"-n=5", "-p=half"}).text, std::string("-p=half is not a real number"));
    EO_CHECK_EQ(with({"-n=5", "-flag=maybe"}).text, std::string("-flag=maybe is not true or false"));
    EO_CHECK_EQ(with({"-n=5", "-shape=cycle"}).text,
                std::string("-shape=cycle is not one of random, sorted"));
    EO_CHECK_EQ(with({"-n=5", "-m=1"}).text,
                std::string("unknown option -m; the options are -n, -p, -flag, -shape"));
    EO_CHECK(with({"-n=5", "-flag=1"}).test == "ok\n");
    EO_CHECK(with({"-n=5", "-flag=0"}).test == "ok\n");
}

EO_TEST(an_unknown_option_is_refused_before_the_test_is_written) {
    auto const counting = [](std::vector<char const*> given) {
        return generate(given, [](eo::generator& g) {
            int const n = g.option<int>("n", 1, 1000000);
            for (int at = 0; at < n; at++) g.out.line(1000000 + at);
        });
    };
    made const small = counting({"-n=2", "-m=1"});
    EO_CHECK_EQ(small.code, 3);
    EO_CHECK_EQ(small.text, std::string("unknown option -m; the options are -n"));
    EO_CHECK_EQ(small.test, std::string(""));
    made const large = counting({"-n=300000", "-m=1"});
    EO_CHECK_EQ(large.code, 3);
    EO_CHECK_EQ(large.test, std::string(""));
    made const late = generate({"-n=300000", "-last=7"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 1000000);
        for (int at = 0; at < n; at++) g.out.line(1000000 + at);
        g.out.line(g.option<int>("last", 1, 9));
    });
    EO_CHECK(!late.stopped);
    EO_CHECK_EQ(late.test.size(), static_cast<std::size_t>(300000 * 8 + 2));
    EO_CHECK_EQ(late.test.substr(late.test.size() - 10), std::string("1299999\n7\n"));
}

EO_TEST(a_generator_that_exits_early_writes_its_test_once) {
    made const result = generate({"-n=7"}, [](eo::generator& g) {
        g.out.line(g.option<int>("n", 1, 9));
        eo::detail::finish_what_exit_left();
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.test, std::string("7\n"));
}

EO_TEST(the_command_line_has_to_look_like_options) {
    made const bare = generate({"n=5"}, [](eo::generator& g) { g.out.line("never"); });
    EO_CHECK_IN(bare.text, "n=5 is not an option; write -name=value");
    EO_CHECK_EQ(bare.test, std::string(""));
    EO_CHECK_EQ(bare.log, std::string("eolymp.h: n=5 is not an option; write -name=value\n"));
    made const dashed = generate({"-n"}, [](eo::generator& g) { g.out.line("never"); });
    EO_CHECK_IN(dashed.text, "-n is not an option");
    made const twice = generate({}, [](eo::generator& g) {
        std::string program = "gen.cpp";
        char* only[] = {program.data()};
        eo::generator second(1, only);
        (void)second.rng();
        g.out.line("never");
    });
    EO_CHECK_IN(twice.text, "this program already has a generator");
}

EO_TEST(named_streams_stay_still_while_their_neighbours_change) {
    auto const draw = [](int extra) {
        return generate({"-n=4"}, [extra](eo::generator& g) {
            int const n = g.option<int>("n", 1, 9);
            for (int at = 0; at < extra; at++) (void)g.rng("shape").uniform(1, 100);
            g.out.line(g.rng("weights").ints(n, 1, 1000));
        });
    };
    EO_CHECK_EQ(draw(0).test, draw(3).test);
    made const mixed = generate({"-n=4"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 9);
        g.out.line(g.rng().uniform(1, 9));
        g.out.line(g.rng("weights").ints(n, 1, 1000));
    });
    EO_CHECK(!mixed.stopped);
    EO_CHECK_IN(checker_warnings(), "note EO507: the default random stream is used alongside named ones");
}

EO_TEST(the_writer_lays_the_test_out) {
    made const result = generate({}, [](eo::generator& g) {
        g.out.line(3, 4);
        g.out.line(std::vector<int>{1, 2, 3});
        g.out.line();
        g.out.lines(std::vector<std::string>{"one", "two"});
        g.out.line(eo::fixed(1.0 / 3, 4));
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.test, std::string("3 4\n1 2 3\n\none\ntwo\n0.3333\n"));
}

EO_TEST(a_generator_describes_its_options) {
    made const result = generate({"--eo-describe", "-n=5"}, [](eo::generator& g) {
        (void)g.option<int>("n", 1, 200000);
        (void)g.option<int>("max", 1, 1000000000, 1000000000);
        (void)g.option<std::string>("shape", {"random", "sorted"}, "random");
        g.out.line("this is not printed");
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(result.test, "eo-describe option n an integer 1..200000\n");
    EO_CHECK_IN(result.test, "eo-describe option max an integer 1..1000000000 default=1000000000\n");
    EO_CHECK_IN(result.test, "eo-describe option shape choice random, sorted default=random\n");
    EO_CHECK(!eot::contains(result.test, "this is not printed"));
}

EO_TEST(a_generator_says_when_its_options_do_not_fit_together) {
    made const result = generate({"-t=10", "-total=5"}, [](eo::generator& g) {
        int const t = g.option<int>("t", 1, 10000);
        int const total = g.option<int>("total", 1, 200000);
        g.require(total >= t, "-total={} is smaller than -t={}", total, t);
        g.out.line(t);
    });
    EO_CHECK_EQ(result.text, std::string("-total=5 is smaller than -t=10"));
    EO_CHECK_EQ(result.code, 4);
}

EO_TEST(a_stress_run_that_draws_nothing_is_reported) {
    made const still = generate({"-n=5", "9f3a0c41d2e87b60"}, [](eo::generator& g) {
        g.out.line(g.option<int>("n", 1, 9));
    });
    EO_CHECK(!still.stopped);
    EO_CHECK_IN(checker_warnings(), "warning EO501: this stress run made no random draw");
    made const moving = generate({"-n=5", "9f3a0c41d2e87b60"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 9);
        g.out.line(g.rng().ints(n, 1, 100));
    });
    EO_CHECK(!moving.stopped);
    EO_CHECK(!eot::contains(checker_warnings(), "EO501"));
}

EO_TEST(a_value_that_prints_nothing_at_the_end_of_a_line_takes_its_space_with_it) {
    made const result = generate({}, [](eo::generator& g) {
        g.out.line("a", "");
        g.out.line(0, std::vector<int>{});
        g.out.line("a", "", "");
        g.out.line("a", "", "b");
        g.out.line("", "a");
        g.out.line("x ");
        g.out.line("");
        g.out.line("a", std::vector<std::string>{"", ""});
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.test, std::string("a\n0\na\na  b\n a\nx \n\na\n"));
    std::string const wide(std::size_t{1} << 20, 'w');
    made const flushed = generate({}, [&wide](eo::generator& g) {
        g.out.line(wide, std::vector<std::string>{"", "", ""});
    });
    EO_CHECK(!flushed.stopped);
    EO_CHECK(flushed.test == wide + "\n");
}

EO_TEST(bytes_written_around_the_writer_land_ahead_of_the_lines_it_holds) {
    made const result = generate({}, [](eo::generator& g) {
        g.out.line("first");
        std::printf("stray\n");
        g.out.line("last");
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.test, std::string("stray\nfirst\nlast\n"));
    EO_CHECK_IN(checker_warnings(), "warning EO503: 6 bytes reached stdout without going through g.out");
}

EO_TEST(writing_around_the_writer_is_reported) {
    made const result = generate({}, [](eo::generator& g) {
        std::printf("stray\n");
        std::fflush(stdout);
        g.out.line("proper");
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_EQ(result.test, std::string("stray\nproper\n"));
    EO_CHECK_IN(checker_warnings(), "warning EO503: 6 bytes reached stdout without going through g.out");
}

EO_TEST(a_test_sent_nowhere_is_not_counted_as_stray_bytes) {
    eo::detail::diagnostics::shared().forget_everything();
    forget_the_environment();
    std::string const log_path = temporary_path();
    made result;
    {
        swapped_descriptors kept;
        int const nowhere = ::open("/dev/null", O_WRONLY);
        int const grumbles = ::open(log_path.c_str(), O_WRONLY | O_TRUNC);
        ::dup2(nowhere, 1);
        ::dup2(grumbles, 2);
        std::string program = "gen.cpp";
        std::vector<char*> arguments{program.data()};
        try {
            eo::generator g(static_cast<int>(arguments.size()), arguments.data());
            g.out.line(1, 2, 3);
        } catch (eo::detail::stop const& reached) {
            result.stopped = true;
        }
        std::fflush(stdout);
        ::close(nowhere);
        ::close(grumbles);
    }
    ::unlink(log_path.c_str());
    EO_CHECK(!result.stopped);
    EO_CHECK(!eot::contains(checker_warnings(), "EO503"));
}

EO_TEST(a_test_too_big_to_store_is_reported) {
    made const result = generate({}, [](eo::generator& g) {
        std::string const wide(1024 * 1024 - 1, 'x');
        for (int at = 0; at < 65; at++) g.out.line(wide);
    });
    EO_CHECK(!result.stopped);
    EO_CHECK_IN(checker_warnings(), "warning EO502: this test is 68157440 bytes");
}

EO_TEST(a_range_wider_than_half_a_long_long_draws_the_same_values) {
    eo::rng r(1);
    long long mixed = 0;
    bool inside = true;
    for (int at = 0; at < 1000; at++) {
        long long const drawn = r.uniform(-5000000000000000000LL, 5000000000000000000LL);
        inside = inside && drawn >= -5000000000000000000LL && drawn <= 5000000000000000000LL;
        mixed ^= drawn;
    }
    EO_CHECK(inside);
    EO_CHECK_EQ(mixed, -4525108027791797183LL);
}

EO_TEST(a_real_draw_rounds_the_product_before_the_sum) {
    eo::rng r(42);
    std::uint64_t mixed = 1469598103934665603ull;
    for (int at = 0; at < 1000; at++) {
        double const drawn = r.real(-1.3, 7.7);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &drawn, sizeof(bits));
        mixed = (mixed ^ bits) * 1099511628211ull;
    }
    EO_CHECK_EQ(mixed, 14174797998470170970ull);
}

EO_TEST(the_random_stream_draws_what_a_generator_needs) {
    eo::rng r(12345);
    EO_CHECK_EQ(r.uniform(5, 5), 5LL);
    std::pair<long long, long long> const both = r.pair(1, 1000000);
    EO_CHECK(both.first != both.second);
    long long low = 1000000;
    long long high = 0;
    for (int at = 0; at < 200; at++) {
        long long const drawn = r.weighted(1, 100, 3);
        low = std::min(low, drawn);
        high = std::max(high, drawn);
    }
    EO_CHECK(low > 5);
    EO_CHECK_EQ(high, 100LL);
    for (int at = 0; at < 200; at++) EO_CHECK(r.weighted(1, 100, -3) < 96);
    std::vector<long long> const parts = r.partition(5, 100);
    EO_CHECK_EQ(parts.size(), static_cast<std::size_t>(5));
    long long sum = 0;
    for (long long const one : parts) {
        EO_CHECK(one >= 1);
        sum += one;
    }
    EO_CHECK_EQ(sum, 100LL);
    std::vector<long long> const fat = r.partition(3, 30, 8);
    long long fat_sum = 0;
    for (long long const one : fat) {
        EO_CHECK(one >= 8);
        fat_sum += one;
    }
    EO_CHECK_EQ(fat_sum, 30LL);
    std::string const word = r.letters(12, eo::charset("ab"));
    EO_CHECK_EQ(word.size(), static_cast<std::size_t>(12));
    for (char const one : word) EO_CHECK(one == 'a' || one == 'b');
    EO_CHECK(r.chance(1.0));
    EO_CHECK(!r.chance(0.0));
    std::vector<int> order = r.perm(6, 1);
    EO_CHECK_EQ(order.size(), static_cast<std::size_t>(6));
    r.shuffle(order);
    EO_CHECK(eo::is_permutation(order));
    EO_CHECK(r.real(2.0, 2.0) == 2.0);
    EO_CHECK_EQ(r.distinct(3, 1, 3).size(), static_cast<std::size_t>(3));
}

EO_TEST(a_partition_into_one_part_is_the_sum_itself) {
    eo::rng r(1);
    EO_CHECK(r.partition(1, 1) == std::vector<long long>{1});
    EO_CHECK(r.partition(1, 0, 0) == std::vector<long long>{0});
    EO_CHECK(r.partition(1, 5, 5) == std::vector<long long>{5});
    EO_CHECK(r.distinct(0, 1, 0).empty());
    eo::rng same(1);
    EO_CHECK_EQ(r.uniform(1, 1000000000), same.uniform(1, 1000000000));
}

EO_TEST(the_random_stream_refuses_what_it_cannot_draw) {
    eo::rng r(1);
    eot::outcome const backwards = eot::run_until_stop([&] { (void)r.uniform(9, 1); });
    EO_CHECK_IN(backwards.text, "uniform(9, 1) has no values in it");
    eot::outcome const crowded = eot::run_until_stop([&] { (void)r.distinct(5, 1, 3); });
    EO_CHECK_IN(crowded.text, "cannot draw 5 different values from 1..3");
    eot::outcome const empty = eot::run_until_stop([&] {
        std::vector<int> const nothing;
        (void)r.pick(nothing);
    });
    EO_CHECK_IN(empty.text, "pick needs something to pick from");
    eot::outcome const none = eot::run_until_stop([&] { (void)r.partition(0, 10); });
    EO_CHECK_IN(none.text, "a partition has at least one part, not 0");
    long long const top = std::numeric_limits<long long>::max();
    eot::outcome const halves = eot::run_until_stop([&] { (void)r.partition(3, top, top / 2); });
    EO_CHECK_IN(halves.text, "3 parts of at least 4611686018427387903 cannot add up to 9223372036854775807");
    eot::outcome const wide = eot::run_until_stop([&] { (void)r.partition(2, top, 0); });
    EO_CHECK_IN(wide.text, "partition(2, 9223372036854775807, 0) spans more values than a long long holds");
    eot::outcome const deep = eot::run_until_stop([&] { (void)r.partition(3, 10, -top); });
    EO_CHECK_IN(deep.text, "spans more values than a long long holds");
    eot::outcome const below = eot::run_until_stop([&] { (void)r.partition(2, top, -2); });
    EO_CHECK_IN(below.text, "spans more values than a long long holds");
    eot::outcome const endless = eot::run_until_stop([&] { (void)r.partition(top, top, 1); });
    EO_CHECK_EQ(endless.text,
                std::string("eolymp.h: cannot draw 9223372036854775806 values: no vector holds that many"));
    if (!eot::sanitized()) {
        eot::outcome const vast = eot::run_until_stop([&] { (void)r.partition(1LL << 56, top, 1); });
        EO_CHECK_EQ(vast.text, std::string("eolymp.h: cannot draw 72057594037927935 values: there is not enough "
                                           "memory for them"));
    }
    eot::outcome const many = eot::run_until_stop([&] { (void)r.ints(top, 1, 9); });
    EO_CHECK_IN(many.text, "cannot draw 9223372036854775807 values: no vector holds that many");
    std::vector<long long> const edge = r.partition(2, top, top / 2);
    EO_CHECK(edge.size() == 2 && edge[0] >= top / 2 && edge[1] >= top / 2 && edge[0] == top - edge[1]);
    eot::outcome const tight = eot::run_until_stop([&] { (void)r.partition(5, 10, 3); });
    EO_CHECK_IN(tight.text, "5 parts of at least 3 cannot add up to 10");
    eot::outcome const negative = eot::run_until_stop([&] { (void)r.perm(-1); });
    EO_CHECK_EQ(negative.text, std::string("eolymp.h: cannot draw -1 values"));
    eot::outcome const fewer = eot::run_until_stop([&] { (void)r.ints(-2, 1, 9); });
    EO_CHECK_EQ(fewer.text, std::string("eolymp.h: cannot draw -2 values"));
    eot::outcome const unwritten = eot::run_until_stop([&] { (void)r.letters(-3, eo::charset("a")); });
    EO_CHECK_EQ(unwritten.text, std::string("eolymp.h: cannot draw -3 letters"));
    EO_CHECK(r.perm(0).empty() && r.ints(0, 1, 9).empty() && r.letters(0, eo::charset("a")).empty());
    eot::outcome const blank = eot::run_until_stop([&] { (void)r.letters(3, eo::charset("")); });
    EO_CHECK_IN(blank.text, "holds no characters");
}

EO_TEST(a_generator_near_its_time_limit_is_reported) {
    made const quick = generate({"-n=4"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 9);
        g.out.line(g.rng().ints(n, 1, 9));
    });
    EO_CHECK(!quick.stopped);
    EO_CHECK(!eot::contains(checker_warnings(), "EO504"));

    made const slow = generate({"-n=4"}, [](eo::generator& g) {
        int const n = g.option<int>("n", 1, 9);
        g.out.line(g.rng().ints(n, 1, 9));
        eo::detail::diagnostics::shared().clock().started -= std::chrono::seconds(31);
    });
    EO_CHECK(!slow.stopped);
    EO_CHECK_IN(checker_warnings(), "warning EO504: the generator ran for ");
    EO_CHECK_IN(checker_warnings(), " ms of its 60000 ms limit");
}

EO_TEST(a_stream_draws_a_string_that_matches_a_pattern) {
    eo::rng r(2026);
    for (char const* text : {"[a-z]{1,5}", "YES|NO", "[A-Z][a-z0-9_]{0,9}", "(ab|cd){2,3}x?", "[^a-y]{3}", "a*b+",
                             "(x[0-9]{2,})*", "\\.\\ \\[", "", "()", "(|-)[1-9][0-9]{0,8}"}) {
        eo::pattern const told(text);
        for (int round = 0; round < 200; round++) {
            std::string const drawn = r.pattern(told);
            EO_CHECK(told.matches(drawn));
        }
    }
    eo::rng same(7);
    EO_CHECK_EQ(same.pattern("[a-z]{5}"), eo::rng(7).pattern(eo::pattern("[a-z]{5}")));
}

EO_TEST(a_pattern_is_drawn_uniformly_one_construct_at_a_time) {
    eo::rng r(11);
    int yes = 0;
    int lengths[4] = {0, 0, 0, 0};
    int letters[3] = {0, 0, 0};
    long long longest = 0;
    for (int round = 0; round < 30000; round++) {
        if (r.pattern("YES|NO") == "YES") yes++;
        lengths[r.pattern("x{0,3}").size()]++;
        letters[r.pattern("[abc]")[0] - 'a']++;
        longest = std::max<long long>(longest, static_cast<long long>(r.pattern("q{2,}").size()));
    }
    EO_CHECK(yes > 14500 && yes < 15500);
    for (int const one : lengths) EO_CHECK(one > 7000 && one < 8000);
    for (int const one : letters) EO_CHECK(one > 9500 && one < 10500);
    EO_CHECK_EQ(longest, 22);
    std::string seen;
    for (int round = 0; round < 3000; round++) seen += r.pattern("[^a-z]");
    for (char const one : seen) EO_CHECK(one >= '!' && one <= '~' && !(one >= 'a' && one <= 'z'));
    EO_CHECK(seen.find('{') != std::string::npos && seen.find('A') != std::string::npos);
}

EO_TEST(a_pattern_draws_the_same_string_from_the_same_seed_everywhere) {
    eo::rng r(123456789);
    std::string drawn;
    for (int round = 0; round < 4; round++) drawn += r.pattern("[A-Z][a-z]{2,6}|[0-9]{3}|(ab|cd)+") + " ";
    EO_CHECK_EQ(drawn, std::string("cdabcdabcdababcdabab abab 287 Jzkwg "));
    std::string more;
    for (int round = 0; round < 3; round++) more += r.pattern("[^a-z]{3}x?y?z?q{3,}") + " ";
    EO_CHECK_EQ(more, std::string("U3#xyzqqqqqqq }~}xqqqqq #+7xyqqqqqqqqqqqqqqqqqqqqqqq "));
}

EO_TEST(a_repeat_of_nothing_is_drawn_at_once) {
    eo::rng r(5);
    EO_CHECK_EQ(r.pattern("(){1000000000}a"), std::string("a"));
    EO_CHECK_EQ(r.pattern("((){1000000000}){1000000000}b(x{0}){4000}"), std::string("b"));
    for (int round = 0; round < 50; round++) {
        std::string const endless = r.pattern("(c+)+aa");
        EO_CHECK(endless.size() > 2 && endless.substr(endless.size() - 2) == "aa" && endless[0] == 'c');
    }
}

EO_TEST(a_class_with_nothing_visible_to_draw_is_refused) {
    int drawn_at = 0;
    eot::outcome const drawn = eot::run_until_stop([&] {
        eo::rng r(1);
        drawn_at = __LINE__ + 1;
        (void)r.pattern("a[^!-~]");
    });
    EO_CHECK_EQ(drawn.code, 3);
    EO_CHECK_EQ(drawn.text, "eolymp.h: tests/generate.inc:" + std::to_string(drawn_at) + std::string(": eo::pattern(\"a[^!-~]\") has a class written "
                                        "with ^ that leaves nothing to draw: a draw takes only the visible characters "
                                        "! to ~ that it does not exclude"));
    for (std::uint64_t seed = 0; seed < 20; seed++) {
        eot::outcome const maybe = eot::run_until_stop([&] {
            eo::rng r(seed);
            eo::pattern const told("a[^!-~]?");
            (void)r.pattern(told, eo::detail::site{"here.cpp", 9});
        });
        EO_CHECK_EQ(maybe.code, 3);
        EO_CHECK_IN(maybe.text, "eolymp.h: here.cpp:9: eo::pattern(\"a[^!-~]?\") has a class written with ^");
    }
    EO_CHECK(eo::pattern("a[^!-~]?").matches("a"));
}

EO_TEST(a_pattern_that_could_draw_too_much_is_refused_before_it_draws) {
    for (char const* huge : {"(a{1000000000}){4000}", "[a-z]{100000001}", "((((((a*)*)*)*)*)*)*",
                             "(((((((((((((((((((((((((((((((((((((((((((((((((a*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*)*"}) {
        eot::outcome const drawn = eot::run_until_stop([&] {
            eo::rng r(1);
            (void)r.pattern(huge);
        });
        EO_CHECK_EQ(drawn.code, 3);
        EO_CHECK_IN(drawn.text, "can draw more than 100000000 characters, where * and + draw at most 20 more than "
                                "their least; bound its repeats");
    }
    eo::rng r(2);
    EO_CHECK_EQ(r.pattern("[a-z]{100000000}").size(), static_cast<std::size_t>(100000000));
    EO_CHECK(r.pattern("((a*)*)*").size() <= 9261);
}
