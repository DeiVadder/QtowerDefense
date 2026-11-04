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

public:
    explicit AppInfo(QObject *parent = nullptr);

    [[nodiscard]] QString version() const;
    [[nodiscard]] QString windowTitle() const;
};

} // namespace qtd::client
