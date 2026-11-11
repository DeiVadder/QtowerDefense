#include "client/graphics.h"

#include <QSurfaceFormat>
#include <QtQuick3D/qquick3d.h>

namespace qtd::client {

void configureGraphics()
{
#ifdef Q_OS_WASM
    // idealSurfaceFormat() tries an OpenGL context, and before the application exists that
    // crashes in the browser. WebGL 2 needs no version, only the buffers of a 3D scene (E-47).
    constexpr int depthBits = 24;
    constexpr int stencilBits = 8;
    QSurfaceFormat format;
    format.setDepthBufferSize(depthBits);
    format.setStencilBufferSize(stencilBits);
    QSurfaceFormat::setDefaultFormat(format);
#else
    QSurfaceFormat::setDefaultFormat(QQuick3D::idealSurfaceFormat());
#endif
}

} // namespace qtd::client
