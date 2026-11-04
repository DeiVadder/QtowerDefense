#include "client/app_info.h"

#include "core/version.h"

namespace qtd::client {

AppInfo::AppInfo(QObject *parent) : QObject(parent) {}

QString AppInfo::version() const
{
    return core::toString(core::projectVersion());
}

QString AppInfo::windowTitle() const
{
    return tr("QtowerDefense %1").arg(version());
}

} // namespace qtd::client
