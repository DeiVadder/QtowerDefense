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

/// The most rounds of an endless game the progress keeps, more is no score of a player.
constexpr qint64 kMostRounds = 1'000'000'000;

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

/// What a player has done in the world: the best stars of each section, 0 if not won yet, and
/// the most rounds of its endless game (E-36), 0 if none.
struct CampaignProgress
{
    std::vector<qint64> stars;
    std::vector<qint64> best;
    QMap<QString, qint64> others;    ///< stars of sections this world does not know, kept
    QMap<QString, qint64> otherBest; ///< rounds of sections this world does not know, kept
};

/// Whether a section can be played: the start always, any other once it or a neighbor is won.
[[nodiscard]] bool
isOpen(const World &world, const CampaignProgress &progress, std::size_t section);

/// Whether the endless game of a section can be played: once the section is won.
[[nodiscard]] bool isEndlessOpen(const CampaignProgress &progress, std::size_t section);

/// Keeps the better of the old and the new stars.
void recordStars(CampaignProgress *progress, std::size_t section, qint64 stars);

/// Keeps the most rounds of an endless game, at most kMostRounds.
void recordRounds(CampaignProgress *progress, std::size_t section, qint64 rounds);

/// Reads saved progress. It belongs to a player, not to an author: a broken text gives no stars,
/// impossible numbers are left out, unknown sections are kept aside.
[[nodiscard]] CampaignProgress parseProgress(const World &world, QByteArrayView json);

/// The stars and the rounds by section id, only those above 0, of unknown sections as well.
[[nodiscard]] QByteArray progressToJson(const World &world, const CampaignProgress &progress);

} // namespace qtd::core
