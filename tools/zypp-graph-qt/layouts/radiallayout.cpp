// Ported from graph-tool
//
// Copyright (C) 2006-2019 Tiago de Paula Peixoto <tiago@skewed.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 3
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.



#include "radiallayout.h"
#include "helper/functional.h"

#include <QDebug>

#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/copy.hpp>

GraphData makePredecessorGraph ( GraphData g, std::vector<Vertex> predecessorMap ) {
  GraphData pGraph( boost::num_vertices(g) );
  Graph::vertex_iterator v, vend, vnext;
  for (boost::tie(v, vend) = boost::vertices( g ); v != vend; ++v) {
    if ( *v >= predecessorMap.size() ) continue;
    Vertex pred = predecessorMap[*v];

    if ( pred == GraphData::null_vertex() ) continue;
    if ( pred == *v ) continue;

    auto source = boost::vertex( pred, pGraph );
    auto target = boost::vertex( *v, pGraph );
    boost::add_edge( source, target, pGraph );
  }

  return pGraph;
}

RadialLayout::RadialLayout(QObject *parent) : QObject(parent)
{

}

void RadialLayout::operator()( GraphData &g, const size_t rootNode, const QSizeF topology, std::vector<int> order, const bool weighted, const bool order_propagate )
{
  // will contain the predecessor Vertexes , each index in the vector represents the node with the equal index in the graph
  std::vector<Vertex> p(boost::num_vertices(g), boost::graph_traits<GraphData>::null_vertex());

  //contains the distance for each node from the root node, we use that to know in which level to put the node
  std::vector<int> levels(boost::num_vertices(g), 0);

  //we need a undirected graph, otherwise the algorithm is not able to reach all nodes, since it won't go against the direction
  UndirectedGraphData uGraph;
  boost::copy_graph( g, uGraph );

  //create the shortest path from the root node to each node in the graph
  boost::dijkstra_shortest_paths( uGraph,
                                  rootNode,
                                  boost::predecessor_map( &p[0] ).distance_map( &levels[0] ));

  //build a graph that only contains edges for the minimal spanning tree, aka just contain edges that are on a shortest path
  //the node indices will be equal to the original graph
  GraphData predecessorGraph = makePredecessorGraph( g, p );

  auto weights = boost::get( boost::vertex_weight, g );
  auto tpos = boost::get( boost::vertex_pos, g );

  const size_t numVert = boost::num_vertices( predecessorGraph );

  //if the graph should be weighted summarize up the weights and propagate them throught the branches
  std::vector<int> count( numVert, 0 );
  if ( !weighted ) {
    for_each_vertex( predecessorGraph, [ & ]( const Vertex &v ){
      count[v] = weights[v];
    });
  } else {
    //find all vertices that have no outgoing edges, and put their weight into the count map
    std::deque<Vertex> q;
    for_each_vertex( predecessorGraph, [ &]( auto v ){
      if ( boost::out_degree( v, predecessorGraph) == 0 ) {
        q.push_back( v );
        count[v] = weights[v];
      }
    });

    //now go over all vectors that point at the previously collected nodes
    std::vector<bool> mark( numVert, false );
    while ( !q.empty() ) {
      auto v = q.front();
      q.pop_front();

      //now add up the weights from the predecessor nodes until we run out of nodes
      for_each_edge( boost::in_edges( v, predecessorGraph), [ & ]( auto edge ){

        auto sourceV = boost::source( edge, predecessorGraph );
        count[sourceV] += weights[v];

        //check if the sourceV was already considered, if not add it into the queue
        if ( !mark[sourceV] ) {
          mark[sourceV] = true;
          q.push_back( sourceV );
        }
      });
    }
  }

  if ( order.size() < numVert ) {
    order.resize( numVert );
  }
  std::vector<double> vorder( boost::num_vertices( predecessorGraph ), 0.0 );

  if ( order_propagate ) {
    const auto verticesIt = boost::vertices( predecessorGraph );
    std::vector<Vertex> vertices( verticesIt.first, verticesIt.second );

    //presort with the user provided node order
    std::sort(vertices.begin(), vertices.end(),
              [&] (const Vertex u, const Vertex v) { return order[u] < order[v]; });

    //build a map of the vertex index to its order
    for (size_t i = 0; i < vertices.size(); ++i)
      vorder[vertices[i]] = i;

    //now order them by level
    std::sort(vertices.begin(), vertices.end(),
              [&] (const Vertex u, const Vertex v) { return levels[u] > levels[v]; });

    //propagate the order up the branches
    for ( const auto v : vertices )
    {
      if ( boost::out_degree(v,predecessorGraph) == 0 )
        continue;
      vorder[v] = 0;
      for_each_edge( boost::out_edges( v, predecessorGraph ), [&]( auto e ){
        vorder[v] += vorder[ boost::target(e,predecessorGraph) ];
      });
      vorder[v] /= boost::out_degree(v,predecessorGraph);
    }
  }

  //now start to arrange the nodes into the layers
  std::vector<std::vector<Vertex>> layers(1);
  layers[0].push_back(rootNode);

  bool last = false;
  while (!last)
  {
    layers.push_back( {} );
    std::vector<Vertex>& new_layer = layers[layers.size() - 1];
    std::vector<Vertex>& last_layer = layers[layers.size() - 2];

    last = true;
    for (size_t i = 0; i < last_layer.size(); ++i) {
      Vertex v = last_layer[i];
      for_each_edge( boost::out_edges( v, predecessorGraph ), [&]( auto e ){
        Vertex targetVertex = boost::target(e, predecessorGraph);

        if (int(layers.size()) - 1 == int(levels[ targetVertex ]))
          last = false;

        if ( targetVertex == v ) {
          return;
        }
        new_layer.push_back( targetVertex );
      });

      if (order_propagate)
      {
        std::sort(new_layer.end() - boost::out_degree(v, predecessorGraph),
                  new_layer.end(),
                  [&] (const auto u, const auto v)
        { return vorder[u] < vorder[v]; });
      }
      else
      {
        std::sort(new_layer.end() - boost::out_degree( v, predecessorGraph ),
                  new_layer.end(),
                  [&] (auto u, auto v)
        { return order[u] < order[v]; });
      }

      if ( boost::out_degree(v, predecessorGraph) == 0 )
        new_layer.push_back(v);
    }

    if (last)
      layers.pop_back();
  }

  std::vector<double> angle( numVert );

  const double radius = qMin(topology.width() - 5 , topology.height() - 5 ) / 2 / ( layers.size() );

  double d_sum = 0;
  std::vector<Vertex>& outer_layer = layers.back();
  for (size_t i = 0; i < outer_layer.size(); ++i)
    d_sum += count[outer_layer[i]];
  angle[outer_layer[0]] = (2 * M_PI * count[outer_layer[0]]) / d_sum;
  for (size_t i = 1; i < outer_layer.size(); ++i)
    angle[outer_layer[i]] = angle[outer_layer[i-1]] + (2 * M_PI * count[outer_layer[i]]) / d_sum;
  for (size_t i = 0; i < outer_layer.size(); ++i)
    angle[outer_layer[i]] -= (2 * M_PI * count[outer_layer[i]]) / (2 * d_sum);

  for (size_t i = 0; i < layers.size(); ++i) {
    std::vector<Vertex>& vs = layers[layers.size() - 1 - i];
    for (size_t j = 0; j < vs.size(); ++j) {
      auto v = vs[j];
      d_sum = 0;
      for_each_edge( boost::out_edges( v, predecessorGraph), [&]( auto e ){
        Vertex w = target(e, predecessorGraph);
        d_sum += count[w];
      });
      for_each_edge( boost::out_edges( v, predecessorGraph), [&]( auto e ){
        Vertex w = target(e, predecessorGraph);
        angle[v] += angle[w] * count[w] / d_sum;
      });

      double d = levels[v] * radius;
      tpos[v][0] = ( d * cos(angle[v]) ) + (topology.width() / 2);
      tpos[v][1] = ( d * sin(angle[v]) ) + (topology.height() / 2);
    }
  }
}
