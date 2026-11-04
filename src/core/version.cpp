#include "core/version.h"

#include <QList>

#include <algorithm>

namespace qtd::core {

namespace {

std::optional<int> parseNumber(QStringView part)
{
    const bool onlyDigits = std::ranges::all_of(
        part, [](QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); });
    if (part.isEmpty() || !onlyDigits) {
        return std::nullopt;
    }
    bool ok = false;
    const int number = part.toInt(&ok);
    return ok ? std::optional(number) : std::nullopt;
}

} // namespace

Version projectVersion()
{
    return {QTD_VERSION_MAJOR, QTD_VERSION_MINOR, QTD_VERSION_PATCH};
}

QString toString(const Version &version)
{
    return QStringLiteral("%1.%2.%3")
        .arg(version.majorNumber)
        .arg(version.minorNumber)
        .arg(version.patchNumber);
}

std::optional<Version> parseVersion(QStringView text)
{
    const QList<QStringView> parts = text.split(QLatin1Char('.'));
    if (parts.size() != 3) {
        return std::nullopt;
    }
    const std::optional<int> majorNumber = parseNumber(parts[0]);
    const std::optional<int> minorNumber = parseNumber(parts[1]);
    const std::optional<int> patchNumber = parseNumber(parts[2]);
    if (!majorNumber || !minorNumber || !patchNumber) {
        return std::nullopt;
    }
    return Version{*majorNumber, *minorNumber, *patchNumber};
}

} // namespace qtd::core
