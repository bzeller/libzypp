#ifndef ZYPPPOOLMODEL_H
#define ZYPPPOOLMODEL_H

#include <QAbstractListModel>
#include <zypp/sat/Pool.h>
#include <memory>

class TestSetup;

class ZyppPoolModel : public QAbstractListModel
{
  Q_OBJECT

public:
  ZyppPoolModel( QObject *parent = nullptr );
  virtual ~ZyppPoolModel();

  Q_INVOKABLE void load (const QUrl &path );

public:

  enum Roles {
    SolvIdRole = Qt::UserRole,
    RepositoryRole
  };

  // QAbstractItemModel interface
  int rowCount(const QModelIndex &parent = QModelIndex() ) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole ) const override;
  QHash<int, QByteArray> roleNames() const override;

signals:
  void failedToLoadSystem ( const QString &reason );

private:
  QVector<zypp::sat::Solvable::IdType> _allSolvables;
  std::unique_ptr<TestSetup> _testSetup;
};

#endif // ZYPPPOOLMODEL_H
