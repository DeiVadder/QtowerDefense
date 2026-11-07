#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QtTypes>

#include <cstddef>
#include <optional>
#include <vector>

namespace qtd::core {

class Simulation;

/// The most stars a map gives.
constexpr qint64 kMaxStars = 3;

/// The size of the world map in its own units, places run from 0 to this.
constexpr qint64 kWorldSize = 1000;

/// One part of the campaign world (E-32). The simulation knows nothing of it.
struct WorldSection
{
    QString id;
    QString name;
    QString landscape; ///< the look, by its name in `landscapes.json`
    QString map;       ///< the file of its map, empty while the map is not made yet
    qint64 x = 0;      ///< the place on the world map, 0 to kWorldSize
    qint64 y = 0;
    std::vector<std::size_t> neighbors; ///< in the order of the links
};

/// The sections of the campaign and the ways between them. All of them can be reached.
struct World
{
    std::vector<WorldSection> sections;
    std::size_t start = 0;
};

/// Holds the world, or an error text that names the place of the problem.
struct WorldLoadResult
{
    std::optional<World> world;
    QString error;
};

/// Reads and validates a world. Unknown keys are errors.
WorldLoadResult parseWorld(QByteArrayView json);

/// Reads a file and calls parseWorld. The error text starts with the file name.
WorldLoadResult loadWorldFile(const QString &fileName);

/// The stars of a game: none until it is won, then two for half the lives kept, three for all.
[[nodiscard]] qint64 starsOfGame(const Simulation &simulation);

/// What a player has done in the world: the best stars of each section, 0 if not won yet.
struct CampaignProgress
{
    std::vector<qint64> stars;
    QMap<QString, qint64> others; ///< sections this world does not know, kept for the player
};

/// Whether a section can be played: the start always, any other once it or a neighbor is won.
[[nodiscard]] bool
isOpen(const World &world, const CampaignProgress &progress, std::size_t section);

/// Keeps the better of the old and the new stars.
void recordStars(CampaignProgress *progress, std::size_t section, qint64 stars);

/// Reads saved progress. It belongs to a player, not to an author: a broken text gives no stars,
/// impossible numbers are left out, unknown sections are kept aside.
[[nodiscard]] CampaignProgress parseProgress(const World &world, QByteArrayView json);

/// The stars by section id, only the won ones, those of unknown sections as well.
[[nodiscard]] QByteArray progressToJson(const World &world, const CampaignProgress &progress);

} // namespace qtd::core
