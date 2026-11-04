#pragma once

#include "Common.h"
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>

namespace ChromiumAltTab {

class TabExtractor {
public:
    using TabUpdateCallback = std::function<void(std::vector<std::shared_ptr<TabInfo>>)>;

    TabExtractor();
    ~TabExtractor();

    // Start background extraction service
    bool Start(TabUpdateCallback callback);
    // Stop background service
    void Stop();

    // Trigger immediate extraction (e.g. when Alt-Tab is invoked)
    void TriggerScan();

    // Synchronous scan of all active Chromium windows
    std::vector<std::shared_ptr<TabInfo>> ScanAllChromiumTabs();

    // Scan tabs for a specific browser window handle
    std::vector<std::shared_ptr<TabInfo>> ScanWindowTabs(HWND hwnd, BrowserType browserType);

    // Helpers to identify Chromium top-level browser windows
    static bool IsChromiumBrowserWindow(HWND hwnd, BrowserType& outType);
    static std::vector<HWND> EnumChromiumWindows();

private:
    void WorkerLoop();
    bool InitUIA();
    void UninitUIA();

    // Extract active tab URL from address bar (Omnibox)
    std::wstring ExtractOmniboxUrl(IUIAutomation* pUIA, IUIAutomationElement* pRootElement);

    // Favicon retrieval
    HICON ExtractBrowserFavicon(HWND hwnd, const std::wstring& domain);

    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_scanRequested{ false };
    std::thread m_workerThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    TabUpdateCallback m_callback;

    // COM UI Automation pointer (thread-local to worker thread)
    CComPtr<IUIAutomation> m_pUIA;
    CComPtr<IUIAutomationCondition> m_pTabItemCondition;
    CComPtr<IUIAutomationCacheRequest> m_pTabCacheRequest;
};

} // namespace ChromiumAltTab

