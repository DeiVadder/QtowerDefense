#pragma once

#include <QByteArray>

#include <functional>

namespace qtd::automation {

/// Calls handle for each line on standard input that is not empty, until the input ends.
void readControlLines(const std::function<void(const QByteArray &)> &handle);

/// Writes one answer as a line to standard output, at once: the other side waits for it.
void writeControlAnswer(const QByteArray &answer);

} // namespace qtd::automation
