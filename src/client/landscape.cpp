#include "client/landscape.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cstddef>
#include <optional>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

std::optional<SceneryZone> zoneOf(const QString &name)
{
    if (name == "surroundings"_L1) {
        return SceneryZone::Surroundings;
    }
    if (name == "border"_L1) {
        return SceneryZone::Border;
    }
    if (name == "board"_L1) {
        return SceneryZone::Board;
    }
    if (name == "water"_L1) {
        return SceneryZone::Water;
    }
    return std::nullopt;
}

std::vector<SceneryItem> sceneryOf(const QJsonArray &list)
{
    std::vector<SceneryItem> items;
    items.reserve(static_cast<std::size_t>(list.size()));
    for (const QJsonValueConstRef value : list) {
        const QJsonObject item = value.toObject();
        const std::optional<SceneryZone> zone = zoneOf(item["zone"_L1].toString());
        if (!zone) {
            qWarning("Scenery with an unknown zone: %s",
                     qUtf8Printable(item["zone"_L1].toString()));
            continue;
        }
        items.push_back({item["model"_L1].toString(),
                         *zone,
                         item["count"_L1].toInt(),
                         static_cast<float>(item["minSize"_L1].toDouble(1)),
                         static_cast<float>(item["maxSize"_L1].toDouble(1))});
    }
    return items;
}

Landscape landscapeFrom(const QJsonObject &look)
{
    Landscape landscape;
    const auto color = [&look](QLatin1StringView key, const QColor &fallback) {
        const QColor found = QColor::fromString(look[key].toString());
        return found.isValid() ? found : fallback;
    };
    landscape.grass = color("grass"_L1, landscape.grass);
    landscape.grassDark = color("grassDark"_L1, landscape.grassDark);
    landscape.earth = color("earth"_L1, landscape.earth);
    landscape.path = color("path"_L1, landscape.path);
    landscape.pathDark = color("pathDark"_L1, landscape.pathDark);
    landscape.sky = color("sky"_L1, landscape.sky);
    landscape.water = color("water"_L1, QColor());
    landscape.sceneryTint = color("sceneryTint"_L1, landscape.sceneryTint);
    landscape.waterLevel = static_cast<float>(look["waterLevel"_L1].toDouble(landscape.waterLevel));
    landscape.hillHeight = static_cast<float>(look["hillHeight"_L1].toDouble(landscape.hillHeight));
    landscape.lakes = look["lakes"_L1].toBool(landscape.lakes);
    landscape.treeModel = look["treeModel"_L1].toString(landscape.treeModel);
    landscape.rockModel = look["rockModel"_L1].toString(landscape.rockModel);
    landscape.scenery = sceneryOf(look["scenery"_L1].toArray());
    return landscape;
}

} // namespace

Landscape landscapeOfMap(const QByteArray &landscapes, const QString &mapFile)
{
    const QJsonObject root = QJsonDocument::fromJson(landscapes).object();
    const QString file = QFileInfo(mapFile).fileName();
    const QString name = root["maps"_L1].toObject()[file].toString(root["fallback"_L1].toString());
    return landscapeFrom(root["landscapes"_L1].toObject()[name].toObject());
}

Landscape landscapeOfMap(const QString &mapFile)
{
    QFile file(u":/content/landscapes.json"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("The landscapes are missing from the resources");
        return {};
    }
    return landscapeOfMap(file.readAll(), mapFile);
}

} // namespace qtd::client
