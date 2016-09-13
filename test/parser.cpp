#include "catch.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "mp/parser.hpp"

using namespace mp;

template<typename ContainerA, typename ContainerB, typename EqualFn>
bool container_equal(const ContainerA &a, const ContainerB &b, EqualFn fn)
{
    if (a.size() != b.size()) {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin(), fn);
}

template<typename ContainerA, typename ContainerB>
bool container_equal(const ContainerA &a, const ContainerB &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin());
}

template<typename Container>
string to_string(const Container &a)
{
    std::ostringstream out;

    auto begin = a.begin();
    auto end = a.end();
    for (auto i = begin; i != end; ++i) {
        if (i != begin) {
            out << ", ";
        }
        out << *i;
    }
    return out.str();
}

std::ostream& operator<<(std::ostream &o, const signal_data::measurement &data)
{
    o << "{"
      << data.timestamp << ","
      << data.access_point_id << ","
      << data.signal_strength
      << "}";
    return o;
}

TEST_CASE("parses signal strength lines correctly", "[parser]")
{
    string input = "123456;DEVICE_1;AP_1=-50,2400,ignore,ignore;AP_2=-60,2442,ignore,ignore\n"
                   "123457;DEVICE_1;AP_1=-50,2400,ignore,ignore;AP_3=-80,2442,ignore,ignore\n"
                   "123457;DEVICE_2;AP_1=-55,2400,ignore,ignore\n"
                   "123454;DEVICE_2;AP_1=-54,2400,ignore,ignore\n";
    std::stringstream str(input);
    auto data = parse_signal_data(str);

    REQUIRE(data.devices.size() == 2);
    REQUIRE(data.bssids.size() == 3);

    // Access points
    {
        vector<string> expect_bssids{"AP_1", "AP_2", "AP_3"};
        INFO("The actual BSSIDs are: " << to_string(data.bssids));
        INFO("Expected: " << to_string(expect_bssids));
        bool eq = container_equal(data.bssids, expect_bssids);
        REQUIRE(eq);
    }

    // Devices
    {
        vector<signal_data::device_data> expect_devices{
            {
                "DEVICE_1",
                {
                    {123456, 0, -50},
                    {123456, 1, -60},
                    {123457, 0, -50},
                    {123457, 2, -80},
                },
            },
            {
                "DEVICE_2",
                {
                    {123454, 0, -54},
                    {123457, 0, -55},
                }
            }
        };

        REQUIRE(data.devices.size() == expect_devices.size());

        for (size_t i = 0; i < data.devices.size(); ++i) {
            auto &got      = data.devices[i];
            auto &expected = expect_devices[i];

            REQUIRE(got.name == expected.name);

            INFO("Device: " << got.name);
            INFO("The actual measurements are: " << to_string(got.data));
            INFO("Expected: " << to_string(expected.data));

            using measurement = signal_data::measurement;
            bool eq = container_equal(got.data, expected.data,
                                      [](const measurement &a, const measurement &b) {
                return a.timestamp == b.timestamp
                        && a.access_point_id == b.access_point_id
                        && a.signal_strength == b.signal_strength;
            });
            REQUIRE(eq);
        }
    }
}

TEST_CASE("parses location files", "[parser]")
{
    // [('ts','int'),('user','str'),('lat','float'),('long','float'),('alt','float'),
    //  ('uncertainty','float'),('speed','float'),('heading','float'),('vspeed','float')]
    string input =  "100;DEVICE_A;1;4;7;1;2;3;4\n"
                    "101;DEVICE_B;2;5;8;2;3;4;5\n"
                    "100;DEVICE_B;3;6;9;6;7;8;9\n";
    std::stringstream str(input);
    auto data = parse_location_data(str);

    location_data expected{
        {
            // First Device
            {
                "DEVICE_A",
                {
                    {
                        100,
                        1, 4, 7,
                        1, 2, 3, 4
                    }
                }
            },
            // Second device
            {
                "DEVICE_B",
                {
                    // First measurement (note time order)
                    {
                        100,
                        3, 6, 9,
                        6, 7, 8, 9
                    },
                    // Second measurement
                    {
                        101,
                        2, 5, 8,
                        2, 3, 4, 5
                    }
                }
            }
        }
    };

    REQUIRE(data.devices.size() == expected.devices.size());
    for (size_t i = 0; i < data.devices.size(); ++i) {
        auto &got_dev = data.devices[i];
        auto &expected_dev = expected.devices[i];

        REQUIRE(got_dev.name == expected_dev.name);
        REQUIRE(got_dev.data.size() == expected_dev.data.size());

        INFO("Device name is " << got_dev.name);
        for (size_t j = 0; j < got_dev.data.size(); ++j) {
            INFO("Measurement index is " << j);
            auto &g = got_dev.data[j];
            auto &e = expected_dev.data[j];

            REQUIRE(g.timestamp == e.timestamp);
            REQUIRE(g.lat == e.lat);
            REQUIRE(g.lng == e.lng);
            REQUIRE(g.alt == e.alt);
            REQUIRE(g.uncertainty == e.uncertainty);
            REQUIRE(g.speed == e.speed);
            REQUIRE(g.heading == e.heading);
            REQUIRE(g.vspeed == e.vspeed);
        }
    }
}

TEST_CASE("parses ground truth files", "[parser]")
{
    using device = ground_truth::device;

    string input = "Follower 3 TIME_1 TIME_2 1 4 DEV_A DEV_B DEV_C\n"
                   "# Comments are ignored\n"
                   "Follower 3 TIME_1 TIME_2 2 5 DEV_D DEV_E,DEV_F\n";
    std::stringstream str(input);
    ground_truth g = parse_ground_truth_data(str);

    auto equal = [](const device &a, const device &b) {
        return a.name == b.name && a.group == b.group && a.order == b.order;
    };

    vector<device> a{
        {"DEV_A", 0, 0},
        {"DEV_B", 0, 1},
        {"DEV_C", 0, 2},
    };

    vector<device> b{
        {"DEV_D", 1, 0},
        {"DEV_E", 1, 1},
        {"DEV_F", 1, 1},
    };

    vector<device> both;
    std::copy(a.begin(), a.end(), std::back_inserter(both));
    std::copy(b.begin(), b.end(), std::back_inserter(both));

    REQUIRE_FALSE(g.timestamps.empty());
    REQUIRE(g.timestamps.begin()->first == 1);
    REQUIRE(g.timestamps.rbegin()->first == 5);

    REQUIRE(container_equal(g.timestamps[1], a, equal));
    REQUIRE(container_equal(g.timestamps[2], both, equal));
    REQUIRE(container_equal(g.timestamps[3], both, equal));
    REQUIRE(container_equal(g.timestamps[4], both, equal));
    REQUIRE(container_equal(g.timestamps[5], b, equal));
}

TEST_CASE("parses game experiment signal data files", "[parser]")
{
    string input1 = "1331133709724;DEV_1;AP_1=-80,2412,,;AP_2=-81,2462,,;\n"
                    "1331133711510;DEV_1;AP_1=-68,2412,,;AP_3=-76,2462,,;\n"
                    "1331133714004;DEV_1;AP_1=-70,2412,,;AP_3=-76,2462,,;\n";
    string input2 = "1331133630203;DEV_2;AP_4=-47,2412,,;AP_5=-67,2437,,;\n";

    std::istringstream in1(input1);
    std::istringstream in2(input2);

    game_signal_data_parser parser;
    parser.parse("DEV_1", in1);
    parser.parse("DEV_2", in2);

    signal_data data = parser.take();

    // Access points
    {
        vector<string> expected_bssids{"AP_1", "AP_2", "AP_3", "AP_4", "AP_5"};
        INFO("The actual BSSIDs are: " << to_string(data.bssids));
        INFO("Expected: " << to_string(expected_bssids));
        REQUIRE(container_equal(data.bssids, expected_bssids));
    }

    // Devices
    vector<signal_data::device_data> expected_devices{
        {
            "DEV_1",
            {
                {1331133709, 0, -80},
                {1331133709, 1, -81},
                {1331133711, 0, -68},
                {1331133711, 2, -76},
                {1331133714, 0, -70},
                {1331133714, 2, -76},
            },
        },
        {
            "DEV_2",
            {
                {1331133630, 3, -47},
                {1331133630, 4, -67},
            }
        }
    };

    REQUIRE(data.devices.size() == expected_devices.size());
    for (size_t i = 0; i < data.devices.size(); ++i) {
        auto &got      = data.devices[i];
        auto &expected = expected_devices[i];

        REQUIRE(got.name == expected.name);

        INFO("Device: " << got.name);
        INFO("The actual measurements are: " << to_string(got.data));
        INFO("Expected: " << to_string(expected.data));

        using measurement = signal_data::measurement;
        bool eq = container_equal(got.data, expected.data,
                                  [](const measurement &a, const measurement &b) {
            return a.timestamp == b.timestamp
                    && a.access_point_id == b.access_point_id
                    && a.signal_strength == b.signal_strength;
        });
        REQUIRE(eq);
    }
}

// Returns true if va and vb contain the same elements,
// ignoring order and duplicates.
bool set_equals(const vector<ground_truth::device> &va,
                const vector<ground_truth::device> &vb)
{
    using dev = ground_truth::device;

    struct less
    {
        bool operator()(const dev &a, const dev &b) const
        {
            return std::tie(a.name, a.group, a.order)
                    < std::tie(b.name, b.group, b.order);
        }
    };

    std::set<dev, less> sa(va.begin(), va.end());
    std::set<dev, less> sb(vb.begin(), vb.end());

    if (sa.size() != sb.size()) {
        return false;
    }

    return std::equal(sa.begin(), sa.end(), sb.begin(),
                      [](const dev &a, const dev &b) {
        return a.name == b.name
                && a.group == b.group
                && a.order == b.order;
    });
}

TEST_CASE("parses game ground truth files", "[parser]")
{
    string input1 = "1000;1\n"
                    "5000;2\n"
                    "6000;-1\n";
    string input2 = "2000;2\n"
                    "3000;-1\n"
                    "4000;1\n";
    std::istringstream in1(input1), in2(input2);

    std::unordered_map<string, i32> evaders{
        {"EV_1", 1}, {"EV_2", 2},
    };

    game_ground_truth_parser parser(evaders, 1, 6);
    parser.parse("DEV_1", in1);
    parser.parse("DEV_2", in2);

    ground_truth gt = parser.take();

    std::map<i64, vector<ground_truth::device>> expected;
    expected[1] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 1, 1},
        {"DEV_2", 4, 0},
    };
    expected[2] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 1, 1},
        {"DEV_2", 2, 1},
    };
    expected[3] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 1, 1},
        {"DEV_2", 4, 0},
    };
    expected[4] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 1, 1},
        {"DEV_2", 1, 1},
    };
    expected[5] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 2, 1},
        {"DEV_2", 1, 1},
    };
    expected[6] = {
        {"EV_1", 1, 0},
        {"EV_2", 2, 0},
        {"DEV_1", 3, 0},
        {"DEV_2", 1, 1},
    };

    for (i64 ts = 1; ts <= 6; ++ts) {
        INFO("timestamp " << ts);
        REQUIRE(set_equals(expected[ts], gt.timestamps[ts]));
    }
}
