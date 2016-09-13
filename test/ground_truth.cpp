#include "catch.hpp"

#include "mp/ground_truth.hpp"

using namespace mp;

TEST_CASE("ground truth co-moving detection", "[ground-truth]")
{
    ground_truth g;
    g.timestamps[0] = {
        {"DEVICE_A", 1, 0},
        {"DEVICE_B", 1, 1},
    };
    g.timestamps[1] = {
        {"DEVICE_A", 1, 0},
        {"DEVICE_B", 1, 1},
        {"DEVICE_A", 2, 0},
        {"DEVICE_C", 2, 1},
    };
    g.timestamps[2] = {
        // Same as 1 but different entry order (should not matter)
        {"DEVICE_A", 2, 0},
        {"DEVICE_A", 1, 0},
        {"DEVICE_C", 2, 1},
        {"DEVICE_B", 1, 1},
    };

    REQUIRE(g.co_moving_at(0, "DEVICE_A", "DEVICE_A"));
    REQUIRE(g.co_moving_at(0, "DEVICE_A", "DEVICE_B"));

    REQUIRE(g.co_moving_at(1, "DEVICE_A", "DEVICE_C"));
    REQUIRE_FALSE(g.co_moving_at(1, "DEVICE_A", "DEVICE_B")); // second group overrides
    REQUIRE_FALSE(g.co_moving_at(1, "DEVICE_C", "DEVICE_B"));

    REQUIRE(g.co_moving_at(2, "DEVICE_A", "DEVICE_C"));
    REQUIRE_FALSE(g.co_moving_at(2, "DEVICE_A", "DEVICE_B")); // second group overrides
    REQUIRE_FALSE(g.co_moving_at(2, "DEVICE_C", "DEVICE_B"));
}
