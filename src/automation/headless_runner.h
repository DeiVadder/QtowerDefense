#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace qtd::automation {

constexpr quint64 kDefaultSeed = 1;

/// What a run of qtd_headless should do, see docs/Architektur.md.
struct HeadlessOptions
{
    QString map;
    std::optional<quint64> seed; // otherwise the seed of the script, or kDefaultSeed
    qint64 runs = 1;
    QString bot = QStringLiteral("idle");
    QString script;
    QString replay;
    QString record;
    QString output;
    qint64 maxTicks = 1'000'000;
    bool benchmark = false;
};

/// Holds the options, or an error text.
struct HeadlessArguments
{
    std::optional<HeadlessOptions> options;
    bool buildInfo = false;
    bool help = false;
    QString helpText;
    QString error;
};

/// Reads the command line, the program name comes first.
HeadlessArguments parseHeadlessArguments(const QStringList &arguments);

/// The report as JSON, and the exit code of the program.
struct HeadlessOutcome
{
    int exitCode = 0;
    QByteArray json;
};

/// Plays the games or the replay. Problems end up in the report as {"error": text}.
HeadlessOutcome runHeadless(const HeadlessOptions &options);

} // namespace qtd::automation
