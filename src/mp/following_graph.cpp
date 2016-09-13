#include "mp/following_graph.hpp"

#include <sstream>
#include <unordered_map>
#include <type_traits>

#include <boost/graph/connected_components.hpp>
#include <pugixml/pugixml.hpp>

#include "mp/following_detection.hpp"
#include "mp/page_rank.hpp"

namespace mp {

following_graph following_graph_at(const following_data &f, i64 timestamp)
{
    if (timestamp < f.begin_timestamp || timestamp > f.end_timestamp) {
        throw std::logic_error("timestamp is not in range: " + std::to_string(timestamp));
    }

    using vertex = following_graph::vertex_descriptor;
    using vertex_map = std::unordered_map<i32, vertex>;

    following_graph g;
    vertex_map vertices; // A mapping from device-id to vertex-id.
    for (size_t i = 0; i < f.devices.size(); ++i) {
        vertex_data d{f.devices[i]};
        vertex v = add_vertex(std::move(d), g);
        vertices[i] = v;
    }
    auto get_vertex = [&](i32 device_id) -> vertex {
        auto iter = vertices.find(device_id);
        if (iter != vertices.end()) {
            return iter->second;
        }
        throw std::logic_error("vertex for id does not exist: " + device_id);
    };

    // The data at "timestamp" can be thought of as
    // the list of edges for the result graph.
    auto &data = f.data_at(timestamp);
    for (auto &pair : data.co_moving) {
        auto left = get_vertex(pair.left);
        auto right = get_vertex(pair.right);

        edge_data d;
        d.weight = std::abs(pair.lag);

        switch (pair.type) {
        case following_type::following:
            add_edge(left, right, d, g);
            break;
        case following_type::leading:
            add_edge(right, left, d, g);
            break;
        case following_type::co_leading:
            add_edge(left, right, d, g);
            add_edge(right, left, d, g);
            break;
        }
    }

    return g;
}

// Takes a following graph as input
// and computes the set of connected components.
// The edges and vertices of the following_graph must not be modified
// during the lifetime of an instance of graph_components.
class graph_components
{
private:
    using undirected_graph = boost::adjacency_list<boost::setS, boost::vecS, boost::undirectedS>;

    using follow_vertex = boost::graph_traits<following_graph>::vertex_descriptor;
    using undir_vertex  = boost::graph_traits<following_graph>::vertex_descriptor;
    using vertex_map    = std::unordered_map<follow_vertex, undir_vertex>;

    // Takes a following_graph and constructs an undirected graph
    // with the same vertices and edges.
    // A mapping from following_graph vertex -> undirected_graph vertex will be stored in "mapping".
    static void make_undirected(const following_graph &g, undirected_graph &ug, vertex_map &mapping)
    {
        ug = undirected_graph();
        mapping.clear();
        mapping.reserve(num_vertices(g));

        for (auto vertex : make_iter_range(vertices(g))) {
            auto un_vertex = add_vertex(ug);
            mapping[vertex] = un_vertex;
        }
        for (auto edge : make_iter_range(edges(g))) {
            auto un_source = mapping[source(edge, g)];
            auto un_target = mapping[target(edge, g)];
            add_edge(un_source, un_target, ug);
        }
    }

public:
    graph_components(const following_graph &g)
        : ug()
        , num_components(0)
        , mapping()
        , vertex_components(num_vertices(g))
    {
        make_undirected(g, ug, mapping);
        num_components = boost::connected_components(
                    ug, boost::make_iterator_property_map(
                        vertex_components.begin(), get(boost::vertex_index, ug)));
    }

    int components() const
    {
        return num_components;
    }

    int component_of(following_graph::vertex_descriptor v) const
    {
        auto it = mapping.find(v);
        assert(it != mapping.end());

        auto uv = it->second;
        auto ui = get(boost::vertex_index, ug, uv);
        return vertex_components[ui];
    }

private:
    undirected_graph ug;
    int num_components;
    vertex_map mapping;
    vector<int> vertex_components;
};

// Stop condition for the PageRank algorithm.
// The algorithm will stop running if
// a) the absolute error (as measured by euclidean distance) is smaller
//    than "error" or
// b) the maximum number of iterations has been reached (as specified by "limit").
template<typename LastResultMap>
struct page_rank_stop
{
    page_rank_stop(double error, int limit, LastResultMap last)
        : counter(0)
        , limit(limit)
        , error(error)
        , last(last)
    {}

    int counter;
    int limit;
    double error;
    LastResultMap last;

    template<typename RankMap, typename Graph>
    bool operator()(const RankMap &result, const Graph &g)
    {
        if (++counter >= limit) {
            return true;
        }

        double err = 0.0;
        for (auto vertex : make_iter_range(vertices(g))) {
            double rank = get(result, vertex);
            double last_rank = get(last, vertex);
            double diff = rank - last_rank;

            err += diff * diff;
            put(last, vertex, rank);
        }
        return std::sqrt(err) <= error;
    }
};

vector<string> detect_leaders(const following_graph &g, bool use_weights)
{
    static constexpr double error = 0.000001;
    static constexpr double damping_factor = 0.85;
    static constexpr int    max_iterations = 500;

    // Run PageRank
    vector<double> ranks(num_vertices(g));
    auto rank_map = boost::make_iterator_property_map(
                ranks.begin(), get(boost::vertex_index, g));
    {
        vector<double> last_ranks(ranks);
        auto last_rank_map = boost::make_iterator_property_map(
                    last_ranks.begin(), get(boost::vertex_index, g));
        // All edges have the same weight for now.
        auto stop = page_rank_stop<decltype(last_rank_map)>(
                    error, max_iterations, last_rank_map);

        if (use_weights) {
            auto weight_map = get(&edge_data::weight, g);
            page_rank(g, rank_map, damping_factor, weight_map, stop);
        } else {
            auto weight_map = boost::make_static_property_map<
                    following_graph::edge_descriptor, double>(1.0);
            page_rank(g, rank_map, damping_factor, weight_map, stop);
        }
    }

    // Find leaders by taking the node with the hightest
    // PageRank value in each connected component.
    vector<string> leaders;
    graph_components c(g);
    for (int i = 0; i < c.components(); ++i) {
        double max_rank = 0;
        following_graph::vertex_descriptor max_vertex{};

        for (auto vertex : make_iter_range(vertices(g))) {
            if (c.component_of(vertex) == i) {
                double rank = get(rank_map, vertex);
                if (rank >= max_rank) {
                    max_rank = rank;
                    max_vertex = vertex;
                }
            }
        }
        // There is at least one vertex in each component,
        // thus max_vertex is valid.
        leaders.push_back(g[max_vertex].name);
    }
    return leaders;
}

vector<vector<string>> detect_groups(const following_graph &g)
{
    graph_components c(g);
    vector<vector<string>> result(c.components());

    for (auto vertex : make_iter_range(vertices(g))) {
        int group = c.component_of(vertex);
        const string &name = g[vertex].name;
        result[group].push_back(name);
    }
    return result;
}

leader_data detect_leaders(const following_data &fd, bool use_weights)
{
    leader_data ld;
    ld.begin_timestamp = fd.begin_timestamp;
    ld.end_timestamp = fd.end_timestamp;
    ld.duration = fd.duration;
    ld.devices = fd.devices;
    ld.timestamps.resize(static_cast<size_t>(ld.duration));

    for (i64 ts = ld.begin_timestamp; ts <= ld.end_timestamp; ++ts) {
        auto &data = ld.data_at(ts);
        data.timestamp = ts;
        data.leaders = detect_leaders(following_graph_at(fd, ts), use_weights);
    }
    return ld;
}

void to_graphml(const following_graph &g, std::ostream &o)
{
    using namespace pugi;

    xml_document doc;
    xml_node root = doc.append_child("graphml");
    root.append_attribute("xmlns") = "http://graphml.graphdrawing.org/xmlns";

    xml_node attr_weight = root.append_child("key");
    attr_weight.append_attribute("id") = "weight";
    attr_weight.append_attribute("for") = "edge";
    attr_weight.append_attribute("attr.name") = "weight";
    attr_weight.append_attribute("attr.type") = "double";

    xml_node graph = root.append_child("graph");
    graph.append_attribute("id") = "G";
    graph.append_attribute("edgedefault") = "directed";

    for (auto vertex : make_iter_range(vertices(g))) {
        const vertex_data &d = g[vertex];

        xml_node node = graph.append_child("node");
        node.append_attribute("id") = d.name.c_str();
    }

    for (auto edge : make_iter_range(edges(g))) {
        const vertex_data &sd = g[source(edge, g)];
        const vertex_data &td = g[target(edge, g)];

        xml_node node = graph.append_child("edge");
        node.append_attribute("source") = sd.name.c_str();
        node.append_attribute("target") = td.name.c_str();

        xml_node weight_node = node.append_child("data");
        weight_node.append_attribute("key") = "weight";
        weight_node.text() = g[edge].weight;
    }

    doc.save(o);
}

} // namespace mp
