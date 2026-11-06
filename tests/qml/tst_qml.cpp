#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuickTest/quicktest.h>

Q_IMPORT_QML_PLUGIN(QtowerDefenseClientPlugin)

using namespace Qt::StringLiterals;

/// Gives the tests a folder of their own for saved games.
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(u"saveFolder"_s, folder_.path());
    }

private:
    QTemporaryDir folder_;
};

QUICK_TEST_MAIN_WITH_SETUP(qml, Setup)
#include "tst_qml.moc"
