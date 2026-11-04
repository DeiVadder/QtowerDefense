#include "automation/build_info.h"
#include "automation/control_session.h"
#include "automation/headless_runner.h"
#include "core/version.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace {

/// One JSON request per line on stdin, one answer per line on stdout, until stdin ends. Binary, as
/// the text mode of Windows ends the input at a Ctrl-Z. Not QFile: its readLine gave stray bytes
/// after CRLF, and QTextStream reads in blocks while the other side waits for its answer.
int runControl()
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    QTextStream output(stdout);
    qtd::automation::ControlSession session;
    try {
        std::string text;
        while (std::getline(std::cin, text)) {
            const QByteArray line = QByteArray::fromStdString(text).trimmed();
            if (!line.isEmpty()) {
                output << session.handle(line) << '\n';
                output.flush();
            }
        }
    } catch (const std::exception &error) {
        std::fputs(error.what(), stderr);
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    const QCoreApplication app(argc, argv);
    constexpr int kExitUsage = 2;

    const qtd::automation::HeadlessArguments arguments =
        qtd::automation::parseHeadlessArguments(app.arguments());
    if (arguments.help) {
        QTextStream(stdout) << arguments.helpText;
        return 0;
    }
    if (arguments.buildInfo) {
        QTextStream(stdout) << qtd::automation::buildInfoJson(u"qtd_headless",
                                                              qtd::core::projectVersion())
                            << '\n';
        return 0;
    }
    if (!arguments.options) {
        QTextStream(stderr) << arguments.error << '\n';
        return kExitUsage;
    }
    if (arguments.options->control) {
        return runControl();
    }
    const qtd::automation::HeadlessOutcome outcome =
        qtd::automation::runHeadless(*arguments.options);
    QTextStream(stdout) << outcome.json << '\n';
    return outcome.exitCode;
}
