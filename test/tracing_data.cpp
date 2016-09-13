#include "catch.hpp"

#include "mp/feature_computation.hpp"
#include "mp/parser.hpp"
#include "mp/tracing_data.hpp"

using namespace mp;

TEST_CASE("unique pairs returns correct result", "[tracing-data]")
{
    tracing_data data;
    data.devices = {
        {
            "dev0",
            {}, {},
        },
        {
            "dev1",
            {}, {},
        },
        {
            "dev2",
            {}, {},
        }
    };

    auto tup = [](i32 i, i32 j) {
        return make_tuple(i, j);
    };

    auto pairs = data.unique_pairs();
    auto expect = {tup(0, 1), tup(0, 2), tup(1, 2)};

    REQUIRE(pairs.size() == expect.size());
    REQUIRE(std::equal(expect.begin(), expect.end(), pairs.begin()));
}

TEST_CASE("transforms signal data correctly", "[tracing-data]")
{
    signal_data data{
        // Access points
        {"AP_1", "AP_2", "AP_3"},
        // Devices
        {
            {
                "DEV_1",
                {
                    // timestamp, ap_id, signal_strength
                    {1, 0, -50},
                    {1, 1, -60},
                    {1, 0, -48},
                    // No measurement for AP_3
                    // No measurement for timestamp 2
                },
            },
            {
                "DEV_2",
                {

                    {1, 2, -42},
                    {1, 1, -46},
                    {2, 2, -41},
                    {2, 1, -48},
                    // No measurement for AP_1
                }
            }
        },
    };

    tracing_data result = transform(data, -90);

    REQUIRE(result.data_dimension == 3); // 3 access points
    REQUIRE(result.min_timestamp == 1);
    REQUIRE(result.max_timestamp == 2);
    REQUIRE(result.duration == 2);
    REQUIRE(result.devices.size() == 2);

    vector<vector<double>> expect{
        // First device.
        // Row: timestamp, Column: Access Point
        {
            -(50 + 48)/2.0,   -60,    -90,    // first item: average of two values, third item: minimum signal strenght
            -(50 + 48)/2.0,   -60,    -90,    // copied because no measurement in source data
        },
        // Second device
        {
            -90, -46, -42,
            -90, -48, -41,
        }
    };

    vector<vector<char>> expect_has_data{
        // First device.
        {
            1, 1, 0,
            1, 1, 0,
        },
        {
            0, 1, 1,
            0, 1, 1,
        }
    };

    for (size_t i = 0; i < result.devices.size(); ++i) {
        auto &dev           = result.devices[i];
        auto &data          = dev.data;
        auto &expected_data = expect[i];
        auto &has           = dev.has_data;
        auto &expected_has  = expect_has_data[i];

        INFO("Device " << dev.name << " (index " << i << ")");
        REQUIRE(data.cells() == expected_data.size());
        REQUIRE(has.cells() == expected_has.size());

        for (size_t i = 0; i < data.cells(); ++i) {
            INFO("Index " << i);
            REQUIRE(data.cell(i) == expected_data[i]);
        }

        for (size_t i = 0; i < has.cells(); ++i) {
            INFO("Index " << i);
            REQUIRE(has.cell(i) == expected_has[i]);
        }
    }
}

TEST_CASE("transforms location data correctly", "[tracing-data]")
{
    location_data data{
        // Devices
        {
            {
                "DEV_1",
                {
                    {1, 10, 11, 12, 0, 0, 0, 0},
                    {2, 11,  9, 12, 0, 0, 0, 0},
                    {3, 12,  8, 11, 0, 0, 0, 0},
                },
            },
            {
                "DEV_2",
                {

                    {1, 14, 33, 12, 0, 0, 0, 0},
                    {1, 14, 28, 12, 0, 0, 0, 0},
                    // No measurement for timestamp 2
                    {3, 16, 35, 13, 0, 0, 0, 0},
                }
            }
        },
    };

    tracing_data result = transform(data);

    REQUIRE(result.data_dimension == 3); // 3 spatial dimensions
    REQUIRE(result.min_timestamp == 1);
    REQUIRE(result.max_timestamp == 3);
    REQUIRE(result.duration == 3);
    REQUIRE(result.devices.size() == 2);

    vector<vector<double>> expect{
        // First device.
        // Row: timestamp, Column: x/y/z
        {
            10, 11, 12,
            11,  9, 12,
            12,  8, 11,
        },
        // Second device
        {
            14, (33.0 + 28.0) / 2.0, 12,    // Averaged because of two measurements
            14, (33.0 + 28.0) / 2.0, 12,    // Repeated value, (timestamp 2 is missing)
            16,                  35, 13,
        }
    };

    for (size_t i = 0; i < result.devices.size(); ++i) {
        auto &dev           = result.devices[i];
        auto &data          = dev.data;
        auto &expected_data = expect[i];

        INFO("Device " << dev.name << " (index " << i << ")");
        REQUIRE(data.cells() == expected_data.size());

        for (size_t i = 0; i < data.cells(); ++i) {
            INFO("Index " << i);
            REQUIRE(data.cell(i) == expected_data[i]);
        }

        // has_data true for all entries.
        REQUIRE(std::all_of(dev.has_data.begin(), dev.has_data.end(),
                            [](char c) { return c == 1; }));
    }
}

TEST_CASE("moving average", "[tracing-data]")
{
    // Moving average over 3 data points.
    vector<double> input_1 {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    vector<double> expect_1{1.0, 1.5, 2.0, 3.0, 4.0, 5.0};

    vector<double> input_2 {10.0, 11.0, 12.0, 13.0, 14.0, 15.0};
    vector<double> expect_2{10.0, 10.5, 11.0, 12.0, 13.0, 14.0};

    tracing_data td;
    td.data_dimension = 1;
    td.min_timestamp = 1;
    td.max_timestamp = 6;
    td.duration = 6;
    td.devices.push_back({"A", {input_1, 6, 1}, array_2d<char>(6, 1, 1)});
    td.devices.push_back({"B", {input_2, 6, 1}, array_2d<char>(6, 1, 1)});
    moving_average(td, 3);

    REQUIRE(td.devices[0].data == array_2d<double>(expect_1, 6, 1));
    REQUIRE(td.devices[1].data == array_2d<double>(expect_2, 6, 1));
}
