#include "client/translation.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTest>

using qtd::client::Translation;
using Choice = qtd::client::Translation::Choice;

class TestTranslation : public QObject
{
    Q_OBJECT

private slots:
    static void init()
    {
        QLocale::setDefault(QLocale(QLocale::English));
    }

    static void theSystemSpeaksGermanOnlyWhereItIsGerman_data()
    {
        QTest::addColumn<QLocale>("system");
        QTest::addColumn<Choice>("spoken");
        QTest::newRow("Germany") << QLocale(QLocale::German, QLocale::Germany) << Choice::German;
        QTest::newRow("Austria") << QLocale(QLocale::German, QLocale::Austria) << Choice::German;
        QTest::newRow("England") << QLocale(QLocale::English, QLocale::UnitedKingdom)
                                 << Choice::English;
        QTest::newRow("France, no translation") << QLocale(QLocale::French) << Choice::English;
    }

    static void theSystemSpeaksGermanOnlyWhereItIsGerman()
    {
        QFETCH(const QLocale, system);
        QFETCH(const Choice, spoken);
        QCOMPARE(Translation::spokenLanguage(Choice::System, system), spoken);
        QCOMPARE(Translation::spokenLanguage(Choice::English, system), Choice::English);
        QCOMPARE(Translation::spokenLanguage(Choice::German, system), Choice::German);
    }

    static void aChoiceInstallsItsTexts()
    {
        Translation translation;
        QCOMPARE(translation.language(), QStringLiteral("en"));
        translation.setChoice(Choice::German);
        QCOMPARE(translation.language(), QStringLiteral("de"));
        QCOMPARE(QCoreApplication::translate("SettingsPanel", "Language"),
                 QStringLiteral("Sprache"));
        QCOMPARE(QCoreApplication::translate("WorldMap", "Best: %n round(s)", nullptr, 1),
                 QStringLiteral("Bestwert: 1 Runde"));
        translation.setChoice(Choice::English);
        QCOMPARE(QCoreApplication::translate("SettingsPanel", "Language"),
                 QStringLiteral("Language"));
    }

    // Without the English file, "1 round(s)" would stand on the screen.
    static void englishBringsItsPlurals()
    {
        const Translation translation;
        QCOMPARE(QCoreApplication::translate("WorldMap", "Best: %n round(s)", nullptr, 1),
                 QStringLiteral("Best: 1 round"));
        QCOMPARE(QCoreApplication::translate("WorldMap", "Best: %n round(s)", nullptr, 3),
                 QStringLiteral("Best: 3 rounds"));
    }

    static void theTranslationLeavesWithItsObject()
    {
        {
            Translation translation;
            translation.setChoice(Choice::German);
        }
        QCOMPARE(QCoreApplication::translate("SettingsPanel", "Language"),
                 QStringLiteral("Language"));
    }

    // An edited settings file may hold any number.
    static void aChoiceOutOfRangeMeansTheSystem()
    {
        QLocale::setDefault(QLocale(QLocale::German));
        Translation translation;
        translation.setChoice(Choice::English);
        translation.setChoice(static_cast<Choice>(7));
        QCOMPARE(translation.choice(), Choice::System);
        QCOMPARE(translation.language(), QStringLiteral("de"));
    }
};

QTEST_GUILESS_MAIN(TestTranslation)
#include "tst_translation.moc"
