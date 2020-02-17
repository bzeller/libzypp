#ifndef SOURCE_H
#define SOURCE_H

#include <QObject>

class Source : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
  Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
public:
  Source( const QString &url, const QString &alias, QObject *parent = nullptr );
  QString url() const;
  QString alias() const;

public slots:
  void setUrl(QString url);
  void setAlias(QString alias);

signals:
  void urlChanged(QString url);
  void aliasChanged(QString alias);

private:

  QString m_url;
  QString m_alias;
};

#endif // SOURCE_H
