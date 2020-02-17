#ifndef FORCEINSTALL_H
#define FORCEINSTALL_H

#include <QObject>

class ForceInstall : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString sourceAlias READ sourceAlias WRITE setSourceAlias NOTIFY sourceAliasChanged)
  Q_PROPERTY(QString packageName READ packageName WRITE setPackageName NOTIFY packageNameChanged)
  Q_PROPERTY(QString kindName READ kindName WRITE setKindName NOTIFY kindNameChanged)
public:
  ForceInstall( const QString &sourceAlias, const QString &packageName, const QString &kindName, QObject *parent = nullptr );
  QString sourceAlias() const;
  QString packageName() const;
  QString kindName() const;

public slots:
  void setSourceAlias(QString sourceAlias);
  void setPackageName(QString packageName);
  void setKindName(QString kindName);

signals:
  void sourceAliasChanged(QString sourceAlias);
  void packageNameChanged(QString packageName);
  void kindNameChanged(QString kindName);

private:
  QString m_sourceAlias;
  QString m_packageName;
  QString m_kindName;
};

#endif // FORCEINSTALL_H
