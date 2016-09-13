#include "catch.hpp"

#include "mp/signal_data.hpp"

using namespace mp;
using namespace std;

TEST_CASE("bad access points are recognized", "[signal-data]")
{
    signal_data sd;
    sd.bssids = {"AP_1", "AP_2", "AP_3"};
    sd.devices = {
        {
            "DEV_1",
            {
                {0, 0, -50},
                {0, 1, -60},
                {1, 1, -70},
                {1, 2, -80},
            }
        },
        {
            "DEV_2",
            {
                {0, 1, -30},
                {0, 2, -40},
                {1, 1, -40},
                {1, 0, -100},
            }
        }
    };

    // Averages for access points:
    // 0 -> (-50 + -100) / 2 = -75
    // 1 -> (-60 + -70 + -30 + -40) / 4 = 50
    // 2 -> (-80 + -40) / 2 = -60
    vector<i32> bad;

    bad = bad_access_points(sd, -74);
    REQUIRE(bad.size() == 1);
    REQUIRE(bad[0] == 0);

    bad = bad_access_points(sd, -75);
    REQUIRE(bad.empty());

    bad = bad_access_points(sd, -51);
    REQUIRE(bad.size() == 2);
    REQUIRE(bad[0] == 0);
    REQUIRE(bad[1] == 2);
}

TEST_CASE("access points can be removed", "[signal-data]")
{
    signal_data sd;
    sd.bssids = {"A", "B", "C"};
    sd.devices = {
        {
            "DEV_1",
            {
                {0, 0, -50},
                {0, 1, -60},
                {1, 1, -70},
                {1, 2, -80},
            }
        },
        {
            "DEV_2",
            {
                {0, 1, -30},
                {0, 2, -40},
                {1, 1, -40},
                {1, 0, -100},
            }
        }
    };

    using dev = signal_data::device_data;
    using mes = signal_data::measurement;

    SECTION("removing 0 access points does not alter anything") {
        signal_data copy = sd;
        remove_access_points(copy, {});
        REQUIRE(copy == sd);
    }

    SECTION("removing access point at end") {
        signal_data copy = sd;
        remove_access_points(copy, {2}); // "C"

        vector<string> expect_bssids{"A", "B"};
        REQUIRE(copy.bssids == expect_bssids);

        vector<dev> expect_devices{
            {
                "DEV_1",
                {
                    {0, 0, -50},
                    {0, 1, -60},
                    {1, 1, -70},
                }
            },
            {
                "DEV_2",
                {
                    {0, 1, -30},
                    {1, 1, -40},
                    {1, 0, -100},
                }
            }
        };
        REQUIRE(copy.devices == expect_devices);
    }

    SECTION("removing access point with reordering") {
        signal_data copy = sd;
        remove_access_points(copy, {0, 2});

        vector<string> expect_bssids{"B"};
        REQUIRE(copy.bssids == expect_bssids);

        vector<dev> expect_devices{
            {
                "DEV_1",
                {
                    {0, 0, -60},
                    {1, 0, -70},
                }
            },
            {
                "DEV_2",
                {
                    {0, 0, -30},
                    {1, 0, -40},
                }
            }
        };
        REQUIRE(copy.devices == expect_devices);
    }

    SECTION("removing all access points") {
        signal_data copy = sd;
        remove_access_points(copy, {2, 1, 0});

        REQUIRE(copy.bssids.empty());

        vector<dev> expect_devices{
            {"DEV_1", {}},
            {"DEV_2", {}},
        };
        REQUIRE(copy.devices == expect_devices);
    }
}
