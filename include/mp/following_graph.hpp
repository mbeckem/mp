#ifndef MP_FOLLOWING_GRAPH_HPP
#define MP_FOLLOWING_GRAPH_HPP

#include <unordered_map>

#include "defs.hpp"
#include "serialization.hpp"

#include <boost/graph/adjacency_list.hpp>

namespace mp {

struct following_data;

struct vertex_data
{
    string name; // unique device ID
};

struct edge_data
{
    double weight = 0.0;
};

/**
 * A graph of devices and following relations.
 */
using following_graph = boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::bidirectionalS,  // edges are directed and can be visited from both directions
    vertex_data,            // data stored with each vertex
    edge_data               // data stored with each edge
>;

/**
 * Returns the following graph for the given following_data at `timestamp`.
 *
 * The function adds a vertex for every device that is mentioned by the data
 * at that timestamp.
 * An edge is added between two vertices a, b if a is following b.
 * If the relation between a and b is "co-leading", both edges (a, b) and (b, a)
 * will be created.
 *
 * \param f the following data to use.
 * \param timestamp must be in range.
 */
following_graph following_graph_at(const following_data &f, i64 timestamp);

/**
 * Stores the list of leaders for every timestamp.
 */
struct leader_data
{
    struct timestamp_data
    {
        i64 timestamp = 0;
        vector<string> leaders;
    };

    /**
     * Returns the data for the given timestamp.
     */
    const timestamp_data& data_at(i64 timestamp) const
    {
        assert(timestamp >= begin_timestamp
               && timestamp <= end_timestamp
               && "Timestamp in range");
        size_t index = static_cast<size_t>(timestamp - begin_timestamp);
        return timestamps[index];
    }

    /**
     * Returns the data for the given timestamp.
     */
    timestamp_data& data_at(i64 timestamp)
    {
        assert(timestamp >= begin_timestamp
               && timestamp <= end_timestamp
               && "Timestamp in range");
        size_t index = static_cast<size_t>(timestamp - begin_timestamp);
        return timestamps[index];
    }

    i64 begin_timestamp = 0;
    i64 end_timestamp = 0;
    i64 duration = 0;
    vector<string> devices; // All devices
    vector<timestamp_data> timestamps;
};

/**
 * Detect leaders in the given graph using the PageRank algorithm.
 *
 * The computation works as follows:
 * - Run the PageRank algorithm over the entire graph
 * - Find the set of connected components in the given graph.
 *   Every such component is considered a group.
 * - For every connected component, find the node with the hightest PageRank value.
 *   This node is classified as a leader of its group.
 *
 * This function will return the list of these leaders.
 */
vector<string> detect_leaders(const following_graph &g, bool use_weights);

/**
 * Detect groups in the given graph using the connected-components algorithm.
 *
 * This function will return a vector of vectors.
 * Each inner vector stores a single group and contains the names of all
 * members.
 */
vector<vector<string>> detect_groups(const following_graph &g);

/**
 * Same as `detect_leaders(following_graph)`, but for all timestamps in `fd`.
 */
leader_data detect_leaders(const following_data &fd, bool use_weights);

/**
 *  Writes the graph to the given output stream, using the GraphML format.
 */
void to_graphml(const following_graph &g, std::ostream &o);

struct serialized_vertex
{
    string id;

    template<typename Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("id", id));
    }
};

struct serialized_edge
{
    string source;
    string target;
    double weight;

    template<typename Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("source", source),
           cereal::make_nvp("target", target),
           cereal::make_nvp("weight", weight));
    }
};

/**
 * Save the graph using the given archive.
 */
template<typename Archive>
void save(Archive &ar, const following_graph &g)
{
    vector<serialized_vertex> vertex_list;
    vector<serialized_edge> edge_list;

    for (auto v : make_iter_range(vertices(g))) {
        vertex_list.push_back({g[v].name});
    }
    for (auto e : make_iter_range(edges(g))) {
        auto s = source(e, g);
        auto t = target(e, g);
        edge_list.push_back({g[s].name, g[t].name, g[e].weight});
    }

    ar(cereal::make_nvp("vertices", vertex_list),
       cereal::make_nvp("edges", edge_list));
}

/**
 * Load the graph using the given archive.
 */
template<typename Archive>
void load(Archive &ar, following_graph &g)
{
    vector<serialized_vertex> vertex_list;
    vector<serialized_edge> edge_list;
    ar(cereal::make_nvp("vertices", vertex_list),
       cereal::make_nvp("edges", edge_list));

    using vertex_map = std::unordered_map<string, following_graph::vertex_descriptor>;

    vertex_map v; // vertex name -> vertex descriptor
    auto get_vertex = [&](const string &id) {
        auto iter = v.find(id);
        if (iter == v.end()) {
            throw std::logic_error("serialized graph misses vertex: " + id);
        }
        return iter->second;
    };

    g.clear();
    for (auto &vertex : vertex_list) {
        vertex_data d{vertex.id};
        v[vertex.id] = add_vertex(d, g);
    }
    for (auto &edge : edge_list) {
        auto s = get_vertex(edge.source);
        auto t = get_vertex(edge.target);

        edge_data d;
        d.weight = edge.weight;
        add_edge(s, t, d, g);
    }
}

/**
 * Serialize the timestamp data using the given archive.
 *
 * \relates leader_data::timestamp_data
 */
template<typename Archive>
void serialize(Archive &ar, leader_data::timestamp_data &td)
{
    ar(cereal::make_nvp("timestamp", td.timestamp),
       cereal::make_nvp("leaders", td.leaders));
}

/**
 * Serialize the leader data using the given archive.
 *
 * \relates leader_data
 */
template<typename Archive>
void serialize(Archive &ar, leader_data &ld)
{
    ar(cereal::make_nvp("begin_timestamp", ld.begin_timestamp),
       cereal::make_nvp("end_timestamp", ld.end_timestamp),
       cereal::make_nvp("duration", ld.duration),
       cereal::make_nvp("devices", ld.devices),
       cereal::make_nvp("timestamps", ld.timestamps));
}

} // namespace mp

#endif // MP_FOLLOWING_GRAPH_HPP
