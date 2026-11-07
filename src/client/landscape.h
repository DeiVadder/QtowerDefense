#pragma once

#include <QByteArray>
#include <QColor>
#include <QString>
#include <QStringList>

#include <vector>

namespace qtd::client {

/// Where a kind of scenery may stand.
enum class SceneryZone : quint8 {
    Surroundings, ///< far from the board, it may be tall
    Border,       ///< around the board, low enough to hide no cell
    Board,        ///< on free cells, gone when a tower stands there
    Water,        ///< in the water around the board
};

/// One kind of scenery. Sizes are in fields, a model is one unit high.
struct SceneryItem
{
    QString model;
    SceneryZone zone = SceneryZone::Surroundings;
    int count = 0;
    float minSize = 1;
    float maxSize = 1;
};

/// A model that marks a landscape on the world map. Its size is in fields of a map.
struct Landmark
{
    QString model;
    float size = 1;
};

/// How a map looks. Only the client knows it, the simulation has no landscapes (E-34).
struct Landscape
{
    QColor grass = QColor(0x6f9a45);
    QColor grassDark = QColor(0x4f7a38);
    QColor earth = QColor(0x8a7a4c);
    QColor path = QColor(0xb39a6c);
    QColor pathDark = QColor(0x8f7650);
    QColor sky = QColor(0xb9d4dc);
    QColor water;                   ///< invalid if the landscape has no water
    QColor sceneryTint = Qt::white; ///< multiplies the colors of the plants
    float waterLevel = -0.05F;
    float hillHeight = 2; ///< in fields
    bool lakes = true;    ///< the land sinks below the water, not only on water cells
    QString treeModel = QStringLiteral("tree");
    QString rockModel = QStringLiteral("rock");
    std::vector<SceneryItem> scenery;
    std::vector<Landmark> landmarks; ///< what rises out of the fog of the world map
};

/// A landscape by its name in `landscapes.json`, the plain default for an unknown name.
[[nodiscard]] Landscape landscapeNamed(const QString &name);

/// The names of all landscapes in `landscapes.json`.
[[nodiscard]] QStringList landscapeNames();

/// The landscape of a map file, by its name in `landscapes.json`. Other maps get the fallback.
[[nodiscard]] Landscape landscapeOfMap(const QString &mapFile);

/// Reads the same from a text. A broken text gives the plain default.
[[nodiscard]] Landscape landscapeOfMap(const QByteArray &landscapes, const QString &mapFile);

} // namespace qtd::client
