#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuickTest/quicktest.h>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

using namespace Qt::StringLiterals;

/// Gives the tests a folder of their own for saved games, and the first map with gold enough for
/// a full hand of any cards.
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(u"saveFolder"_s, folder_.path());
        QFile map(u":/content/training_ground.json"_s);
        QFile rich(folder_.filePath(u"rich_ground.json"_s));
        if (map.open(QIODevice::ReadOnly) && rich.open(QIODevice::WriteOnly)) {
            rich.write(map.readAll().replace(R"("startGold": 120)", R"("startGold": 5000)"));
        }
        engine->rootContext()->setContextProperty(u"richMap"_s, rich.fileName());
    }

private:
    QTemporaryDir folder_;
};

QUICK_TEST_MAIN_WITH_SETUP(qml, Setup)
#include "tst_qml.moc"
