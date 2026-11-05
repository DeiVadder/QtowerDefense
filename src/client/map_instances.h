#pragma once

#include "client/game_session.h"

#include <QByteArray>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dinstancing.h>

namespace qtd::client {

/// Draws one layer of a game with a single model: a map has hundreds of cells, and the number of
/// enemies changes with every wave. A QML object for each of them would not hold up.
class MapInstances : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::GameSession *game READ game WRITE setGame NOTIFY gameChanged)
    Q_PROPERTY(Layer layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(float fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)
    Q_PROPERTY(float enemyScale READ enemyScale WRITE setEnemyScale NOTIFY enemyScaleChanged)

public:
    enum class Layer : quint8 {
        Board, // one block for each cell of the grid, in the colour of its terrain
        Towers,
        Enemies,
    };
    Q_ENUM(Layer)

    explicit MapInstances(QQuick3DObject *parent = nullptr);

    [[nodiscard]] GameSession *game() const;
    [[nodiscard]] Layer layer() const;
    [[nodiscard]] float fieldSize() const;
    [[nodiscard]] float enemyScale() const;

    void setGame(GameSession *game);
    void setLayer(Layer layer);
    void setFieldSize(float size);
    void setEnemyScale(float scale);

signals:
    void gameChanged();
    void layerChanged();
    void fieldSizeChanged();
    void enemyScaleChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    void markLayerDirty(Layer layer);

    QPointer<GameSession> game_;
    Layer layer_ = Layer::Enemies;
    float fieldSize_ = 1;
    float enemyScale_ = 1;
};

} // namespace qtd::client
