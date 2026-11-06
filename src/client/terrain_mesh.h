#pragma once

#include "client/landscape_view.h"

#include <QPointer>

#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dgeometry.h>

namespace qtd::client {

/// The ground as one mesh, in the coordinates of the map: the board and the land around it. It
/// shows the landscape it was given, Ground makes a new one for a new landscape.
class TerrainMesh : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::LandscapeView *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(float fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)

public:
    using QQuick3DGeometry::QQuick3DGeometry;

    [[nodiscard]] LandscapeView *view() const;
    void setView(LandscapeView *view);
    [[nodiscard]] float fieldSize() const;
    void setFieldSize(float size);

signals:
    void viewChanged();
    void fieldSizeChanged();

private:
    void rebuild();

    QPointer<LandscapeView> view_;
    float fieldSize_ = 1;
};

} // namespace qtd::client
