#pragma once

#include "core/simulation.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace qtd::automation {

/// One line of JSON, the form of all reports and answers.
QByteArray compact(const QJsonObject &object);

/// What every report says about a game: status, tick, gold, lives and hash.
QJsonObject stateReport(const core::Simulation &game);

/// Writes a whole file or nothing. A failure leaves its text in error.
bool writeFile(const QString &fileName, const QByteArray &data, QString *error);

} // namespace qtd::automation
