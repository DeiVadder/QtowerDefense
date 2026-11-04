#include "core/version.h"

#include <QTest>

using qtd::core::parseVersion;
using qtd::core::Version;

class TestVersion : public QObject
{
    Q_OBJECT

private slots:
    static void projectVersionMatchesBuildSystem()
    {
        QCOMPARE(qtd::core::toString(qtd::core::projectVersion()),
                 QStringLiteral(QTD_EXPECTED_VERSION));
    }

    static void parsesValidText()
    {
        QVERIFY(parseVersion(u"12.0.345") == (Version{12, 0, 345}));
    }

    static void rejectsMalformedText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("two parts") << QStringLiteral("1.2");
        QTest::newRow("four parts") << QStringLiteral("1.2.3.4");
        QTest::newRow("empty part") << QStringLiteral("1..3");
        QTest::newRow("letters") << QStringLiteral("1.a.3");
        QTest::newRow("plus sign") << QStringLiteral("+1.2.3");
        QTest::newRow("minus sign") << QStringLiteral("1.-2.3");
        QTest::newRow("space") << QStringLiteral("1. 2.3");
        QTest::newRow("too large") << QStringLiteral("99999999999.0.0");
    }

    static void rejectsMalformedText()
    {
        QFETCH(const QString, text);
        QVERIFY(!parseVersion(text).has_value());
    }

    static void formatsParts()
    {
        QCOMPARE(qtd::core::toString(Version{3, 14, 0}), QStringLiteral("3.14.0"));
    }

    static void ordersNumericallyNotTextually()
    {
        QVERIFY((Version{1, 9, 0}) < (Version{1, 10, 0}));
        QVERIFY((Version{2, 0, 0}) > (Version{1, 99, 99}));
        QVERIFY((Version{1, 2, 3}) == (Version{1, 2, 3}));
    }
};

QTEST_APPLESS_MAIN(TestVersion)
#include "tst_version.moc"
