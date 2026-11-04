#include "client/graphics.h"

#include <QSurfaceFormat>
#include <QtQuick3D/qquick3d.h>

namespace qtd::client {

void configureGraphics()
{
    QSurfaceFormat::setDefaultFormat(QQuick3D::idealSurfaceFormat());
}

} // namespace qtd::client
