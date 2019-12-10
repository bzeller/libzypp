#include "graphmodel.h"

#include <QPointF>
#include <QDebug>
#include <QTimer>
#include <QHash>
#include <QSet>

#include <zypp/sat/Pool.h>
#include <zypp/sat/WhatProvides.h>
#include <zypp/PoolQuery.h>
#include <zypp/Capability.h>

Graph::Graph(QObject *parent) : QObject( parent )
{
    //boost::minstd_rand gen;
    //boost::generate_random_graph( *this, 200, 200, gen, false, false );
}

Graph::~Graph()
{

}

void Graph::load(  const zypp::sat::Solvable::IdType solvId, const int levels )
{
  emit beginGraphReset();
  this->clear();

  QHash<int, QSet<int> > solvableConnections;
  QHash<int, Vertex> solvableToVertex;

  auto v = boost::add_vertex( *this );
  boost::put( boost::vertex_userdata, *this, v, 0);
  boost::put( boost::vertex_solvid, *this, v, solvId );
  boost::put( boost::vertex_weight, *this, v, 10 );
  solvableToVertex.insert( solvId, v );

  std::function<void (const zypp::sat::Solvable &, const int )> addDepsAsNodes =
      [ this, &solvableConnections, &solvableToVertex, &addDepsAsNodes, &levels ]( const zypp::sat::Solvable &solv, const int level ){
        if ( level > levels ) return;

        auto &depList = solvableConnections.insert( solv.id(), QSet<int>() ).value();

        for ( const auto dep : zypp::sat::WhatProvides( solv.requires() ) ) {

          if ( !dep.isSystem() ) continue;
          if ( dep == zypp::sat::Solvable::noSolvable ) continue;

          depList.insert( dep.id() );

          if ( solvableToVertex.contains( dep.id() ) ) continue;

          Vertex v = boost::add_vertex( *this );
          boost::put( boost::vertex_userdata, *this, v, 0);
          boost::put( boost::vertex_solvid, *this, v, dep.id() );
          boost::put( boost::vertex_weight, *this, v, 10 );
          solvableToVertex.insert( dep.id(), v );

          addDepsAsNodes( dep, level + 1);
        }
  };

  addDepsAsNodes( zypp::sat::Solvable( solvId ), 0 );

  for ( auto iter = solvableConnections.constBegin(); iter != solvableConnections.constEnd(); iter++ ) {
    const auto &sourceV = solvableToVertex[ iter.key() ];

    for ( const auto &targetElem : iter.value() ) {
      const auto &targetV =   solvableToVertex[ targetElem ];

      auto e = boost::add_edge( sourceV, targetV, *this );
      boost::put( boost::edge_userdata, *this, e.first, 0);
      //every edge has the same weight
      boost::put( boost::edge_weight, *this, e.first, 1);
    }
  }

  qDebug()<<"Finished loading";
  emit endGraphReset();
}

int Graph::vertexCount() const
{
    return static_cast<int>( boost::num_vertices ( *this ) );
}

int Graph::edgeCount() const
{
    return static_cast<int>( boost::num_edges ( *this ) );
}
