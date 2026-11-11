#include "client/graphics.h"

#include <QSurfaceFormat>
#include <QtQuick3D/qquick3d.h>

namespace qtd::client {

void configureGraphics()
{
#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID)
    // idealSurfaceFormat() tries an OpenGL context, and before the application exists that
    // crashes in the browser (E-47) and on Android (E-55). Both have only OpenGL ES: WebGL 2 needs
    // no version, Android is asked for ES 3, which Qt Quick 3D needs. And the buffers of a 3D scene.
    constexpr int depthBits = 24;
    constexpr int stencilBits = 8;
    QSurfaceFormat format;
    format.setDepthBufferSize(depthBits);
    format.setStencilBufferSize(stencilBits);
#ifdef Q_OS_ANDROID
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
#endif
    QSurfaceFormat::setDefaultFormat(format);
#else
    QSurfaceFormat::setDefaultFormat(QQuick3D::idealSurfaceFormat());
#endif
}

} // namespace qtd::client
