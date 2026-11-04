#pragma once

#include "Common.h"
#include "TabExtractor.h"
#include "DomainGrouper.h"
#include "ActivationRouter.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace ChromiumAltTab {

class AltTabHookManager {
public:
    static AltTabHookManager& Instance();

    bool Initialize();
    void Uninitialize();

    // Updates the current collection of tabs and refreshes virtual domain cards
    void UpdateTabs(std::vector<std::shared_ptr<TabInfo>> tabs);

    // Proxy window procedure handling Alt-Tab selection
    static LRESULT CALLBACK ProxyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Keyboard hook procedure to detect Alt+Tab invocation
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    AltTabHookManager();
    ~AltTabHookManager();

    bool RegisterProxyWindowClass();
    void UnregisterProxyWindowClass();

    void SyncProxyWindows();
    void CleanupProxyWindows();

    // Hooking twinui.pcshell.dll / explorer.exe internal Alt-Tab symbols
    bool InstallShellSymbolHooks();
    void UninstallShellSymbolHooks();

    std::mutex m_stateMutex;
    DomainGrouper m_domainGrouper;
    std::unique_ptr<TabExtractor> m_tabExtractor;
    
    std::unordered_map<std::wstring, std::shared_ptr<DomainGroup>> m_domainGroups;
    std::vector<HWND> m_activeProxyWindows;

    HHOOK m_hKeyboardHook = nullptr;
    bool m_altPressed = false;
    bool m_initialized = false;
};

} // namespace ChromiumAltTab

