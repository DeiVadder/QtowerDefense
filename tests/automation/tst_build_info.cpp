#include "automation/build_info.h"

#include <QTest>

class TestBuildInfo : public QObject
{
    Q_OBJECT

private slots:
    static void writesCompactJson()
    {
        QCOMPARE(qtd::automation::buildInfoJson(u"qtd_headless", {2, 10, 7}),
                 QByteArray("{\"program\":\"qtd_headless\",\"version\":\"2.10.7\"}"));
    }

    static void escapesProgramName()
    {
        QCOMPARE(qtd::automation::buildInfoJson(u"a\"b", {0, 0, 1}),
                 QByteArray("{\"program\":\"a\\\"b\",\"version\":\"0.0.1\"}"));
    }
};

QTEST_APPLESS_MAIN(TestBuildInfo)
#include "tst_build_info.moc"
