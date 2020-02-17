#include "channel.h"


Channel::Channel(QString name, QString file, QString type, unsigned priority, QObject *parent) : QObject( parent ),
  m_name( std::move(name) ),
  m_file( std::move(file) ),
  m_type( std::move(type) ),
  m_priority( priority )
{ }

QString Channel::name() const
{
  return m_name;
}

QString Channel::file() const
{
  return m_file;
}

QString Channel::type() const
{
  return m_type;
}

unsigned Channel::priority() const
{
  return m_priority;
}

void Channel::setName(QString name)
{
  if (m_name == name)
    return;

  m_name = name;
  emit nameChanged(m_name);
}

void Channel::setFile(QString file)
{
  if (m_file == file)
    return;

  m_file = file;
  emit fileChanged(m_file);
}

void Channel::setType(QString type)
{
  if (m_type == type)
    return;

  m_type = type;
  emit typeChanged(m_type);
}

void Channel::setPriority(unsigned priority)
{
  if (m_priority == priority)
    return;

  m_priority = priority;
  emit priorityChanged(m_priority);
}
