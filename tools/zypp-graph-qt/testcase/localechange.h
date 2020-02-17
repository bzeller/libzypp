#ifndef LOCALECHANGE_H
#define LOCALECHANGE_H

#include <QObject>

class LocaleChange : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
  Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)

public:
  enum State {
    Current,
    Added,
    Removed
  };
  Q_ENUM(State);

  LocaleChange( const QString &name, State state = Current, QObject *parent = nullptr );
  QString name() const;
  State state() const;

public slots:
  void setName(QString name);
  void setState(State state);

signals:
  void nameChanged(QString name);
  void stateChanged(State state);

private:
  QString m_name;
  State m_state;
};

#endif // LOCALECHANGE_H
