#include "graphitem.h"
#include "graphmodel.h"
#include "layouts/radiallayout.h"
#include "helper/functional.h"

#include <QMap>
#include <QQmlIncubator>
#include <QQmlEngine>
#include <QQmlContext>
#include <QTimer>
#include <QQmlProperty>

#include <boost/graph/random_layout.hpp>
#include <boost/graph/random.hpp>
#include <boost/graph/fruchterman_reingold.hpp>
#include <boost/graph/kamada_kawai_spring_layout.hpp>
#include <boost/graph/copy.hpp>

struct GraphItemPrivate {
  QTimer *m_layoutTimer = nullptr;
  Graph * m_model = nullptr;
  QQmlComponent * m_nodeDelegate = nullptr;
  QQmlComponent * m_lineDelegate = nullptr;
  QQuickItem    * m_selectedItem = nullptr;

  quint32 m_zoom = 1;

  QList<QQuickItem*> m_items;
  QList<QMetaObject::Connection> m_modelConnections;

  ~GraphItemPrivate(){
    clear();
  }

  void clear () {
    q_ptr->doSelectItem( nullptr );
    std::for_each ( m_items.begin(), m_items.end(), []( auto item ){
      item->deleteLater();
    });
    m_items.clear();
  }

  void layoutStep () {
    Q_Q(GraphItem);

    qDebug() << "Begin Layout calc ";

    bool layoutDone = false;
    if ( m_selectedItem ) {

      Vertex found = boost::graph_traits< GraphData >::null_vertex();
      auto nodesUserData = boost::get( boost::vertex_userdata, *m_model );
      for_each_vertex( *m_model, [&]( const auto v ){
        if ( nodesUserData[v] == reinterpret_cast<quintptr>(m_selectedItem) ) {
          found = v;
        }
      });

      if( found != boost::graph_traits< GraphData >::null_vertex() ) {
        RadialLayout l;
        l( *m_model, found, q->size() / m_zoom, {}, true );
        layoutDone = true;
        qDebug()<<"Used radial";
      }

    }

    if ( !layoutDone ){
      QSizeF scaled = q->size() / m_zoom;
      TopologyType topology( 10, 10, scaled.width()-10, scaled.height()-10 );

      //boost::kamada_kawai_spring_layout( uGraph, boost::get( boost::vertex_pos, *m_model ), boost::get( boost::edge_weight, *m_model ), topology, boost::side_length( 1000 ) );
      boost::fruchterman_reingold_force_directed_layout(  *m_model, boost::get( boost::vertex_pos, *m_model ), topology/*, boost::cooling( boost::linear_cooling<double>(30) ) */);
    }
    qDebug() << "End Layout calc ";

    Graph::vertex_iterator v, vend, vnext;
    for (boost::tie(v, vend) = boost::vertices( *m_model ); v != vend; ++v) {
      const auto &pos = boost::get( boost::vertex_pos, *m_model)[ *v ];

      const auto qtPoint = QPointF( pos[0] * m_zoom , pos[1] * m_zoom );
      const auto obj = reinterpret_cast< QQuickItem *>( boost::get( boost::vertex_userdata, *m_model)[ *v ] );
      QQmlProperty( obj, "x" ).write( qtPoint.x() );
      QQmlProperty( obj, "y" ).write( qtPoint.y() );
    }

    //m_layoutTimer->setSingleShot( true );
    //m_layoutTimer->start( 1000 );
  }

  void reposition () {
    Graph::vertex_iterator v, vend, vnext;
    for (boost::tie(v, vend) = boost::vertices( *m_model ); v != vend; ++v) {
      const auto &pos = boost::get( boost::vertex_pos, *m_model)[ *v ];

      const auto qtPoint = QPointF( pos[0] * m_zoom , pos[1] * m_zoom );
      const auto obj = reinterpret_cast< QQuickItem *>( boost::get( boost::vertex_userdata, *m_model)[ *v ] );
      QQmlProperty( obj, "x" ).write( qtPoint.x() );
      QQmlProperty( obj, "y" ).write( qtPoint.y() );
    }

  }

private:
  Q_DECLARE_PUBLIC( GraphItem )
  GraphItem *q_ptr = nullptr;
};

GraphItem::GraphItem(QQuickItem *parent) : QQuickItem( parent ), d_ptr( std::make_unique<GraphItemPrivate>( ) )
{
  Q_D(GraphItem);
  d->q_ptr = this;

  d->m_model = new Graph( this );
  d->m_modelConnections.push_back( connect( d->m_model, &Graph::beginGraphReset, [ this ](){
    Q_D(GraphItem);
    if ( d->m_layoutTimer )
      d->m_layoutTimer->stop();
    d->clear();
  }) );

  d->m_modelConnections.push_back( connect( d->m_model, &Graph::endGraphReset, [this](){
    Q_D(GraphItem);
    TopologyType topology( 10, 10, width()-10, height()-10 );
    boost::random_graph_layout( *d->m_model, get( boost::vertex_pos, *d->m_model ), topology );
    this->GraphItem::regenerate();
  }) );
}

//keep around for std::unique_ptr to work with forward declaration
GraphItem::~GraphItem() {}

Graph *GraphItem::model() const
{
  return d_func()->m_model;
}

QQmlComponent *GraphItem::nodeDelegate() const
{
  return d_func()->m_nodeDelegate;
}

QQmlComponent *GraphItem::lineDelegate() const
{
  return d_func()->m_lineDelegate;
}

quint32 GraphItem::zoom() const
{
  return d_func()->m_zoom;
}

void GraphItem::clear()
{
  d_func()->clear();
}

QQuickItem *GraphItem::selectedItem() const
{
  return d_func()->m_selectedItem;
}

void GraphItem::showDependencyGraphFor(const uint solvId, const uint levels)
{
  Q_D(GraphItem);
  d->m_model->load( solvId, levels );

  auto solvIds = boost::get( boost::vertex_solvid, *d->m_model );
  auto userData = boost::get( boost::vertex_userdata, *d->m_model );

  typename GraphData::vertex_iterator v, vend, vnext;
  for (boost::tie(v, vend) = boost::vertices( *d->m_model ); v != vend; ++v) {
    if ( solvIds[*v] == solvId ) {
      doSelectItem( reinterpret_cast<QQuickItem *>( userData[*v] ) );
      break;
    }
  }

}

void GraphItem::setNodeDelegate(QQmlComponent *nodeDelegate)
{
  Q_D(GraphItem);
  if (d->m_nodeDelegate == nodeDelegate)
    return;

  d->m_nodeDelegate = nodeDelegate;
  emit nodeDelegateChanged(d->m_nodeDelegate);
}

void GraphItem::setLineDelegate(QQmlComponent *lineDelegate)
{
  Q_D(GraphItem);
  if ( d->m_lineDelegate == lineDelegate )
    return;

  d->m_lineDelegate = lineDelegate;
  emit lineDelegateChanged( d->m_lineDelegate );
}

void GraphItem::setZoom(quint32 scale)
{
  Q_D(GraphItem);
  if ( d->m_zoom == scale || scale < 1 )
    return;

  int oldZoom = d->m_zoom;

  d->m_zoom = scale;
  QQmlProperty( this, "width").write( ( width() / oldZoom )  * d->m_zoom );
  QQmlProperty( this, "height").write( ( height() / oldZoom ) * d->m_zoom );
  emit zoomChanged(d->m_zoom);
}

void GraphItem::doSelectItem(QQuickItem *selectedItem)
{
  Q_D(GraphItem);
  if (d->m_selectedItem == selectedItem)
    return;

  d->m_selectedItem = selectedItem;
  emit selectedItemChanged(d->m_selectedItem);
}

void GraphItem::triggerLayout()
{
  d_func()->layoutStep();
}

void GraphItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
  QQuickItem::geometryChanged( newGeometry, oldGeometry );

  Q_D(GraphItem);
  if ( !d || !d->m_model ) return;
  if ( newGeometry == oldGeometry )
    return;
  d->reposition();
}

void GraphItem::regenerate()
{
  Q_D(GraphItem);

  d->clear();

  auto context = QQmlEngine::contextForObject( this );
  qDebug()<<context;

  if ( d->m_nodeDelegate ) {
    Graph::vertex_iterator v, vend, vnext;
    for (boost::tie(v, vend) = boost::vertices( *d->m_model ); v != vend; ++v) {

      QQmlContext *cont = new QQmlContext( context );
      cont->setContextProperty( "MyGraphItem", this );
      cont->setContextProperty( "solvId", boost::get( boost::vertex_solvid, *d->m_model)[*v] );

      QObject *obj = d->m_nodeDelegate->create( cont );
      QQuickItem *nodeItem = qobject_cast<QQuickItem*>( obj );
      if ( !nodeItem ) {
        qWarning()<<"nodeDelegate must be of Type Item";
        delete obj;
        delete cont;
        break;
      }
      cont->setParent( obj ); //tie the context to the item

      const auto &pos = boost::get( boost::vertex_pos, *d->m_model)[ *v ];
      nodeItem->setPosition( QPointF( pos[0] * d->m_zoom, pos[1] * d->m_zoom ) );
      nodeItem->setParentItem( this );
      boost::put( boost::vertex_userdata, *d->m_model, *v, reinterpret_cast<quintptr>(obj));
      d->m_items.push_back( nodeItem );
    }

  }

  if ( d->m_lineDelegate ) {
    Graph::edge_iterator e, eend, enext;
    for (boost::tie(e, eend) = boost::edges( *d->m_model ); e != eend; ++e) {
      QQmlContext *cont = new QQmlContext( context );

      const auto source = reinterpret_cast< QQuickItem *>( boost::get( boost::vertex_userdata, *d->m_model)[ e->m_source ] );
      const auto target = reinterpret_cast< QQuickItem *>( boost::get( boost::vertex_userdata, *d->m_model)[ e->m_target ] );
      cont->setContextProperty( "sourceNode", source );
      cont->setContextProperty( "targetNode", target );
      cont->setContextProperty( "MyGraphItem", this );

      QObject *obj = d->m_lineDelegate->create( cont );
      cont->setParent( obj ); //tie the context to the item

      QQuickItem *edgeItem = qobject_cast<QQuickItem*>( obj );
      if ( !edgeItem ) {
        qWarning()<<"lineDelegate must be of Type Item";
        delete obj;
        delete cont;
        break;
      }
      edgeItem->setParentItem( this );
      boost::put( boost::edge_userdata, *d->m_model, *e, reinterpret_cast<quintptr>(obj));
      d->m_items.push_back( edgeItem );
    }
  }
}
