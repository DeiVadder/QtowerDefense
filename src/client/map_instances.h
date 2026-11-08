#pragma once

#include "client/game_session.h"

#include <QByteArray>
#include <QPointer>

#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dinstancing.h>
#include <initializer_list>

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
    Q_PROPERTY(float modelSize READ modelSize WRITE setModelSize NOTIFY modelSizeChanged)
    Q_PROPERTY(int typeIndex READ typeIndex WRITE setTypeIndex NOTIFY typeIndexChanged)
    // Of an enemy with walk frames, the one frame drawn here; -1 draws all, without frames.
    Q_PROPERTY(int walkFrame READ walkFrame WRITE setWalkFrame NOTIFY walkFrameChanged)

public:
    enum class Layer : quint8 {
        Board,      // one block for each cell of the grid, in the colour of its terrain
        Towers,     // one block for each cell of a tower, it takes the taps
        TowerLooks, // the towers of one type as they are seen
        Enemies,    // the enemies of one type
        Rocks,
        Trees,
        Shots, // what is on its way from a tower that just fired
    };
    Q_ENUM(Layer)

    explicit MapInstances(QQuick3DObject *parent = nullptr);

    [[nodiscard]] GameSession *game() const;
    [[nodiscard]] Layer layer() const;
    [[nodiscard]] float fieldSize() const;
    [[nodiscard]] float enemyScale() const;
    [[nodiscard]] float modelSize() const;
    [[nodiscard]] int typeIndex() const;
    [[nodiscard]] int walkFrame() const;

    void setGame(GameSession *game);
    void setLayer(Layer layer);
    void setFieldSize(float size);
    void setModelSize(float size);
    void setEnemyScale(float scale);
    void setTypeIndex(int index);
    void setWalkFrame(int frame);

signals:
    void gameChanged();
    void layerChanged();
    void fieldSizeChanged();
    void enemyScaleChanged();
    void modelSizeChanged();
    void typeIndexChanged();
    void walkFrameChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    void markDirtyFor(std::initializer_list<Layer> layers);

    QPointer<GameSession> game_;
    Layer layer_ = Layer::Enemies;
    float fieldSize_ = 1;
    float enemyScale_ = 1;
    float modelSize_ = 1; // the edge of what TowerLooks draws, in its own units
    int typeIndex_ = 0;
    int walkFrame_ = -1;
};

} // namespace qtd::client
