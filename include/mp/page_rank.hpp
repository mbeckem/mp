#ifndef MP_PAGERANK_HPP
#define MP_PAGERANK_HPP

#include "defs.hpp"
#include "tools/iter.hpp"

#include <boost/property_map/property_map.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_concepts.hpp>

namespace mp {

namespace detail {

/*
 * Params:
 *  g            - The bidirectional Graph (read-only).
 *  from_rank    - The page-rank of the last step (read-only).
 *  to_rank      - The page-rank of the next iteration (write-only).
 *  damping      - The damping factor.
 *  sinks        - A Collection of sinks (vertices without outgoing edges; read-only).
 *  weight       - A mapping from edge-descriptor to edge-weight (read-only).
 *  total_weight - A mapping from vertex-descriptor to the sum of all edge-weights beginning at that vertex (read-only).
 */
template<typename Graph, typename RankMap1, typename RankMap2, typename SinkCollection, typename EdgeWeightMap, typename TotalWeightMap>
void page_rank_step(const Graph &g,
                    RankMap1 from_rank, RankMap2 to_rank, typename boost::property_traits<RankMap1>::value_type damping,
                    const SinkCollection &sinks, EdgeWeightMap weight, TotalWeightMap total_weight)
{
    using rank_type = typename boost::property_traits<RankMap1>::value_type;

    // Iterate over all vertices and their incoming edges.
    for (auto vertex : make_iter_range(vertices(g))) {
        rank_type rank(0);

        // Consider all incoming edges.
        // Sources of these edges give the current vertex a part of their
        // rank, according to the edge's weight.
        for(auto edge : make_iter_range(in_edges(vertex, g)))  {
            auto vertex_source = source(edge, g);

            // Using this coefficient instead of 1/out_degree(vertex_source)
            // to support weighted edges.
            rank_type coeff = rank_type(get(weight, edge)) / rank_type(get(total_weight, vertex_source));
            rank += get(from_rank, vertex_source) * coeff;
        }

        // Treat all "sinks" as incoming edges, too.
        for (auto sink : sinks) {
            // Sinks distribute their rank evenly.
            rank += get(from_rank, sink) / num_vertices(g);
        }

        rank = ((rank_type(1) - damping) / num_vertices(g)) + damping * rank;
        put(to_rank, vertex, rank);
    }
}

} // namespace datail

/**
  * Computes PageRank for the given Graph.
  *
  * The algorithm will run until the StopPredicate returns true.
  *
  * \param g
  *      The Graph
  * \param[out] rank
  *      A map from vertex to rank. Will store the output once the function returns.
  * \param damping
  *     The damping factor. Between 0 and 1.
  * \param weight
  *     A map from edge descriptor to edge weight. Values will be normalized by this function.
  * \param stop
  *     The predicate.
  */
// Vertex index: http://www.boost.org/doc/libs/1_56_0/libs/graph/doc/faq.html (item 5)
template<typename Graph, typename RankMap, typename EdgeWeightMap, typename StopPredicate>
void page_rank(const Graph &g,
               RankMap rank, typename boost::property_traits<RankMap>::value_type damping,
               EdgeWeightMap weight,
               StopPredicate &&stop)
{
    using graph_traits       = boost::graph_traits<Graph>;
    using edge_descriptor    = typename graph_traits::edge_descriptor;
    using vertex_descriptor  = typename graph_traits::vertex_descriptor;
    using rank_type          = typename boost::property_traits<RankMap>::value_type;
    using weight_type        = typename boost::property_traits<EdgeWeightMap>::value_type;

    BOOST_CONCEPT_ASSERT(( boost::BidirectionalGraphConcept<Graph> ));
    BOOST_CONCEPT_ASSERT(( boost::ReadWritePropertyMapConcept<RankMap, vertex_descriptor> ));
    BOOST_CONCEPT_ASSERT(( boost::ReadablePropertyMapConcept<EdgeWeightMap, edge_descriptor> ));

    // The start rank of every vertex is 1/n
    // where n is the number of vertices.
    {
        rank_type start(rank_type(1) / num_vertices(g));

        for (auto vertex : make_iter_range(vertices(g))) {
            put(rank, vertex, start);
        }
    }

    auto indices = get(boost::vertex_index, g);

    // A second rank_map to store the ranks of the previous or next iteration.
    std::vector<rank_type> aux_rank_vector(num_vertices(g));
    auto aux_rank = boost::make_iterator_property_map(
                aux_rank_vector.begin(), indices);

    // For every vertex, compute the total of its outgoing edges' weights.
    std::vector<weight_type> total_weight_vector(num_vertices(g));
    auto total_weight = boost::make_iterator_property_map(
                total_weight_vector.begin(), indices);
    {
        for (auto vertex : make_iter_range(vertices(g))) {
            weight_type total(0);

            for(auto edge : make_iter_range(out_edges(vertex, g))) {
                total += get(weight, edge);
            }

            put(total_weight, vertex, total);
        }
    }

    // Collection of "sinks" - vertices that have no outgoing edges.
    std::vector<vertex_descriptor> sinks;
    {
        for (auto vertex : make_iter_range(vertices(g))) {
            if (out_degree(vertex, g) == 0) {
                sinks.push_back(vertex);
            }
        }
    }

    bool toggle = true;
    while (1) {
        if (toggle) {
            if (stop(rank, g)) {
                break;
            }

            detail::page_rank_step(g, rank, aux_rank, damping, sinks, weight, total_weight);
        } else {
            if (stop(aux_rank, g)) {
                break;
            }

            detail::page_rank_step(g, aux_rank, rank, damping, sinks, weight, total_weight);
        }

        toggle = !toggle;
    }

    if (!toggle) {
        // Copy results from "aux_rank" to "rank".
        for (auto vertex : make_iter_range(vertices(g))) {
            put(rank, vertex, get(aux_rank, vertex));
        }
    }
}

} // namespace mp

#endif // MP_PAGERANK_HPP
