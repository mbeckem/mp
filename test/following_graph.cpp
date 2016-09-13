#include "catch.hpp"

#include "mp/following_detection.hpp"
#include "mp/following_graph.hpp"

#include <boost/functional/hash.hpp>
#include <cereal/archives/json.hpp>

#include <unordered_set>

using namespace mp;
using namespace std;

unordered_set<string> get_name_set(const following_graph &g)
{
    unordered_set<string> names;
    for (auto vertex : make_iter_range(vertices(g))) {
        names.insert(g[vertex].name);
    }
    return names;
}

// Must use boost::hash since STL does not give a hash impl for tuple.
using edge_set_type = unordered_set<tuple<string, string>, boost::hash<tuple<string, string>>>;

edge_set_type get_edge_set(const following_graph &g)
{
    edge_set_type set;
    for (auto edge : make_iter_range(edges(g))) {
        auto s = source(edge, g);
        auto t = target(edge, g);
        set.insert(make_tuple(g[s].name, g[t].name));
    }
    return set;
}

TEST_CASE("graph construction", "[following-graph]")
{
    following_data data;
    data.devices = {"A", "B", "C"};
    data.begin_timestamp = 0;
    data.end_timestamp = 0;
    data.duration = 0;
    data.timestamps = {
        {
            // Timestamp 0
            0,
            {
                {0, 1, -5, following_type::following},
                {2, 0,  2, following_type::leading},
            }
        }
    };

    // Out of range:
    REQUIRE_THROWS_AS(following_graph_at(data, -1), std::logic_error);
    REQUIRE_THROWS_AS(following_graph_at(data,  1), std::logic_error);

    following_graph g = following_graph_at(data, 0);
    auto names = get_name_set(g);
    auto expected_names = unordered_set<string>{"A", "B", "C"};

    auto edges = get_edge_set(g);
    auto expected_edges = edge_set_type{make_tuple("A", "B"), make_tuple("A", "C")};

    REQUIRE(names == expected_names);
    REQUIRE(edges == expected_edges);
}

TEST_CASE("graph serialization", "[following-graph]")
{
    following_graph g;
    {
        auto a = add_vertex({"A"}, g);
        auto b = add_vertex({"B"}, g);
        auto c = add_vertex({"C"}, g);

        add_edge(a, b, {}, g);
        add_edge(c, b, {}, g);
        add_edge(a, c, {}, g);
    }

    std::stringstream s;

    // Serialize
    {
        cereal::JSONOutputArchive ar(s);
        ar(cereal::make_nvp("graph", g));
    }

    // Deserialize
    following_graph test;
    {
        cereal::JSONInputArchive ar(s);
        ar(cereal::make_nvp("graph", test));
    }

    REQUIRE(get_name_set(test) == get_name_set(g));
    REQUIRE(get_edge_set(test) == get_edge_set(g));
}
