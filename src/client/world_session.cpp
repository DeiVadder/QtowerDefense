#include "client/world_session.h"

#include "automation/report.h"
#include "client/landscape.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

QVariantList landmarksOf(const Landscape &landscape)
{
    QVariantList landmarks;
    for (const Landmark &landmark : landscape.landmarks) {
        landmarks.append(QVariantMap{{u"model"_s, landmark.model},
                                     {u"size"_s, static_cast<qreal>(landmark.size)}});
    }
    return landmarks;
}

} // namespace

WorldSession::WorldSession(QObject *parent)
    : QObject(parent), worldFile_(u":/content/world.json"_s),
      progressFile_(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                    u"/progress.json"_s)
{
    load();
}

const QString &WorldSession::worldFile() const
{
    return worldFile_;
}

void WorldSession::setWorldFile(const QString &fileName)
{
    if (worldFile_ != fileName) {
        worldFile_ = fileName;
        load();
    }
}

const QString &WorldSession::progressFile() const
{
    return progressFile_;
}

void WorldSession::setProgressFile(const QString &fileName)
{
    if (progressFile_ != fileName) {
        progressFile_ = fileName;
        load();
    }
}

const QString &WorldSession::error() const
{
    return error_;
}

void WorldSession::load()
{
    core::WorldLoadResult loaded = core::loadWorldFile(worldFile_);
    world_ = loaded.world ? *std::move(loaded.world) : core::World();
    error_ = loaded.error;
    QFile file(progressFile_);
    progress_ =
        core::parseProgress(world_, file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
    selected_ = nextSection();
    emit worldChanged();
    emit progressChanged();
    emit selectedChanged();
}

int WorldSession::nextSection() const
{
    for (std::size_t section = 0; section < world_.sections.size(); ++section) {
        if (core::isOpen(world_, progress_, section) && progress_.stars[section] == 0 &&
            !world_.sections[section].map.isEmpty()) {
            return static_cast<int>(section);
        }
    }
    return static_cast<int>(world_.start);
}

QVariantList WorldSession::sections() const
{
    QVariantList list;
    for (std::size_t index = 0; index < world_.sections.size(); ++index) {
        const core::WorldSection &section = world_.sections[index];
        const Landscape landscape = landscapeNamed(section.landscape);
        const bool open = core::isOpen(world_, progress_, index);
        list.append(QVariantMap{
            {u"id"_s, section.id},
            {u"name"_s, section.name},
            {u"landscape"_s, section.landscape},
            {u"x"_s, static_cast<qreal>(section.x) / core::kWorldSize},
            {u"y"_s, static_cast<qreal>(section.y) / core::kWorldSize},
            {u"color"_s, landscape.grass},
            {u"landmarks"_s, landmarksOf(landscape)},
            {u"open"_s, open},
            {u"stars"_s, static_cast<int>(progress_.stars[index])},
            {u"playable"_s, open && !section.map.isEmpty()},
        });
    }
    return list;
}

QVariantList WorldSession::links() const
{
    QVariantList list;
    for (std::size_t index = 0; index < world_.sections.size(); ++index) {
        for (const std::size_t neighbor : world_.sections[index].neighbors) {
            if (index < neighbor) {
                list.append(QVariantMap{{u"from"_s, static_cast<int>(index)},
                                        {u"to"_s, static_cast<int>(neighbor)}});
            }
        }
    }
    return list;
}

int WorldSession::selected() const
{
    return selected_;
}

void WorldSession::setSelected(int section)
{
    if (section >= 0 && std::cmp_less(section, world_.sections.size()) && section != selected_) {
        selected_ = section;
        emit selectedChanged();
    }
}

QString WorldSession::mapFile(int section) const
{
    if (section < 0 || std::cmp_greater_equal(section, world_.sections.size()) ||
        world_.sections[static_cast<std::size_t>(section)].map.isEmpty()) {
        return {};
    }
    return QFileInfo(worldFile_)
        .dir()
        .filePath(world_.sections[static_cast<std::size_t>(section)].map);
}

bool WorldSession::recordGame(int section, int stars)
{
    if (section < 0 || std::cmp_greater_equal(section, world_.sections.size()) || stars <= 0) {
        return false;
    }
    core::recordStars(&progress_, static_cast<std::size_t>(section), stars);
    emit progressChanged();
    setSelected(nextSection()); // the way goes on where the fog has lifted
    QDir().mkpath(QFileInfo(progressFile_).path());
    QString problem;
    if (!automation::writeFile(progressFile_, core::progressToJson(world_, progress_), &problem)) {
        qWarning("The progress could not be saved: %s", qUtf8Printable(problem));
        return false;
    }
    return true;
}

} // namespace qtd::client
