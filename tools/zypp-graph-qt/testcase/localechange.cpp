#include "localechange.h"


LocaleChange::LocaleChange(const QString &name, LocaleChange::State state, QObject *parent) : QObject(parent),
  m_name( name ),
  m_state( state )
{ }

QString LocaleChange::name() const
{
  return m_name;
}

LocaleChange::State LocaleChange::state() const
{
  return m_state;
}

void LocaleChange::setName(QString name)
{
  if (m_name == name)
    return;

  m_name = name;
  emit nameChanged(m_name);
}

void LocaleChange::setState(LocaleChange::State state)
{
  if (m_state == state)
    return;

  m_state = state;
  emit stateChanged(m_state);
}
