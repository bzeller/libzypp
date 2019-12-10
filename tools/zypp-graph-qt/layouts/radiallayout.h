#ifndef RADIALLAYOUT_H
#define RADIALLAYOUT_H

#include <QObject>
#include <QSizeF>
#include "graphmodel.h"

class RadialLayout : public QObject
{
  Q_OBJECT
public:
  explicit RadialLayout(QObject *parent = nullptr);

  virtual void operator()( GraphData &g, const size_t rootNode, const QSizeF topology, std::vector<int> order = std::vector<int>(),  const bool weighted = false, const bool order_propagate = false );

signals:

};

#endif // RADIALLAYOUT_H
