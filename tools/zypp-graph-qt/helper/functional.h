#ifndef ZYPP_GRAPH_TOOL_HELPERS_FUNCTION_H_INCLUDED
#define ZYPP_GRAPH_TOOL_HELPERS_FUNCTION_H_INCLUDED

#include <boost/tuple/tuple.hpp>
#include <boost/graph/adjacency_list.hpp>

template <typename GraphType, typename CB>
void for_each_vertex( GraphType &g, const CB &cb ) {
  typename GraphType::vertex_iterator v, vend, vnext;
  for (boost::tie(v, vend) = boost::vertices( g ); v != vend; ++v) {
    cb( *v );
  }
}

template <typename GraphType, typename CB>
void for_each_edge( GraphType &g, const CB &cb ) {
  typename GraphType::edge_iterator e, eend, enext;
  for (boost::tie(e, eend) = boost::edges( g ); e != eend; ++e) {
    cb( *e );
  }
}

template <typename EdgeIter, typename CB>
void for_each_edge( std::pair<EdgeIter, EdgeIter> edgesRange, const CB &cb ) {
  EdgeIter e, eend, enext;
  for (boost::tie(e, eend) = edgesRange; e != eend; ++e) {
    cb( *e );
  }
}

#endif
