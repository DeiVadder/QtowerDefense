#include "automation/report.h"
#include "client/game_session.h"
#include "core/replay.h"
#include "core/world.h"

#include <QDir>
#include <QFileInfo>

namespace qtd::client {

bool GameSession::saveGame()
{
    if (!canSave()) {
        return false;
    }
    saveError_.clear();
    QDir().mkpath(QFileInfo(saveFile_).path());
    const bool saved = automation::writeFile(saveFile_, core::toJson(game_->replay()), &saveError_);
    saveIsOurs_ = saveIsOurs_ || saved;
    emit saveChanged();
    return saved;
}

bool GameSession::loadGame()
{
    const core::ReplayLoadResult saved = core::loadReplayFile(saveFile_);
    saveError_ = saved.error;
    if (saved.replay) {
        const QString map = QFileInfo(saved.replay->content).fileName();
        saveError_ = resumeGame(QFileInfo(fileName_).dir().filePath(map), saveFile_);
        saveIsOurs_ = saveError_.isEmpty();
    }
    emit saveChanged();
    return saveError_.isEmpty();
}

const QString &GameSession::saveFile() const
{
    return saveFile_;
}

void GameSession::setSaveFile(const QString &fileName)
{
    saveFile_ = fileName;
    emit saveChanged();
    emit stateChanged();
}

bool GameSession::canSave() const
{
    return game_ && !saveFile_.isEmpty() &&
           game_->simulation().state().status == core::GameStatus::Running;
}

bool GameSession::hasSave() const
{
    return !saveFile_.isEmpty() && QFileInfo::exists(saveFile_);
}

const QString &GameSession::saveError() const
{
    return saveError_;
}

// The stars count for the world map, which keeps them with the progress (E-32).
int GameSession::stars() const
{
    return game_ ? static_cast<int>(core::starsOfGame(game_->simulation())) : 0;
}

bool GameSession::endless() const
{
    return game_ && game_->simulation().state().mode == core::GameMode::Endless;
}

// The rounds an endless game has come through, its score (E-36).
int GameSession::round() const
{
    return game_ ? static_cast<int>(game_->simulation().state().round) : 0;
}

} // namespace qtd::client
