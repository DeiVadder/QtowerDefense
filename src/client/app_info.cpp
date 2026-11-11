#include "client/app_info.h"

#include "core/version.h"

namespace qtd::client {

namespace {

bool &controlledFlag()
{
    static bool flag = false;
    return flag;
}

} // namespace

AppInfo::AppInfo(QObject *parent) : QObject(parent) {}

QString AppInfo::version() const
{
    return core::toString(core::projectVersion());
}

QString AppInfo::windowTitle() const
{
    return tr("QtowerDefense %1").arg(version());
}

bool AppInfo::windowed()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(Q_OS_WASM)
    return false;
#else
    return true;
#endif
}

bool AppInfo::controlled()
{
    return controlledFlag();
}

void AppInfo::setControlled(bool controlled)
{
    controlledFlag() = controlled;
}

} // namespace qtd::client
