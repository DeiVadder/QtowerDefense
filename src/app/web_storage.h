#pragma once

#include <QString>

#include <functional>

namespace qtd::app {

/// Keeps FOLDER in the IndexedDB of the browser, so that settings, the world and saved games
/// survive a reload (E-47). Calls READY once the files of the last visit are back. WebAssembly
/// only.
void openWebStorage(const QString &folder, std::function<void()> ready);

} // namespace qtd::app
