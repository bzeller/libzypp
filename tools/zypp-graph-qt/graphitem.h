#ifndef GRAPHITEM_H
#define GRAPHITEM_H

#include <QQuickItem>
#include <QPair>
#include <QList>
#include <memory>


class Graph;

struct GraphItemPrivate;
class GraphItem : public QQuickItem
{
  Q_OBJECT

  Q_PROPERTY(Graph* model READ model NOTIFY modelChanged)
  Q_PROPERTY(quint32 zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
  Q_PROPERTY(QQmlComponent *nodeDelegate READ nodeDelegate WRITE setNodeDelegate NOTIFY nodeDelegateChanged)
  Q_PROPERTY(QQmlComponent *lineDelegate READ lineDelegate WRITE setLineDelegate NOTIFY lineDelegateChanged)
  Q_PROPERTY(QQuickItem* selectedItem READ selectedItem NOTIFY selectedItemChanged)

public:
    GraphItem( QQuickItem *parent = nullptr );
    virtual ~GraphItem();

    Graph * model() const;
    QQmlComponent * nodeDelegate() const;
    QQmlComponent * lineDelegate() const;

    quint32 zoom() const;

    Q_INVOKABLE void clear();

    QQuickItem* selectedItem() const;

    Q_INVOKABLE void doSelectItem(QQuickItem* selectedItem);
    Q_INVOKABLE void showDependencyGraphFor( const uint solvId, const uint levels );

signals:
    void modelChanged(Graph * model);
    void nodeDelegateChanged(QQmlComponent * nodeDelegate);
    void lineDelegateChanged(QQmlComponent * lineDelegate);
    void zoomChanged(quint32 scale);

    void selectedItemChanged(QQuickItem* selectedItem);

public slots:
    void setNodeDelegate(QQmlComponent * nodeDelegate);
    void setLineDelegate(QQmlComponent * lineDelegate);
    void setZoom( quint32 scale );
    void triggerLayout ();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void regenerate ();

private:
    Q_DECLARE_PRIVATE(GraphItem);
    std::unique_ptr<GraphItemPrivate> d_ptr;
};





#endif // GRAPHITEM_H
