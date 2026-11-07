#include "automation/report.h"

#include "core/replay.h"

#include <QJsonDocument>
#include <QLatin1StringView>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace qtd::automation {

namespace {

constexpr qsizetype kMaxFileNameLength = 4096;

} // namespace

QByteArray compact(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject stateReport(const core::Simulation &game)
{
    const core::SimulationState &state = game.state();
    QJsonObject report{{"status"_L1, core::toString(state.status)},
                       {"tick"_L1, state.tick},
                       {"gold"_L1, state.gold},
                       {"lives"_L1, state.lives},
                       {"hash"_L1, core::hashToText(game.hash())}};
    // Only an endless game has rounds, a campaign reports as before.
    if (state.mode == core::GameMode::Endless) {
        report.insert("mode"_L1, core::toString(state.mode));
        report.insert("rounds"_L1, state.round);
    }
    return report;
}

bool writeFile(const QString &fileName, const QByteArray &data, QString *error)
{
    // QSaveFile never returns from a name with a NUL or from one of about 32760 characters, and the
    // control interface takes names from outside.
    if (fileName.isEmpty() || fileName.size() > kMaxFileNameLength ||
        fileName.contains(QChar::Null)) {
        *error = u"not a file name: \"%1\""_s.arg(fileName.left(kMaxFileNameLength));
        return false;
    }
    QSaveFile file(fileName);
    if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
        return true;
    }
    *error = u"%1: %2"_s.arg(fileName, file.errorString());
    return false;
}

} // namespace qtd::automation
