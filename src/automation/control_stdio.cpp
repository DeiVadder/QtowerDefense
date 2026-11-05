#include "automation/control_stdio.h"

#include <QTextStream>
#include <QtGlobal>

#include <cstdio>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace qtd::automation {

// Binary, as the text mode of Windows ends the input at a Ctrl-Z. Not QFile: its readLine gave
// stray bytes after CRLF, and QTextStream reads in blocks while the other side waits for its
// answer.
void readControlLines(const std::function<void(const QByteArray &)> &handle)
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string text;
    while (std::getline(std::cin, text)) {
        const QByteArray line = QByteArray::fromStdString(text).trimmed();
        if (!line.isEmpty()) {
            handle(line);
        }
    }
}

void writeControlAnswer(const QByteArray &answer)
{
    QTextStream output(stdout);
    output << answer << '\n';
    output.flush();
}

} // namespace qtd::automation
