#pragma once

#include "core/content.h"

#include <QByteArrayView>
#include <QString>

#include <optional>

namespace qtd::core {

/// Holds the content, or an error text that names the place of the problem.
struct ContentLoadResult
{
    std::optional<Content> content;
    QString error;
};

/// Reads and validates content JSON. Numbers in the JSON text must be plain integers; decimal
/// values are strings such as "1.25" (E-16). Unknown keys are errors.
ContentLoadResult parseContent(QByteArrayView json);

/// Reads a file and calls parseContent. The error text starts with the file name.
ContentLoadResult loadContentFile(const QString &fileName);

} // namespace qtd::core
