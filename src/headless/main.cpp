#include "automation/build_info.h"
#include "core/version.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    const QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption buildInfo(QStringLiteral("build-info"),
                                       QStringLiteral("Print program and version as JSON."));
    parser.addOption(buildInfo);
    parser.process(app);

    if (!parser.isSet(buildInfo)) {
        QTextStream(stderr) << "No simulation yet; only --build-info is available.\n";
        return 2;
    }
    const auto json = qtd::automation::buildInfoJson(u"qtd_headless", qtd::core::projectVersion());
    QTextStream(stdout) << json << '\n';
    return 0;
}
