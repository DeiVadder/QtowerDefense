#pragma once

#include <QLocale>
#include <QObject>
#include <QTranslator>
#include <QtQml/qqmlregistration.h>

namespace qtd::client {

/// The language of the texts. The program speaks English, a translation replaces its texts. It
/// installs the language of the system as it is made, before QML reads any text: put it first in
/// the window.
class Translation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// What the player chose. Values out of range, from an edited settings file, mean System, the
    /// language of QLocale(): the system unless a test sets another one.
    Q_PROPERTY(Choice choice READ choice WRITE setChoice NOTIFY languageChanged)
    /// The language that speaks now, as its code: "en" or "de".
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    enum class Choice : quint8 {
        System,
        English,
        German,
    };
    Q_ENUM(Choice)

    explicit Translation(QObject *parent = nullptr);
    ~Translation() override;
    Translation(const Translation &) = delete;
    Translation &operator=(const Translation &) = delete;
    Translation(Translation &&) = delete;
    Translation &operator=(Translation &&) = delete;

    [[nodiscard]] Choice choice() const;
    void setChoice(Choice choice);
    [[nodiscard]] QString language() const;

    /// The language a choice means on a system with this locale. German if the system speaks it,
    /// English for every other language: there is no translation for them.
    [[nodiscard]] static Choice spokenLanguage(Choice choice, const QLocale &system);

signals:
    void languageChanged();

private:
    /// Installs the translation of the chosen language and has QML read its texts again.
    void apply();

    Choice choice_ = Choice::System;
    Choice spoken_ = Choice::English;
    QTranslator translator_;
    bool installed_ = false;
};

} // namespace qtd::client
