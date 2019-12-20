#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include <QAbstractItemModel>
#include <memory>
#include <vector>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topology.hpp>
#include <boost/range/iterator_range.hpp>
#include <zypp/sat/Solvable.h>

namespace boost {
    enum vertex_pos_t       { vertex_pos };
    enum vertex_solvid_t    { vertex_solvid };
    enum vertex_weight_t    { vertex_weight };
    enum vertex_userdata_t  { vertex_userdata };
    enum edge_userdata_t    { edge_userdata };

    BOOST_INSTALL_PROPERTY(vertex, pos);
    BOOST_INSTALL_PROPERTY(vertex, solvid);
    BOOST_INSTALL_PROPERTY(vertex, weight);
    BOOST_INSTALL_PROPERTY(vertex, userdata);
    BOOST_INSTALL_PROPERTY(edge  , userdata);
}

using TopologyType = boost::rectangle_topology<> ;
using PointType = TopologyType::point_type;

using VertexWeightProperty     = boost::property<boost::vertex_weight_t, int>;
using VertexUserdataProperty   = boost::property<boost::vertex_userdata_t, quintptr, VertexWeightProperty>;
using SolvidProperty     = boost::property<boost::vertex_solvid_t, zypp::sat::Solvable::IdType, VertexUserdataProperty>;
using VertexPosProperty  = boost::property<boost::vertex_pos_t, PointType, SolvidProperty>;

using EdgeUserdataProperty  = boost::property<boost::edge_userdata_t, quintptr>;
using EdgeWeightProperty    = boost::property<boost::edge_weight_t, int, EdgeUserdataProperty>;

using GraphData = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, VertexPosProperty, EdgeWeightProperty>;
using UndirectedGraphData = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, VertexPosProperty, EdgeWeightProperty>;
using Vertex = boost::graph_traits<GraphData>::vertex_descriptor;
using Edge   = boost::graph_traits<GraphData>::edge_descriptor;

using VertexDescriptorRange = boost::iterator_range< boost::graph_traits<GraphData>::vertex_iterator >;
using EdgeDescriptorRange   = boost::iterator_range< boost::graph_traits<GraphData>::edge_iterator >;

class TestSetup;

class Graph : public QObject, public GraphData
{
    Q_OBJECT
public:

    Graph( QObject *parent = nullptr );
    virtual ~Graph();

    Q_INVOKABLE void load (const zypp::sat::Solvable::IdType solvId, const int levels = 10 );

    int vertexCount () const;
    int edgeCount () const;

signals:
    void vertexAdded (  Vertex  );
    void edgeAdded ( Edge );
    void aboutToRemoveVertex ( Vertex );
    void aboutToRemoveEdge ( Edge );
    void beginGraphReset ();
    void endGraphReset ();
};


#endif // GRAPHMODEL_H
