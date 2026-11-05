#pragma once

#include "client/game_session.h"

#include <QByteArray>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dinstancing.h>

namespace qtd::client {

/// Draws all enemies of a game with one model: their number changes with every wave, and creating
/// a QML object for each of them would not hold up with hundreds.
class EnemyInstances : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::GameSession *game READ game WRITE setGame NOTIFY gameChanged)
    Q_PROPERTY(float fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)
    Q_PROPERTY(float enemyScale READ enemyScale WRITE setEnemyScale NOTIFY enemyScaleChanged)

public:
    explicit EnemyInstances(QQuick3DObject *parent = nullptr);

    [[nodiscard]] GameSession *game() const;
    [[nodiscard]] float fieldSize() const;
    [[nodiscard]] float enemyScale() const;

    void setGame(GameSession *game);
    void setFieldSize(float size);
    void setEnemyScale(float scale);

signals:
    void gameChanged();
    void fieldSizeChanged();
    void enemyScaleChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    QPointer<GameSession> game_;
    float fieldSize_ = 1;
    float enemyScale_ = 1;
};

} // namespace qtd::client
