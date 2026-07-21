#pragma once
#include <functional>
#include <string>

/// @file clipboard.h
/// Clipboard access for editable widgets (#213). The toolkit core stays free of
/// any platform clipboard API: the host installs get/set handlers (e.g. backed by
/// `SDL_GetClipboardText` / `SDL_SetClipboardText`). Until it does, an in-process
/// buffer is used so widgets and headless tests work standalone.

namespace spry {

/// @addtogroup input
/// @{

using ClipboardGet = std::function<std::string()>;        ///< Host hook: read the clipboard.
using ClipboardSet = std::function<void(const std::string&)>;  ///< Host hook: write the clipboard.

/// Install host clipboard get/set handlers (e.g. the SDL-backed ones).
void setClipboardHandlers(ClipboardGet get, ClipboardSet set);

/// Read the clipboard via the installed handler (or the in-process buffer).
std::string getClipboardText();
/// Write the clipboard via the installed handler (or the in-process buffer).
void setClipboardText(const std::string& s);

/// @}

} // namespace spry
