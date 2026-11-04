#include "core/content_loader.h"
#include "core/replay.h"
#include "core/tick_clock.h"

#include <QDir>
#include <QFile>
#include <QTest>

#include <array>
#include <stdexcept>
#include <utility>

using qtd::core::Content;
using qtd::core::loadContentFile;
using qtd::core::loadReplayFile;
using qtd::core::Replay;
using qtd::core::ReplayPlayer;
using qtd::core::ReplayRecorder;
using qtd::core::TickClock;

using namespace Qt::StringLiterals;

namespace {

/// At least these games are in the repository. A golden test without files proves nothing.
constexpr qsizetype kMinimumGoldenFiles = 4;

/// Frame times with no pattern, from far above the tick length down to almost nothing.
constexpr std::array<qint64, 7> kFrameMicroseconds{16'667, 7'000, 33'334, 1, 120'000, 16'000, 50};

QString goldenFile(const QString &name)
{
    return QDir(QStringLiteral(QTD_GOLDEN_DIR)).filePath(name);
}

struct Golden
{
    Content content;
    Replay replay;
};

/// A problem with the files ends the test with the text of the problem.
Golden loadGolden(const QString &name)
{
    qtd::core::ReplayLoadResult replay = loadReplayFile(goldenFile(name));
    if (!replay.replay) {
        throw std::runtime_error(replay.error.toStdString());
    }
    const QString map = QDir(QStringLiteral(QTD_CONTENT_DIR)).filePath(replay.replay->content);
    qtd::core::ContentLoadResult content = loadContentFile(map);
    if (!content.content) {
        throw std::runtime_error(content.error.toStdString());
    }
    return Golden{*std::move(content.content), *std::move(replay.replay)};
}

void addGoldenRows()
{
    QTest::addColumn<QString>("name");
    const QStringList names = QDir(QStringLiteral(QTD_GOLDEN_DIR))
                                  .entryList({u"*.replay.json"_s}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        QTest::newRow(name.toUtf8().constData()) << name;
    }
}

} // namespace

class GoldenTest : public QObject
{
    Q_OBJECT

private slots:
    void theGoldenFilesAreThere()
    {
        const QStringList names =
            QDir(QStringLiteral(QTD_GOLDEN_DIR)).entryList({u"*.replay.json"_s}, QDir::Files);
        QVERIFY2(names.size() >= kMinimumGoldenFiles, qPrintable(names.join(u' ')));
    }

    void aGoldenGameKeepsItsHashes_data()
    {
        addGoldenRows();
    }

    /// The pinned values of the files against the rules of this build.
    void aGoldenGameKeepsItsHashes()
    {
        QFETCH(const QString, name);
        Golden golden = loadGolden(name);
        QVERIFY(golden.replay.checkpoints.size() > 1);
        const QString difference =
            qtd::core::verifyReplay(std::move(golden.content), std::move(golden.replay));
        QVERIFY2(difference.isEmpty(), qPrintable(difference));
    }

    void aGoldenGameDoesNotDependOnTheSpeed_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("speed");
        const QStringList names = QDir(QStringLiteral(QTD_GOLDEN_DIR))
                                      .entryList({u"*.replay.json"_s}, QDir::Files, QDir::Name);
        for (const QString &name : names) {
            for (const int speed : {1, 2, 3}) {
                const QString row = u"%1 at %2x"_s.arg(name).arg(speed);
                QTest::newRow(row.toUtf8().constData()) << name << speed;
            }
        }
    }

    /// Plays like a real client: uneven frames, a clock that turns them into ticks, and a pause on
    /// every fifth frame. Commands fall somewhere inside the batches of ticks.
    void aGoldenGameDoesNotDependOnTheSpeed()
    {
        QFETCH(const QString, name);
        QFETCH(const int, speed);
        Golden golden = loadGolden(name);
        ReplayPlayer player(std::move(golden.content), std::move(golden.replay));
        TickClock clock;
        clock.setSpeed(speed);
        for (std::size_t frame = 0; !player.finished(); ++frame) {
            clock.setPaused(frame % 5 == 4);
            player.step(clock.ticksFor(kFrameMicroseconds[frame % kFrameMicroseconds.size()]));
        }
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));
    }

    void recordingAGoldenGameAgainGivesTheSameFile_data()
    {
        addGoldenRows();
    }

    /// Covers the recorder and the format: the commands alone must lead to the file, byte by byte.
    void recordingAGoldenGameAgainGivesTheSameFile()
    {
        QFETCH(const QString, name);
        Golden golden = loadGolden(name);
        ReplayRecorder recorder(
            std::move(golden.content), golden.replay.content, golden.replay.seed);
        for (const qtd::core::RecordedCommand &recorded : golden.replay.commands) {
            recorder.advance(recorded.tick - recorder.simulation().state().tick);
            recorder.execute(recorded.command);
        }
        recorder.advance(golden.replay.checkpoints.back().tick -
                         recorder.simulation().state().tick);
        QFile file(goldenFile(name));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(toJson(recorder.replay()), file.readAll());
    }
};

QTEST_APPLESS_MAIN(GoldenTest)
#include "tst_golden.moc"
