#pragma once

#include "client/game_session.h"
#include "client/ground_field.h"
#include "client/landscape.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <QtQml/qqmlregistration.h>
#include <memory>

namespace qtd::client {

/// The landscape of the running map, for the scene. It only reads the game.
class LandscapeView : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::GameSession *game READ game WRITE setGame NOTIFY gameChanged)
    Q_PROPERTY(QColor sky READ sky NOTIFY landscapeChanged)
    Q_PROPERTY(QColor water READ water NOTIFY landscapeChanged)
    Q_PROPERTY(QColor plain READ plain NOTIFY landscapeChanged)
    Q_PROPERTY(bool hasWater READ hasWater NOTIFY landscapeChanged)
    Q_PROPERTY(bool lakes READ lakes NOTIFY landscapeChanged)
    Q_PROPERTY(int generation READ generation NOTIFY landscapeChanged)
    Q_PROPERTY(float waterLevel READ waterLevel NOTIFY landscapeChanged)
    Q_PROPERTY(QString treeModel READ treeModel NOTIFY landscapeChanged)
    Q_PROPERTY(QString rockModel READ rockModel NOTIFY landscapeChanged)
    Q_PROPERTY(QStringList sceneryModels READ sceneryModels NOTIFY landscapeChanged)

public:
    using QObject::QObject;

    [[nodiscard]] GameSession *game() const;
    void setGame(GameSession *game);

    [[nodiscard]] QColor sky() const;
    [[nodiscard]] QColor water() const;
    /// The color of the land beyond the ground, where the fog takes over.
    [[nodiscard]] QColor plain() const;
    [[nodiscard]] bool hasWater() const;
    [[nodiscard]] bool lakes() const;
    /// Counts the landscapes so far. The scene builds its ground anew for each.
    [[nodiscard]] int generation() const;
    [[nodiscard]] float waterLevel() const;
    [[nodiscard]] const QString &treeModel() const;
    [[nodiscard]] const QString &rockModel() const;
    /// One entry for each item of the scenery, in its order.
    [[nodiscard]] QStringList sceneryModels() const;

    [[nodiscard]] const Landscape &landscape() const;
    /// The ground of the map, null without one.
    [[nodiscard]] const GroundField *field() const;

signals:
    void gameChanged();
    void landscapeChanged();
    /// A tower came or went: plants on the board make room. The game may arrive after the scene.
    void towersChanged();

private:
    void refresh();

    QPointer<GameSession> game_;
    Landscape landscape_;
    int generation_ = 0;
    std::unique_ptr<GroundField> field_;
};

} // namespace qtd::client
