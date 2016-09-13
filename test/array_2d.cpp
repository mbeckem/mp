#include "catch.hpp"

#include <algorithm>
#include <iostream>

#include "mp/tools/array_2d.hpp"

using namespace mp;

TEST_CASE("array_2d dimensions", "[array-2d]")
{
    array_2d<int> a1;
    REQUIRE(a1.empty());
    REQUIRE(a1.cells() == 0);
    REQUIRE(a1.columns() == 0);
    REQUIRE(a1.rows() == 0);
    REQUIRE(a1.begin() == a1.end());

    array_2d<int> a2(2, 3);
    REQUIRE(!a2.empty());
    REQUIRE(a2.cells() == 6);
    REQUIRE(a2.rows() == 2);
    REQUIRE(a2.columns() == 3);
    REQUIRE(a2.cell(1, 1) == 0);
}

TEST_CASE("array_2d contents row major", "[array-2d]")
{
    vector<int> data = {
        1, 2, 3,
        4, 5, 6,
    };

    array_2d<int> array(2, 3);
    std::copy(data.begin(), data.end(), array.begin());
    REQUIRE(std::equal(array.begin(), array.end(), data.begin()));
}

TEST_CASE("array_2d row view", "[array-2d]")
{
    vector<int> data = {
        1, 2, 3,
        4, 5, 6,
    };

    array_2d<int> array(2, 3);
    std::copy(data.begin(), data.end(), array.begin());

    auto row_1 = array.row(0);
    REQUIRE(row_1.size() == 3);
    REQUIRE(std::equal(row_1.begin(), row_1.end(), data.begin()));

    auto row_2 = array.row(1);
    REQUIRE(row_2.size() == 3);
    REQUIRE(std::equal(row_2.begin(), row_2.end(), data.begin() + 3));
}

TEST_CASE("array_2d column view", "[array-2d]")
{
    vector<int> data = {
        1, 2, 3,
        4, 5, 6,
    };

    array_2d<int> array(2, 3);
    std::copy(data.begin(), data.end(), array.begin());

    auto col_1 = array.column(0);
    REQUIRE(col_1.size() == 2);
    auto col_data_1 = {1, 4};
    REQUIRE(std::equal(col_1.begin(), col_1.end(), col_data_1.begin()));
    REQUIRE(col_1.slice(1)[0] == 4);

    auto col_3 = array.column(2);
    REQUIRE(col_3.size() == 2);
    auto col_data_3 = {3, 6};
    REQUIRE(std::equal(col_3.begin(), col_3.end(), col_data_3.begin()));
}

TEST_CASE("column slicing", "[array-2d]")
{
    vector<int> data = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
    };
    auto a      = array_2d<int>(data, 3, 3);
    auto col    = a.column(2).slice(1, 2);
    auto expect = {6, 9};
    REQUIRE(col.size() == expect.size());
    REQUIRE(std::equal(col.begin(), col.end(), expect.begin()));
}
