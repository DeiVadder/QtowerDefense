#pragma once

#include <QString>
#include <QStringView>

#include <compare>
#include <optional>

namespace qtd::core {

/// A semantic version, ordered by major, minor, then patch number.
struct Version
{
    int majorNumber = 0;
    int minorNumber = 0;
    int patchNumber = 0;

    auto operator<=>(const Version &) const = default;
};

/// The version of this build, as set by project() in CMakeLists.txt.
Version projectVersion();

/// Formats a version as "major.minor.patch".
QString toString(const Version &version);

/// Parses "major.minor.patch". Each part is one or more ASCII digits and fits an int; anything
/// else, such as signs, spaces or a missing part, gives std::nullopt.
std::optional<Version> parseVersion(QStringView text);

} // namespace qtd::core
