#include "zypppoolmodel.h"

#include <QDebug>
#include <QUrl>

#define INCLUDE_TESTSETUP_WITHOUT_BOOST
#include "zypp/../tests/lib/TestSetup.h"
#undef  INCLUDE_TESTSETUP_WITHOUT_BOOST



ZyppPoolModel::ZyppPoolModel( QObject *parent ) : QAbstractListModel( parent )
{ }

ZyppPoolModel::~ZyppPoolModel()
{ }

void ZyppPoolModel::load( const QUrl &path )
{
  qDebug()<< "Loading " << path;
  beginResetModel();
  _allSolvables.clear();
  _testSetup.reset();

  _testSetup = std::make_unique<TestSetup>();

  bool loaded = false;
  try {
    _testSetup->LoadSystemAt( path.path().toStdString() );
    loaded = true;
  } catch ( const zypp::Exception &e) {
    qWarning() << "Failed to load system: " << e.asUserString().c_str();
    emit failedToLoadSystem( QString::fromStdString( e.asUserString() ) );
  }

  if ( loaded ) {
    const auto pool = zypp::sat::Pool::instance();
    std::for_each( pool.solvablesBegin(), pool.solvablesEnd(), [this]( const auto &solvable ){
      _allSolvables.push_back( solvable.id() );
    });
  }

  endResetModel();
}


int ZyppPoolModel::rowCount( const QModelIndex &index ) const
{
  if ( index.parent().isValid() ) return 0;
  return _allSolvables.size();
}

QVariant ZyppPoolModel::data(const QModelIndex &index, int role) const
{
  if( index.parent().isValid() || index.row() >= rowCount( ) ) return QVariant();

  sat::Solvable solv( _allSolvables[index.row()]);

  switch ( role ) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        return QString::fromStdString( solv.name() );
      break;
    case Roles::SolvIdRole:
          return solv.id();
    case Roles::RepositoryRole:
          return QString::fromStdString( solv.repoInfo().name() );
    }
  }
  return QVariant();
}

QHash<int, QByteArray> ZyppPoolModel::roleNames() const
{
  auto roles = QAbstractListModel::roleNames();
  roles.insert( Roles::SolvIdRole, "solvId" );
  roles.insert( Roles::RepositoryRole, "repository" );

  qDebug()<<"Roles " << roles;

  return roles;
}

QModelIndex ZyppPoolModel::solvIdToIndex( const uint solvId ) const
{
  int row = _allSolvables.indexOf( solvId );
  if ( row == -1 )
    return QModelIndex();
  return index( row, 0 );
}
