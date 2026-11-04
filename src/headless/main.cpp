#include "automation/build_info.h"
#include "automation/headless_runner.h"
#include "core/version.h"

#include <QCoreApplication>
#include <QTextStream>

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
    const qtd::automation::HeadlessOutcome outcome =
        qtd::automation::runHeadless(*arguments.options);
    QTextStream(stdout) << outcome.json << '\n';
    return outcome.exitCode;
}
