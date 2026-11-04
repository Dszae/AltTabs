#include "AltTabHook.h"
#include <sstream>

namespace ChromiumAltTab {

static const wchar_t* kProxyWindowClassName = L"ChromiumAltTab_ProxyWindow";

AltTabHookManager& AltTabHookManager::Instance() {
    static AltTabHookManager instance;
    return instance;
}

AltTabHookManager::AltTabHookManager() = default;

AltTabHookManager::~AltTabHookManager() {
    Uninitialize();
}

bool AltTabHookManager::Initialize() {
    if (m_initialized) return true;

    RegisterProxyWindowClass();

    m_tabExtractor = std::make_unique<TabExtractor>();
    m_tabExtractor->Start([this](std::vector<std::shared_ptr<TabInfo>> tabs) {
        UpdateTabs(std::move(tabs));
    });

    // Install low-level keyboard hook to capture Alt+Tab strokes and trigger fast scans
    m_hKeyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );

    InstallShellSymbolHooks();

    m_initialized = true;
    return true;
}

void AltTabHookManager::Uninitialize() {
    if (!m_initialized) return;

    if (m_hKeyboardHook) {
        UnhookWindowsHookEx(m_hKeyboardHook);
        m_hKeyboardHook = nullptr;
    }

    UninstallShellSymbolHooks();

    if (m_tabExtractor) {
        m_tabExtractor->Stop();
        m_tabExtractor.reset();
    }

    CleanupProxyWindows();
    UnregisterProxyWindowClass();

    m_initialized = false;
}

bool AltTabHookManager::RegisterProxyWindowClass() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ProxyWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kProxyWindowClassName;

    return RegisterClassExW(&wc) != 0;
}

void AltTabHookManager::UnregisterProxyWindowClass() {
    UnregisterClassW(kProxyWindowClassName, GetModuleHandleW(nullptr));
}

LRESULT CALLBACK AltTabHookManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKbd->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                // User pressed Alt+Tab -> trigger instant background scan
                AltTabHookManager::Instance().m_tabExtractor->TriggerScan();
            }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK AltTabHookManager::ProxyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ACTIVATE: {
            WORD state = LOWORD(wParam);
            if (state != WA_INACTIVE) {
                // The user selected this proxy domain item in Alt-Tab
                auto* pGroup = reinterpret_cast<DomainGroup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (pGroup) {
                    std::shared_ptr<DomainGroup> groupRef;
                    {
                        std::lock_guard<std::mutex> lock(AltTabHookManager::Instance().m_stateMutex);
                        auto it = AltTabHookManager::Instance().m_domainGroups.find(pGroup->domain);
                        if (it != AltTabHookManager::Instance().m_domainGroups.end()) {
                            groupRef = it->second;
                        }
                    }

                    if (groupRef) {
                        ActivationRouter::ActivateDomainGroup(groupRef);
                    }
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void AltTabHookManager::UpdateTabs(std::vector<std::shared_ptr<TabInfo>> tabs) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    m_domainGroups = m_domainGrouper.GroupTabs(tabs);
    SyncProxyWindows();
}

void AltTabHookManager::SyncProxyWindows() {
    // Collect active domains
    std::vector<std::wstring> currentDomains;
    for (const auto& [domain, group] : m_domainGroups) {
        currentDomains.push_back(domain);
    }

    // 1. Destroy unneeded proxy windows
    for (auto it = m_activeProxyWindows.begin(); it != m_activeProxyWindows.end();) {
        HWND hwnd = *it;
        auto* pGroup = reinterpret_cast<DomainGroup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!pGroup || m_domainGroups.find(pGroup->domain) == m_domainGroups.end()) {
            DestroyWindow(hwnd);
            it = m_activeProxyWindows.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Create or update proxy windows for each domain group
    for (auto& [domain, group] : m_domainGroups) {
        if (!group || group->tabs.empty()) continue;

        // Construct descriptive title: e.g., "GitHub (3 tabs) - Chromium"
        std::wstringstream titleStream;
        if (group->tabs.size() == 1) {
            titleStream << group->tabs[0]->title;
        } else {
            titleStream << domain << L" (" << group->tabs.size() << L" tabs)";
        }
        std::wstring displayTitle = titleStream.str();

        HWND hwnd = group->proxyHwnd;
        if (!hwnd || !IsWindow(hwnd)) {
            // Create a top-level window styled specifically for Windows TaskSwitcher enumeration:
            // Must have WS_EX_APPWINDOW and not WS_EX_TOOLWINDOW
            hwnd = CreateWindowExW(
                WS_EX_APPWINDOW,
                kProxyWindowClassName,
                displayTitle.c_str(),
                WS_POPUP | WS_CLIPSIBLINGS,
                -32000, -32000, 300, 200, // Kept off-screen to avoid desktop visual clutter
                nullptr,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr
            );

            if (hwnd) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(group.get()));
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                group->proxyHwnd = hwnd;
                m_activeProxyWindows.push_back(hwnd);
            }
        } else {
            // Update title if tab counts or active tab changed
            SetWindowTextW(hwnd, displayTitle.c_str());
        }

        // Set icon from active tab or browser
        if (hwnd && !group->tabs.empty()) {
            HICON icon = group->tabs[0]->hFavicon;
            if (icon) {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
            }
        }
    }
}

void AltTabHookManager::CleanupProxyWindows() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    for (HWND hwnd : m_activeProxyWindows) {
        if (IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
    }
    m_activeProxyWindows.clear();
    m_domainGroups.clear();
}

bool AltTabHookManager::InstallShellSymbolHooks() {
    // Reserved for twinui.pcshell.dll symbol hooks inside Windhawk mod entry point
    return true;
}

void AltTabHookManager::UninstallShellSymbolHooks() {
    // Reserved for symbol unhooking
}

} // namespace ChromiumAltTab

