#pragma once

#include "client/landscape_view.h"

#include <QPointer>

#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dtexturedata.h>

namespace qtd::client {

/// The colors of the ground. New data would keep the old mipmaps in the distance, so a new
/// landscape gets a new picture from Ground.
class TerrainPicture : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtd::client::LandscapeView *view READ view WRITE setView NOTIFY viewChanged)

public:
    using QQuick3DTextureData::QQuick3DTextureData;

    [[nodiscard]] LandscapeView *view() const;
    void setView(LandscapeView *view);

signals:
    void viewChanged();

private:
    void paint();

    QPointer<LandscapeView> view_;
};

/// One cell of the grid that shows where towers go: a thin frame, the rest is clear.
class GridCellPicture : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit GridCellPicture(QQuick3DObject *parent = nullptr);
};

} // namespace qtd::client
