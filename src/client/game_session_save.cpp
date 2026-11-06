#include "automation/report.h"
#include "client/game_session.h"
#include "core/replay.h"

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

} // namespace qtd::client
