#include "automation/build_info.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace qtd::automation {

QByteArray buildInfoJson(QStringView program, const core::Version &version)
{
    const QJsonObject info{{QStringLiteral("program"), program.toString()},
                           {QStringLiteral("version"), core::toString(version)}};
    return QJsonDocument(info).toJson(QJsonDocument::Compact);
}

} // namespace qtd::automation
