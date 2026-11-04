#pragma once

#include "core/version.h"

#include <QByteArray>
#include <QStringView>

namespace qtd::automation {

/// Machine-readable identification of a program as compact JSON, for example
/// {"program":"qtd_headless","version":"1.2.3"}.
QByteArray buildInfoJson(QStringView program, const core::Version &version);

} // namespace qtd::automation
