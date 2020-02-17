#include "source.h"

Source::Source(const QString &url, const QString &alias, QObject *parent) : QObject(parent),
  m_url( url ),
  m_alias( alias )
{ }

QString Source::url() const
{
  return m_url;
}

QString Source::alias() const
{
  return m_alias;
}

void Source::setUrl(QString url)
{
  if (m_url == url)
    return;

  m_url = url;
  emit urlChanged(m_url);
}

void Source::setAlias(QString alias)
{
  if (m_alias == alias)
    return;

  m_alias = alias;
  emit aliasChanged(m_alias);
}
