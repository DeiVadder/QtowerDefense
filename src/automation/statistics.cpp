#include "automation/statistics.h"

#include <QJsonArray>
#include <QLatin1StringView>

#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::automation {

QJsonObject gameStatistics(const core::Content &content, const core::Replay &replay)
{
    core::ReplayPlayer player(content, replay);
    QJsonArray goldCurve{player.simulation().state().gold};
    while (!player.finished()) {
        player.step(kGoldCurveTicks);
        goldCurve.append(player.simulation().state().gold);
    }
    QJsonObject spent;
    for (const core::TowerType &type : content.towers) {
        spent.insert(type.id, 0);
    }
    for (const core::Tower &tower : player.simulation().state().towers) {
        const QString &id = content.towers[tower.type].id;
        spent.insert(id, spent.value(id).toInteger() + tower.spent);
    }
    QJsonObject statistics{{"spent"_L1, spent}, {"goldCurve"_L1, goldCurve}};
    if (!player.error().isEmpty()) {
        statistics.insert("error"_L1, player.error()); // the numbers stop where the replay broke
    }
    return statistics;
}

} // namespace qtd::automation
