#pragma once

#include <cstdint>
#include <new>
#include <string>
#include <vector>

#include <algorithm>
#include <utility>

#include "core.h"
#include "fmt.h"
#include "io.h"
#include "pattern.h"
#include "read.h"

namespace eo {

class rng {
public:
    explicit rng(std::uint64_t seed) : state_(seed) {}

    [[nodiscard]] std::uint64_t next() {
        state_ += 0x9e3779b97f4a7c15ull;
        std::uint64_t mixed = state_;
        mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
        mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
        return mixed ^ (mixed >> 31);
    }

    [[nodiscard]] long long uniform(long long low, long long high) {
        if (low > high) detail::library_error(fmt("uniform({}, {}) has no values in it", low, high));
        std::uint64_t const span = reach(low, high);
        if (span == 0) return static_cast<long long>(next());
        return static_cast<long long>(static_cast<std::uint64_t>(low) + below(span));
    }

    [[nodiscard]] double real(double low, double high) {
        double const fraction = static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
        double const span = high - low;
        double volatile const part = fraction * span;
        return low + part;
    }

    [[nodiscard]] bool chance(double odds) { return real(0, 1) < odds; }

    template <class Container>
    [[nodiscard]] typename Container::value_type const& pick(Container const& from) {
        if (from.empty()) detail::library_error("pick needs something to pick from");
        return from[static_cast<std::size_t>(uniform(0, static_cast<long long>(from.size()) - 1))];
    }

    template <class T>
    void shuffle(std::vector<T>& values) {
        for (std::size_t at = values.size(); at > 1; at--) {
            std::size_t const other = static_cast<std::size_t>(uniform(0, static_cast<long long>(at) - 1));
            std::swap(values[at - 1], values[other]);
        }
    }

    [[nodiscard]] std::vector<int> perm(int count, int first = 0) {
        if (count < 0) detail::library_error(fmt("cannot draw {} values", count));
        std::vector<int> values(static_cast<std::size_t>(count));
        for (int at = 0; at < count; at++) values[static_cast<std::size_t>(at)] = first + at;
        shuffle(values);
        return values;
    }

    [[nodiscard]] std::vector<long long> ints(long long count, long long low, long long high) {
        if (count < 0) detail::library_error(fmt("cannot draw {} values", count));
        std::vector<long long> values = room_for(count);
        for (long long at = 0; at < count; at++) values.push_back(uniform(low, high));
        return values;
    }

    [[nodiscard]] std::vector<long long> distinct(long long count, long long low, long long high) {
        if (count < 0) detail::library_error(fmt("cannot draw {} values", count));
        if (count == 0) return {};
        std::vector<long long> values = room_for(count);
        if (low > high) detail::library_error(fmt("distinct({}, {}) has no values in it", low, high));
        std::uint64_t const span = reach(low, high);
        std::uint64_t const wanted = static_cast<std::uint64_t>(count);
        if (span != 0 && wanted > span)
            detail::library_error(fmt("cannot draw {} different values from {}..{}", count, low, high));

        if (span != 0 && span <= 4 * wanted) {
            for (std::uint64_t at = 0; at < span && values.size() < wanted; at++) {
                std::uint64_t const left = span - at;
                std::uint64_t const still = wanted - values.size();
                if (static_cast<std::uint64_t>(uniform(1, static_cast<long long>(left))) <= still)
                    values.push_back(low + static_cast<long long>(at));
            }
        } else {
            int bits = 4;
            while ((std::uint64_t{1} << bits) < 2 * wanted) bits++;
            std::size_t const mask = (std::size_t{1} << bits) - 1;
            std::vector<long long> slots(mask + 1);
            std::vector<unsigned char> taken(mask + 1, 0);
            while (values.size() < wanted) {
                long long const drawn = uniform(low, high);
                std::uint64_t const mixed = static_cast<std::uint64_t>(drawn) * 0x9e3779b97f4a7c15ull;
                std::size_t at = mixed >> (64 - bits);
                while (taken[at] != 0 && slots[at] != drawn) at = (at + 1) & mask;
                if (taken[at] != 0) continue;
                taken[at] = 1;
                slots[at] = drawn;
                values.push_back(drawn);
            }
            std::sort(values.begin(), values.end());
        }
        shuffle(values);
        return values;
    }

    [[nodiscard]] long long weighted(long long low, long long high, int lean) {
        long long best = uniform(low, high);
        int const draws = lean < 0 ? -lean : lean;
        for (int at = 0; at < draws; at++) {
            long long const other = uniform(low, high);
            if (lean > 0 ? other > best : other < best) best = other;
        }
        return best;
    }

    [[nodiscard]] std::pair<long long, long long> pair(long long low, long long high) {
        long long const first = uniform(low, high);
        long long const second = uniform(low, high);
        return {first, second};
    }

    [[nodiscard]] std::vector<long long> partition(long long count, long long sum, long long least = 1) {
        if (count < 1) detail::library_error(fmt("a partition has at least one part, not {}", count));
        long long need = 0;
        bool const huge = __builtin_mul_overflow(least, count, &need);
        if ((huge && least > 0) || (!huge && need > sum))
            detail::library_error(fmt("{} parts of at least {} cannot add up to {}", count, least, sum));
        long long high = 0;
        if (huge || __builtin_sub_overflow(sum, need, &high) || __builtin_add_overflow(high, count - 1, &high))
            detail::library_error(fmt("partition({}, {}, {}) spans more values than a long long holds", count, sum,
                                      least));
        std::vector<long long> cuts = distinct(count - 1, 1, high);
        std::sort(cuts.begin(), cuts.end());
        std::vector<long long> parts = room_for(count);
        long long last = 0;
        for (long long const one : cuts) {
            parts.push_back(one - last + least - 1);
            last = one;
        }
        parts.push_back(high - last + least);
        return parts;
    }

    [[nodiscard]] std::string letters(long long length, charset const& allowed) {
        if (length < 0) detail::library_error(fmt("cannot draw {} letters", length));
        std::vector<char> choices;
        for (int one = 0; one < 256; one++)
            if (allowed.has(static_cast<char>(one))) choices.push_back(static_cast<char>(one));
        if (choices.empty()) detail::library_error(fmt("charset(\"{}\") holds no characters", allowed.text()));
        std::string out;
        out.reserve(static_cast<std::size_t>(length));
        for (long long at = 0; at < length; at++) out.push_back(pick(choices));
        return out;
    }

    [[nodiscard]] std::string pattern(eo::pattern const& told, detail::site where = detail::site::here()) {
        if (!detail::drawable(told.tree_, told.root_))
            detail::library_error(fmt("{}: eo::pattern(\"{}\") has a class written with ^ that leaves nothing to "
                                      "draw: a draw takes only the visible characters ! to ~ that it does not exclude",
                                      detail::where_of(where), detail::escaped(told.text())));
        if (detail::longest_draw(told.tree_, told.root_) > detail::most_drawn)
            detail::library_error(fmt("{}: eo::pattern(\"{}\") can draw more than {} characters, where * and + draw "
                                      "at most {} more than their least; bound its repeats",
                                      detail::where_of(where), detail::escaped(told.text()), detail::most_drawn,
                                      detail::endless_draw));
        std::string out;
        draw(told, told.root_, out);
        return out;
    }

    [[nodiscard]] std::string pattern(detail::pattern_text told) { return pattern(eo::pattern(told), told.where()); }

private:
    void draw(eo::pattern const& told, int index, std::string& out) {
        detail::pattern_piece const& piece = told.tree_.at(index);
        if (piece.shape == detail::piece_shape::one) {
            out.push_back(pick(piece.drawable));
        } else if (piece.shape == detail::piece_shape::row) {
            for (int const part : piece.parts) draw(told, part, out);
        } else if (piece.shape == detail::piece_shape::either) {
            draw(told, pick(piece.parts), out);
        } else if (detail::longest_match(told.tree_, piece.parts[0]) != 0) {
            long long const most = piece.most == detail::unbounded ? piece.least + detail::endless_draw : piece.most;
            for (long long times = uniform(piece.least, most); times > 0; times--) draw(told, piece.parts[0], out);
        }
    }

    static std::vector<long long> room_for(long long count) {
        std::vector<long long> values;
        if (static_cast<unsigned long long>(count) > values.max_size())
            detail::library_error(fmt("cannot draw {} values: no vector holds that many", count));
        try {
            values.reserve(static_cast<std::size_t>(count));
        } catch (std::bad_alloc const&) {
            detail::library_error(fmt("cannot draw {} values: there is not enough memory for them", count));
        }
        return values;
    }

    static std::uint64_t reach(long long low, long long high) {
        return static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low) + 1;
    }

    std::uint64_t below(std::uint64_t span) {
        std::uint64_t const limit = ~std::uint64_t(0) - (~std::uint64_t(0) % span) - 1;
        std::uint64_t drawn = next();
        while (drawn > limit) drawn = next();
        return drawn % span;
    }

    std::uint64_t state_;
};

namespace detail {

inline std::uint64_t constexpr seed_start = 0xcbf29ce484222325ull;

inline std::uint64_t seed_step(std::uint64_t mixed, unsigned char one) {
    return (mixed ^ static_cast<std::uint64_t>(one)) * 0x100000001b3ull;
}

inline std::uint64_t seed_of(std::string const& bytes) {
    std::uint64_t mixed = seed_start;
    for (char const one : bytes) mixed = seed_step(mixed, static_cast<unsigned char>(one));
    return mixed;
}

inline std::uint64_t seed_of_file(char const* path) {
    source reading = source::over_file(path, true);
    std::uint64_t mixed = seed_start;
    for (int one = reading.take(); one >= 0; one = reading.take())
        mixed = seed_step(mixed, static_cast<unsigned char>(one));
    return mixed;
}

}  // namespace detail
}  // namespace eo
