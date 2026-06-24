#include "spry/clipboard.h"

namespace spry {

namespace {
ClipboardGet g_get;
ClipboardSet g_set;
std::string g_internal; // fallback when the host installs no handlers
} // namespace

void setClipboardHandlers(ClipboardGet get, ClipboardSet set) {
    g_get = std::move(get);
    g_set = std::move(set);
}

std::string getClipboardText() { return g_get ? g_get() : g_internal; }

void setClipboardText(const std::string& s) {
    if (g_set)
        g_set(s);
    else
        g_internal = s;
}

} // namespace spry
