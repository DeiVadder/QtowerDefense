#pragma once

#include "client/landscape_view.h"

#include <QByteArray>
#include <QPointer>

#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dinstancing.h>

namespace qtd::client {

/// The places of one kind of scenery. Each kind is one instanced model, hundreds of plants stay a
/// handful of draw calls.
class SceneryInstances : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::LandscapeView *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(int item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(float fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)

public:
    using QQuick3DInstancing::QQuick3DInstancing;

    [[nodiscard]] LandscapeView *view() const;
    void setView(LandscapeView *view);
    [[nodiscard]] int item() const;
    void setItem(int item);
    [[nodiscard]] float fieldSize() const;
    void setFieldSize(float size);

signals:
    void viewChanged();
    void itemChanged();
    void fieldSizeChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    void towersMoved();

    QPointer<LandscapeView> view_;
    int item_ = 0;
    float fieldSize_ = 1;
};

} // namespace qtd::client
