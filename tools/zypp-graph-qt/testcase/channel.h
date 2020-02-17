#ifndef CHANNEL_H
#define CHANNEL_H

#include <QObject>

class Channel : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
  Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
  Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(unsigned priority READ priority WRITE setPriority NOTIFY priorityChanged)
  QString m_name;

  QString m_file;

QString m_type;

unsigned m_priority;

public:
  Channel( QString name, QString file, QString type, unsigned priority = 99, QObject *parent = nullptr);

  QString name() const;
  QString file() const;
  QString type() const;
  unsigned priority() const;

public slots:
  void setName(QString name);
  void setFile(QString file);
  void setType(QString type);
  void setPriority(unsigned priority);

signals:
  void nameChanged(QString name);
  void fileChanged(QString file);
  void typeChanged(QString type);
  void priorityChanged(unsigned priority);
};

#endif // CHANNEL_H
