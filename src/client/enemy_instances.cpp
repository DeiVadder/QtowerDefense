#include "client/enemy_instances.h"

#include <QColor>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>

#include <cstring>
#include <vector>

namespace qtd::client {

EnemyInstances::EnemyInstances(QQuick3DObject *parent) : QQuick3DInstancing(parent) {}

GameSession *EnemyInstances::game() const
{
    return game_;
}

float EnemyInstances::fieldSize() const
{
    return fieldSize_;
}

float EnemyInstances::enemyScale() const
{
    return enemyScale_;
}

void EnemyInstances::setGame(GameSession *game)
{
    if (game_ == game) {
        return;
    }
    if (game_) {
        disconnect(game_, nullptr, this, nullptr);
    }
    game_ = game;
    if (game_) {
        connect(game_, &GameSession::stateChanged, this, [this] { markDirty(); });
        connect(game_, &GameSession::frameChanged, this, [this] { markDirty(); });
    }
    markDirty();
    emit gameChanged();
}

void EnemyInstances::setFieldSize(float size)
{
    if (!qFuzzyCompare(fieldSize_, size)) {
        fieldSize_ = size;
        markDirty();
        emit fieldSizeChanged();
    }
}

void EnemyInstances::setEnemyScale(float scale)
{
    if (!qFuzzyCompare(enemyScale_, scale)) {
        enemyScale_ = scale;
        markDirty();
        emit enemyScaleChanged();
    }
}

QByteArray EnemyInstances::getInstanceBuffer(int *instanceCount)
{
    const std::vector<QPointF> places = game_ ? game_->enemyPlaces() : std::vector<QPointF>();
    constexpr qsizetype entrySize = sizeof(InstanceTableEntry);
    QByteArray buffer(static_cast<qsizetype>(places.size()) * entrySize, Qt::Uninitialized);
    char *next = buffer.data();
    for (const QPointF &place : places) {
        const QVector3D position(static_cast<float>(place.x()) * fieldSize_,
                                 0,
                                 static_cast<float>(place.y()) * fieldSize_);
        const InstanceTableEntry entry = calculateTableEntry(position,
                                                             QVector3D(1, 1, 1) * enemyScale_,
                                                             QVector3D(),
                                                             QColor(Qt::white),
                                                             QVector4D());
        std::memcpy(next, &entry, entrySize);
        next += entrySize;
    }
    if (instanceCount != nullptr) {
        *instanceCount = static_cast<int>(places.size());
    }
    return buffer;
}

} // namespace qtd::client
