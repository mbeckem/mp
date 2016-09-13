#include "catch.hpp"

#include <sstream>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

#include "mp/serialization.hpp"

using namespace mp;

TEST_CASE("binary string <-> binary archive", "[serialization]")
{
    binary_string b;
    b.value.resize(4);
    b.value[0] = 3;
    b.value[1] = 2;
    b.value[2] = 1;
    b.value[3] = 0;

    string serialized;
    {
        std::ostringstream out;
        {
            cereal::BinaryOutputArchive ar(out);
            ar(b);
        }
        serialized = out.str();
    }

    binary_string deserialized;
    {
        std::istringstream in(serialized);
        cereal::BinaryInputArchive ar(in);
        ar(deserialized);
    }

    REQUIRE(deserialized.value.size() == 4);
    REQUIRE(std::equal(b.value.begin(), b.value.end(), deserialized.value.begin()));
}

TEST_CASE("binary string <-> text archive", "[serialization]")
{
    binary_string b;
    b.value.resize(4);
    b.value[0] = 3;
    b.value[1] = 2;
    b.value[2] = 1;
    b.value[3] = 0;

    string serialized;
    {
        std::ostringstream out;
        {
            cereal::JSONOutputArchive ar(out);
            ar(b);
        }
        serialized = out.str();
    }

    binary_string deserialized;
    {
        std::istringstream in(serialized);
        cereal::JSONInputArchive ar(in);
        ar(deserialized);
    }

    REQUIRE(deserialized.value.size() == 4);
    REQUIRE(std::equal(b.value.begin(), b.value.end(), deserialized.value.begin()));
}

TEST_CASE("array2d can be serialized", "[serialization]")
{
    vector<i32> data{
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 8, 7, 6
    };
    array_2d<i32> array(data, 4, 3);

    string serialized;
    {
        std::ostringstream out;
        {
            cereal::JSONOutputArchive ar(out);
            ar(array);
        }
        serialized = out.str();
    }

    array_2d<i32> deserialized;
    {
        std::istringstream in(serialized);
        cereal::JSONInputArchive ar(in);
        ar(deserialized);
    }

    REQUIRE(array.rows() == deserialized.rows());
    REQUIRE(array.columns() == deserialized.columns());
    REQUIRE(std::equal(array.begin(), array.end(), deserialized.begin()));
}
