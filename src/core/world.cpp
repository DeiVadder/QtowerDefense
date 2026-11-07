#include "core/world.h"

#include "core/json_reader.h"
#include "core/simulation.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>

#include <algorithm>
#include <map>
#include <set>
#include <utility>

using namespace Qt::StringLiterals;

namespace qtd::core {

namespace {

constexpr qint64 kWorldFormatVersion = 1;

WorldLoadResult failure(QString error)
{
    return {std::nullopt, std::move(error)};
}

void readSections(ObjectReader &root, World *world, std::map<QString, std::size_t> *ids)
{
    root.forEach("sections"_L1, [world, ids](ObjectReader &item) {
        WorldSection section;
        section.id = item.text("id"_L1);
        section.name = item.text("name"_L1);
        section.landscape = item.text("landscape"_L1);
        if (item.has("map"_L1)) {
            section.map = item.text("map"_L1);
        }
        section.x = item.integer("x"_L1, 0, kWorldSize);
        section.y = item.integer("y"_L1, 0, kWorldSize);
        item.rejectUnknownKeys();
        if (!ids->emplace(section.id, world->sections.size()).second) {
            item.fail(u"id"_s, u"\"%1\" is used twice"_s.arg(section.id));
        }
        world->sections.push_back(std::move(section));
    });
}

void readLinks(ObjectReader &root, World *world, const std::map<QString, std::size_t> &ids)
{
    std::set<std::pair<std::size_t, std::size_t>> seen;
    root.forEach("links"_L1, [world, &ids, &seen](ObjectReader &item) {
        const QString from = item.text("from"_L1);
        const QString to = item.text("to"_L1);
        item.rejectUnknownKeys();
        const std::map<QString, std::size_t>::const_iterator first = ids.find(from);
        const std::map<QString, std::size_t>::const_iterator second = ids.find(to);
        if (first == ids.end() || second == ids.end()) {
            item.fail(first == ids.end() ? u"from"_s : u"to"_s, u"names no section"_s);
            return;
        }
        if (first->second == second->second) {
            item.fail(u"to"_s, u"must not be the section itself"_s);
            return;
        }
        if (!seen.insert(std::minmax(first->second, second->second)).second) {
            item.fail(u"to"_s, u"links two sections a second time"_s);
            return;
        }
        world->sections[first->second].neighbors.push_back(second->second);
        world->sections[second->second].neighbors.push_back(first->second);
    });
}

// A section that cannot be reached would stay in the fog for ever.
void rejectUnreachable(const ObjectReader &root, const World &world)
{
    std::vector<bool> reached(world.sections.size(), false);
    std::vector<std::size_t> next{world.start};
    reached[world.start] = true;
    while (!next.empty()) {
        const std::size_t section = next.back();
        next.pop_back();
        for (const std::size_t neighbor : world.sections[section].neighbors) {
            if (!reached[neighbor]) {
                reached[neighbor] = true;
                next.push_back(neighbor);
            }
        }
    }
    for (std::size_t section = 0; section < reached.size(); ++section) {
        if (!reached[section]) {
            root.fail(u"links"_s, u"no way leads to \"%1\""_s.arg(world.sections[section].id));
            return;
        }
    }
}

} // namespace

WorldLoadResult parseWorld(QByteArrayView json)
{
    QString error;
    const std::optional<QJsonObject> object = parseExactJsonObject(json, &error);
    if (!object) {
        return failure(std::move(error));
    }
    ObjectReader root(*object, QString(), &error);
    root.integer("formatVersion"_L1, kWorldFormatVersion, kWorldFormatVersion);
    World world;
    std::map<QString, std::size_t> ids;
    readSections(root, &world, &ids);
    const QString start = root.text("start"_L1);
    if (!root.failed()) {
        readLinks(root, &world, ids);
    }
    if (!root.failed()) {
        const std::map<QString, std::size_t>::const_iterator found = ids.find(start);
        if (found == ids.end()) {
            root.fail(u"start"_s, u"names no section"_s);
        } else {
            world.start = found->second;
            rejectUnreachable(root, world);
        }
    }
    root.rejectUnknownKeys();
    if (!error.isEmpty()) {
        return failure(std::move(error));
    }
    return {std::move(world), QString()};
}

WorldLoadResult loadWorldFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(u"%1: %2"_s.arg(fileName, file.errorString()));
    }
    WorldLoadResult result = parseWorld(file.readAll());
    if (!result.world) {
        result.error.prepend(fileName + u": "_s);
    }
    return result;
}

qint64 starsOfGame(const Simulation &simulation)
{
    const SimulationState &state = simulation.state();
    if (state.status != GameStatus::Won) {
        return 0;
    }
    const qint64 startLives = simulation.content().startLives;
    if (state.lives >= startLives) {
        return kMaxStars;
    }
    return 2 * state.lives >= startLives ? 2 : 1;
}

bool isOpen(const World &world, const CampaignProgress &progress, std::size_t section)
{
    if (section >= world.sections.size()) {
        return false;
    }
    const auto won = [&progress](std::size_t index) {
        return index < progress.stars.size() && progress.stars[index] > 0;
    };
    return section == world.start || won(section) ||
           std::ranges::any_of(world.sections[section].neighbors, won);
}

void recordStars(CampaignProgress *progress, std::size_t section, qint64 stars)
{
    if (progress->stars.size() <= section) {
        progress->stars.resize(section + 1, 0);
    }
    progress->stars[section] =
        std::max(progress->stars[section], std::clamp<qint64>(stars, 0, kMaxStars));
}

CampaignProgress parseProgress(const World &world, QByteArrayView json)
{
    CampaignProgress progress;
    progress.stars.assign(world.sections.size(), 0);
    const QJsonObject stars =
        QJsonDocument::fromJson(json.toByteArray()).object()["stars"_L1].toObject();
    std::map<QString, std::size_t> ids;
    for (std::size_t section = 0; section < world.sections.size(); ++section) {
        ids.emplace(world.sections[section].id, section);
    }
    for (QJsonObject::const_iterator item = stars.begin(); item != stars.end(); ++item) {
        const qint64 value = item.value().toInteger();
        if (value <= 0 || value > kMaxStars) {
            continue;
        }
        const std::map<QString, std::size_t>::const_iterator known = ids.find(item.key());
        if (known != ids.end()) {
            progress.stars[known->second] = value;
        } else {
            progress.others.insert(item.key(), value);
        }
    }
    return progress;
}

QByteArray progressToJson(const World &world, const CampaignProgress &progress)
{
    QJsonObject stars;
    for (const auto &[id, value] : progress.others.asKeyValueRange()) {
        stars[id] = value;
    }
    for (std::size_t section = 0; section < world.sections.size(); ++section) {
        if (section < progress.stars.size() && progress.stars[section] > 0) {
            stars[world.sections[section].id] = progress.stars[section];
        }
    }
    QJsonObject root;
    root["stars"_L1] = stars;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace qtd::core
