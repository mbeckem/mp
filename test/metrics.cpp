#include "catch.hpp"

#include <iostream>

#include "mp/metrics.hpp"

using namespace mp;

string to_string(const vector<double> &v)
{
    std::stringstream s;

    s << "(";
    auto b = v.begin();
    auto e = v.end();
    for (auto i = b; i != e; ++i) {
        if (i != b) {
            s << ", ";
        }
        s << *i;
    }
    s << ")";

    return s.str();
}

string to_string(const array_2d<double> &a) {
    std::stringstream s;

    for (size_t r = 0; r < a.rows(); ++r) {
        auto row = a.row(r);
        auto b = row.begin();
        auto e = row.end();
        for (auto i = b; i != e; ++i) {
            if (i != b) {
                s << " ";
            }
            s << *i;
        }
        s << "\n";
    }

    return s.str();
}

string to_string(const vector<tuple<size_t, size_t>> &p)
{
    std::stringstream s;

    s << "{";
    auto b = p.begin();
    auto e = p.end();
    for (auto i = b; i != e; ++i) {
        if (i != b) {
            s << ", ";
        }
        s << "("
          << get<0>(*i) << ","
          << get<1>(*i)
          << ")";
    }
    s << "}";

    return s.str();
}

TEST_CASE("dtw cost", "[dtw]")
{
    struct {
        vector<double> a;
        vector<double> b;
        double expected;
    } tests[]{
        {{0, 1, 2}, {0, 1, 2}, 0.0},
        {{0, 1, 1, 2}, {0, 1, 2}, 0.0},
        {{0, 1, 2, 2}, {0, 1, 1, 2}, 0.0},

        {{0, 1, 2}, {0, 2, 2}, 1.0},
        {{0, 2}, {0, 1, 2, 3, 4}, 4.0},
        {{0, 1, 2, 3, 4}, {0, 2}, 4.0},
        {{3, 0, 1}, {2, 4, 2, 0, 1}, 3.0},
    };

    for (auto &test : tests) {
        dtw d(test.a.size(), test.b.size());
        double cost = d.run(test.a, test.b, manhattan_distance_1);

        INFO("for vector a = " << to_string(test.a));
        INFO("for vector b = " << to_string(test.b));
        INFO("and cost matrix\n" << to_string(d.cost_matrix()));
        REQUIRE(cost == test.expected);
    }
}

TEST_CASE("dtw warp path", "[dtw]")
{
    auto t = [](size_t i, size_t j) { return make_tuple(i, j); };

    struct {
        vector<double> a;
        vector<double> b;
        vector<tuple<size_t, size_t>> expected;
    } tests[]{
        {
            {0, 1, 2}, {0, 1, 2},
            {t(0, 0), t(1, 1), t(2, 2)},
        },
        {
            {0, 1, 1, 2}, {0, 1, 2},
            {t(0, 0), t(1, 1), t(2, 1), t(3, 2)},
        },
        {
            {0, 1, 2, 2}, {0, 1, 1, 2},
            {t(0, 0), t(1, 1), t(1, 2), t(2, 3), t(3, 3)},
        },
        {
            {0, 1, 2}, {0, 2, 2},
            {t(0, 0), t(1, 1), t(2, 2)},
        },
        {
            {0, 2}, {0, 1, 2, 3, 4},
            {t(0, 0), t(0, 1), t(1, 2), t(1, 3), t(1, 4)},
        },
        {
            {0, 1, 2, 3, 4}, {0, 2},
            {t(0, 0), t(1, 0), t(2, 1), t(3, 1), t(4, 1)},
        },
        {
            {3, 0, 1}, {2, 4, 2, 0, 1},
            {t(0, 0), t(0, 1), t(0, 2), t(1, 3), t(2, 4)},
        },
    };

    for (auto &test : tests) {
        dtw d(test.a.size(), test.b.size());
        d.run(test.a, test.b, manhattan_distance_1);
        auto path = d.warp_path();

        INFO("for vector a = " << to_string(test.a));
        INFO("for vector b = " << to_string(test.b));
        INFO("and cost matrix\n" << to_string(d.cost_matrix()));
        INFO("expected = " << to_string(test.expected));
        INFO("path = " << to_string(path));
        REQUIRE(path == test.expected);
    }
}
