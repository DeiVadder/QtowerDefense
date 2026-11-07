#pragma once

#include "core/world.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace qtd::client {

/// The campaign world for QML: its sections, which are open and the stars won (E-32). The rules
/// live in core, the progress of the player in a file of its own.
class WorldSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString worldFile READ worldFile WRITE setWorldFile NOTIFY worldChanged)
    Q_PROPERTY(QString progressFile READ progressFile WRITE setProgressFile NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY worldChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY progressChanged)
    Q_PROPERTY(QVariantList links READ links NOTIFY worldChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)

public:
    explicit WorldSession(QObject *parent = nullptr);

    const QString &worldFile() const;
    void setWorldFile(const QString &fileName);
    const QString &progressFile() const;
    void setProgressFile(const QString &fileName);
    const QString &error() const;

    /// For each section: id, name, landscape, x and y from 0 to 1, color, landmarks (model and
    /// size in fields), open, stars
    /// and playable (open and with a map).
    QVariantList sections() const;
    /// The ways as `from` and `to`, section numbers, each way once.
    QVariantList links() const;

    int selected() const;
    void setSelected(int section);

    /// The file of a section's map next to the world file, empty if it has none yet.
    Q_INVOKABLE QString mapFile(int section) const;
    /// Keeps the stars of a won game and saves them. Gives false if they could not be saved.
    Q_INVOKABLE bool recordGame(int section, int stars);

signals:
    void worldChanged();
    void progressChanged();
    void selectedChanged();

private:
    void load();
    /// The first open section with a map that is not won yet, else the start.
    int nextSection() const;

    QString worldFile_;
    QString progressFile_;
    QString error_;
    core::World world_;
    core::CampaignProgress progress_;
    int selected_ = 0;
};

} // namespace qtd::client
