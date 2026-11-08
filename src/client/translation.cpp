#include "client/translation.h"

#include <QCoreApplication>
#include <QQmlEngine>

using namespace Qt::StringLiterals;

namespace qtd::client {

Translation::Translation(QObject *parent) : QObject(parent)
{
    apply();
}

Translation::~Translation()
{
    if (installed_) {
        QCoreApplication::removeTranslator(&translator_);
    }
}

Translation::Choice Translation::choice() const
{
    return choice_;
}

void Translation::setChoice(Choice choice)
{
    if (choice != Choice::English && choice != Choice::German) {
        choice = Choice::System;
    }
    if (choice == choice_) {
        return;
    }
    choice_ = choice;
    apply();
    emit languageChanged();
}

void Translation::apply()
{
    spoken_ = spokenLanguage(choice_, QLocale());
    if (installed_) {
        QCoreApplication::removeTranslator(&translator_);
        installed_ = false;
    }
    // English needs its file only for the plurals: "1 round", "2 rounds".
    installed_ = translator_.load(u":/i18n/qtowerdefense_%1.qm"_s.arg(language())) &&
                 QCoreApplication::installTranslator(&translator_);
    if (!installed_) {
        spoken_ = Choice::English; // without its file, the translation is not there
    }
    if (QQmlEngine *engine = qmlEngine(this)) {
        engine->retranslate(); // made texts read themselves again
    }
}

QString Translation::language() const
{
    return spoken_ == Choice::German ? u"de"_s : u"en"_s;
}

Translation::Choice Translation::spokenLanguage(Choice choice, const QLocale &system)
{
    if (choice != Choice::System) {
        return choice;
    }
    return system.language() == QLocale::German ? Choice::German : Choice::English;
}

} // namespace qtd::client
