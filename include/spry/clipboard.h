#pragma once
#include <functional>
#include <string>

// Clipboard access for editable widgets (#213). The toolkit core stays free of
// any platform clipboard API: the host installs get/set handlers (e.g. backed by
// SDL_GetClipboardText / SDL_SetClipboardText). Until it does, an in-process
// buffer is used so widgets and headless tests work standalone.
namespace spry {

using ClipboardGet = std::function<std::string()>;
using ClipboardSet = std::function<void(const std::string&)>;

void setClipboardHandlers(ClipboardGet get, ClipboardSet set);

std::string getClipboardText();
void setClipboardText(const std::string& s);

} // namespace spry
