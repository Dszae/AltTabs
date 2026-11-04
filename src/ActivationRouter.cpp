#include "ActivationRouter.h"

namespace ChromiumAltTab {

bool ActivationRouter::ForceForegroundWindow(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;

    // Check if already foreground
    if (GetForegroundWindow() == hwnd) {
        return true;
    }

    DWORD currentThreadId = GetCurrentThreadId();
    DWORD foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, nullptr);

    // Attach thread input to bypass Windows ASFW (AllowSetForegroundWindow) lockouts
    if (foregroundThreadId != 0 && foregroundThreadId != currentThreadId) {
        AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);
    }
    if (targetThreadId != 0 && targetThreadId != currentThreadId) {
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);
    }

    // Bypass foreground lock timeout
    DWORD lockTimeout = 0;
    SystemParametersInfoW(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &lockTimeout, 0);
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, SPIF_SENDCHANGE);

    AllowSetForegroundWindow(ASFW_ANY);

    // Restore if minimized
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetFocus(hwnd);

    // Restore lock timeout
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)(ULONG_PTR)lockTimeout, SPIF_SENDCHANGE);

    // Detach thread input
    if (foregroundThreadId != 0 && foregroundThreadId != currentThreadId) {
        AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);
    }
    if (targetThreadId != 0 && targetThreadId != currentThreadId) {
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    }

    return (GetForegroundWindow() == hwnd);
}

bool ActivationRouter::InvokeUiaTab(const std::shared_ptr<TabInfo>& tab) {
    if (!tab || !tab->uiaElement) return false;

    // 1. Try SelectionItemPattern::Select()
    CComPtr<IUIAutomationSelectionItemPattern> pSelPattern;
    HRESULT hr = tab->uiaElement->GetCurrentPatternAs(
        UIA_SelectionItemPatternId, 
        IID_IUIAutomationSelectionItemPattern, 
        reinterpret_cast<void**>(&pSelPattern)
    );

    if (SUCCEEDED(hr) && pSelPattern) {
        hr = pSelPattern->Select();
        if (SUCCEEDED(hr)) {
            return true;
        }
    }

    // 2. Try InvokePattern::Invoke()
    CComPtr<IUIAutomationInvokePattern> pInvPattern;
    hr = tab->uiaElement->GetCurrentPatternAs(
        UIA_InvokePatternId, 
        IID_IUIAutomationInvokePattern, 
        reinterpret_cast<void**>(&pInvPattern)
    );

    if (SUCCEEDED(hr) && pInvPattern) {
        hr = pInvPattern->Invoke();
        if (SUCCEEDED(hr)) {
            return true;
        }
    }

    return false;
}

bool ActivationRouter::PostTabClickMessage(HWND browserHwnd, const RECT& screenRect) {
    if (!IsWindow(browserHwnd)) return false;

    // Calculate midpoint coordinate
    POINT pt;
    pt.x = screenRect.left + (screenRect.right - screenRect.left) / 2;
    pt.y = screenRect.top + (screenRect.bottom - screenRect.top) / 2;

    // Convert screen coordinates to client coordinates
    if (!ScreenToClient(browserHwnd, &pt)) {
        return false;
    }

    LPARAM lParam = MAKELPARAM(pt.x, pt.y);

    // Post native mouse click sequence directly to Chromium's message queue
    PostMessageW(browserHwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    PostMessageW(browserHwnd, WM_LBUTTONUP, 0, lParam);

    return true;
}

bool ActivationRouter::ActivateTab(const std::shared_ptr<TabInfo>& tab) {
    if (!tab) return false;

    // Bring browser to foreground first
    ForceForegroundWindow(tab->browserHwnd);

    // Try UIA invocation pattern
    if (InvokeUiaTab(tab)) {
        return true;
    }

    // Fallback: If UIA call timed out or failed, post synthetic click message to coordinates
    if (tab->boundingRect.right > tab->boundingRect.left) {
        return PostTabClickMessage(tab->browserHwnd, tab->boundingRect);
    }

    return false;
}

bool ActivationRouter::ActivateDomainGroup(const std::shared_ptr<DomainGroup>& group) {
    if (!group || group->tabs.empty()) return false;

    // Prefer currently selected/active tab or the first tab in group
    size_t targetIndex = group->activeTabIndex < group->tabs.size() ? group->activeTabIndex : 0;
    return ActivateTab(group->tabs[targetIndex]);
}

} // namespace ChromiumAltTab

