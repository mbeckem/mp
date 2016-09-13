#include "catch.hpp"

#include <iostream>

#include "mp/following_detection.hpp"
#include "mp/tools/array_2d.hpp"

using namespace mp;
using namespace std;

TEST_CASE("estimates time lag correctly", "[following-detection]")
{
    time_lag_estimation est(2);

    vector<double> data{
        -1, -2, -3, -4, -5, // following
        -2, -1,  0.4,  1,  2, // co-leading
         5,  4,  3,  2,  1, // leading
    };
    array_2d<double> matrix(data, 3, 5);

    double est1 = est.estimate_lag_simple(matrix.row(0));
    double est2 = est.estimate_lag_simple(matrix.row(1));
    double est3 = est.estimate_lag_simple(matrix.row(2));

    REQUIRE(est1 == -2.0);
    REQUIRE(est.get_following_type(est1) == following_type::following);

    REQUIRE(est2 == 0.0);
    REQUIRE(est.get_following_type(est2) == following_type::co_leading);

    REQUIRE(est3 == 2.0);
    REQUIRE(est.get_following_type(est3) == following_type::leading);
}
