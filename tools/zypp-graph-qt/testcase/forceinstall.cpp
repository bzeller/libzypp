#include "forceinstall.h"


ForceInstall::ForceInstall(const QString &sourceAlias, const QString &packageName, const QString &kindName, QObject *parent) : QObject(parent),
  m_sourceAlias( sourceAlias ),
  m_packageName( packageName ),
  m_kindName( kindName )
{ }

QString ForceInstall::sourceAlias() const
{
  return m_sourceAlias;
}

QString ForceInstall::packageName() const
{
  return m_packageName;
}

QString ForceInstall::kindName() const
{
  return m_kindName;
}

void ForceInstall::setSourceAlias(QString sourceAlias)
{
  if (m_sourceAlias == sourceAlias)
    return;

  m_sourceAlias = sourceAlias;
  emit sourceAliasChanged(m_sourceAlias);
}

void ForceInstall::setPackageName(QString packageName)
{
  if (m_packageName == packageName)
    return;

  m_packageName = packageName;
  emit packageNameChanged(m_packageName);
}

void ForceInstall::setKindName(QString kindName)
{
  if (m_kindName == kindName)
    return;

  m_kindName = kindName;
  emit kindNameChanged(m_kindName);
}
