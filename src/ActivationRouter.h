#pragma once

#include "Common.h"

namespace ChromiumAltTab {

class ActivationRouter {
public:
    ActivationRouter() = default;
    ~ActivationRouter() = default;

    // Forces the OS to switch foreground to the browser and selects the target tab
    static bool ActivateTab(const std::shared_ptr<TabInfo>& tab);

    // Activates the most relevant tab in a domain group (or the first tab)
    static bool ActivateDomainGroup(const std::shared_ptr<DomainGroup>& group);

    // Forces a window to the foreground, bypassing Windows ASFW restrictions
    static bool ForceForegroundWindow(HWND hwnd);

private:
    // Pattern-based UIA selection
    static bool InvokeUiaTab(const std::shared_ptr<TabInfo>& tab);

    // Fallback coordinate click injection via window messages
    static bool PostTabClickMessage(HWND browserHwnd, const RECT& screenRect);
};

} // namespace ChromiumAltTab

