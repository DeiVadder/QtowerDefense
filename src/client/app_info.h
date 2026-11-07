#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace qtd::client {

/// Build information for the QML side.
class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
    /// Whether the platform has windows that a player opens and closes, not apps that own the
    /// screen (Android, iOS).
    Q_PROPERTY(bool windowed READ windowed CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    [[nodiscard]] QString version() const;
    [[nodiscard]] QString windowTitle() const;
    [[nodiscard]] static bool windowed();
};

} // namespace qtd::client
