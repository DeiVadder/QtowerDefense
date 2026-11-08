#include "client/game_session.h"

#include <QFile>

using namespace Qt::StringLiterals;

namespace qtd::client {

namespace {

bool hasMesh(const QString &name)
{
    return QFile::exists(u":/assets/%1.mesh"_s.arg(name));
}

/// Walk frames count only all together, `<name>_0` and on (E-41).
bool hasWalkFrames(const QString &name)
{
    for (int frame = 0; frame < kWalkFrames; ++frame) {
        if (!hasMesh(u"%1_%2"_s.arg(name).arg(frame))) {
            return false;
        }
    }
    return true;
}

/// A model has the name of its type in the content, an empty name means there is none.
QStringList modelsOf(const auto &types)
{
    QStringList models;
    for (const auto &type : types) {
        models.append(hasMesh(type.id) || hasWalkFrames(type.id) ? type.id : QString());
    }
    return models;
}

} // namespace

QStringList GameSession::towerModels() const
{
    return game_ ? modelsOf(game_->simulation().content().towers) : QStringList();
}

QStringList GameSession::enemyModels() const
{
    return game_ ? modelsOf(game_->simulation().content().enemies) : QStringList();
}

QList<int> GameSession::enemyFrames() const
{
    QList<int> frames;
    if (game_) {
        for (const core::EnemyType &type : game_->simulation().content().enemies) {
            frames.append(hasWalkFrames(type.id) ? kWalkFrames : 0);
        }
    }
    return frames;
}

} // namespace qtd::client
