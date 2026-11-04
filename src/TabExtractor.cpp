#include "TabExtractor.h"
#include <tlhelp32.h>
#include <algorithm>

namespace ChromiumAltTab {

TabExtractor::TabExtractor() = default;

TabExtractor::~TabExtractor() {
    Stop();
}

bool TabExtractor::Start(TabUpdateCallback callback) {
    if (m_running.load()) return true;

    m_callback = callback;
    m_running.store(true);
    m_workerThread = std::thread(&TabExtractor::WorkerLoop, this);
    return true;
}

void TabExtractor::Stop() {
    if (!m_running.load()) return;

    m_running.store(false);
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void TabExtractor::TriggerScan() {
    m_scanRequested.store(true);
    m_cv.notify_one();
}

bool TabExtractor::InitUIA() {
    HRESULT hr = CoCreateInstance(
        CLSID_CUIAutomation8,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&m_pUIA)
    );

    if (FAILED(hr) || !m_pUIA) {
        // Fallback for older Windows 10 versions
        hr = CoCreateInstance(
            CLSID_CUIAutomation,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IUIAutomation,
            reinterpret_cast<void**>(&m_pUIA)
        );
    }

    if (FAILED(hr) || !m_pUIA) {
        return false;
    }

    // Condition to identify tab items
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_TabItemControlTypeId;
    hr = m_pUIA->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &m_pTabItemCondition);
    if (FAILED(hr)) return false;

    // CacheRequest to batch fetch all needed properties in a SINGLE out-of-process IPC call
    hr = m_pUIA->CreateCacheRequest(&m_pTabCacheRequest);
    if (FAILED(hr)) return false;

    m_pTabCacheRequest->AddProperty(UIA_NamePropertyId);
    m_pTabCacheRequest->AddProperty(UIA_ControlTypePropertyId);
    m_pTabCacheRequest->AddProperty(UIA_BoundingRectanglePropertyId);
    m_pTabCacheRequest->AddProperty(UIA_SelectionItemIsSelectedPropertyId);
    m_pTabCacheRequest->AddPattern(UIA_SelectionItemPatternId);
    m_pTabCacheRequest->AddPattern(UIA_InvokePatternId);
    m_pTabCacheRequest->put_TreeScope(TreeScope_Descendants);

    return true;
}

void TabExtractor::UninitUIA() {
    m_pTabCacheRequest.Release();
    m_pTabItemCondition.Release();
    m_pUIA.Release();
}

void TabExtractor::WorkerLoop() {
    // Initialize COM on this worker thread with MTA for responsiveness
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return;
    }

    if (!InitUIA()) {
        CoUninitialize();
        return;
    }

    while (m_running.load()) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            // Scan periodically every 1500ms or immediately when triggered (e.g., Alt-Tab keypress)
            m_cv.wait_for(lock, std::chrono::milliseconds(1500), [this]() {
                return !m_running.load() || m_scanRequested.load();
            });
        }

        if (!m_running.load()) break;

        m_scanRequested.store(false);

        auto tabs = ScanAllChromiumTabs();
        if (m_callback) {
            m_callback(std::move(tabs));
        }
    }

    UninitUIA();
    CoUninitialize();
}

bool TabExtractor::IsChromiumBrowserWindow(HWND hwnd, BrowserType& outType) {
    outType = BrowserType::Unknown;

    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    // Check if cloaked by DWM (virtual desktop or hidden)
    int cloakedVal = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloakedVal, sizeof(cloakedVal))) && cloakedVal) {
        return false;
    }

    // Chromium top-level windows use the class name "Chrome_WidgetWin_1"
    wchar_t className[256] = { 0 };
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className))) {
        return false;
    }

    if (wcscmp(className, L"Chrome_WidgetWin_1") != 0) {
        return false;
    }

    // Filter out tooltips, off-screen windows, and tiny extension popup windows
    RECT rc;
    if (!GetWindowRect(hwnd, &rc) || (rc.right - rc.left < 300) || (rc.bottom - rc.top < 200)) {
        return false;
    }

    // Check process name
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return false;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;

    wchar_t exePath[MAX_PATH] = { 0 };
    DWORD size = ARRAYSIZE(exePath);
    QueryFullProcessImageNameW(hProc, 0, exePath, &size);
    CloseHandle(hProc);

    std::wstring pathStr(exePath);
    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::towlower);

    if (pathStr.find(L"chrome.exe") != std::wstring::npos) {
        outType = BrowserType::Chrome;
        return true;
    } else if (pathStr.find(L"brave.exe") != std::wstring::npos) {
        outType = BrowserType::Brave;
        return true;
    } else if (pathStr.find(L"msedge.exe") != std::wstring::npos) {
        outType = BrowserType::Edge;
        return true;
    } else if (pathStr.find(L"vivaldi.exe") != std::wstring::npos) {
        outType = BrowserType::Vivaldi;
        return true;
    } else if (pathStr.find(L"opera.exe") != std::wstring::npos) {
        outType = BrowserType::Opera;
        return true;
    }

    return false;
}

std::vector<HWND> TabExtractor::EnumChromiumWindows() {
    std::vector<HWND> results;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* pVec = reinterpret_cast<std::vector<HWND>*>(lParam);
        BrowserType type;
        if (IsChromiumBrowserWindow(hwnd, type)) {
            pVec->push_back(hwnd);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&results));

    return results;
}

std::wstring TabExtractor::ExtractOmniboxUrl(IUIAutomation* pUIA, IUIAutomationElement* pRootElement) {
    if (!pUIA || !pRootElement) return L"";

    // Omnibox in Chromium has ControlType = Edit and typically Name "Address and search bar"
    VARIANT varEdit;
    varEdit.vt = VT_I4;
    varEdit.lVal = UIA_EditControlTypeId;

    CComPtr<IUIAutomationCondition> pEditCond;
    if (FAILED(pUIA->CreatePropertyCondition(UIA_ControlTypePropertyId, varEdit, &pEditCond))) {
        return L"";
    }

    CComPtr<IUIAutomationElement> pOmnibox;
    if (SUCCEEDED(pRootElement->FindFirst(TreeScope_Descendants, pEditCond, &pOmnibox)) && pOmnibox) {
        // Try ValuePattern first
        CComPtr<IUIAutomationValuePattern> pValPattern;
        if (SUCCEEDED(pOmnibox->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern, reinterpret_cast<void**>(&pValPattern))) && pValPattern) {
            BSTR bstrVal = nullptr;
            if (SUCCEEDED(pValPattern->get_CurrentValue(&bstrVal)) && bstrVal) {
                std::wstring val(bstrVal);
                SysFreeString(bstrVal);
                if (!val.empty()) return val;
            }
        }

        // Fallback to Name
        BSTR bstrName = nullptr;
        if (SUCCEEDED(pOmnibox->get_CurrentName(&bstrName)) && bstrName) {
            std::wstring name(bstrName);
            SysFreeString(bstrName);
            return name;
        }
    }

    return L"";
}

HICON TabExtractor::ExtractBrowserFavicon(HWND hwnd, const std::wstring& domain) {
    // 1. Try WM_GETICON
    HICON hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);
    if (!hIcon) {
        hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0);
    }
    if (!hIcon) {
        hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    }
    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    }

    if (hIcon) {
        return CopyIcon(hIcon);
    }

    return nullptr;
}

std::vector<std::shared_ptr<TabInfo>> TabExtractor::ScanWindowTabs(HWND hwnd, BrowserType browserType) {
    std::vector<std::shared_ptr<TabInfo>> tabs;

    if (!m_pUIA || !m_pTabItemCondition || !m_pTabCacheRequest) {
        return tabs;
    }

    CComPtr<IUIAutomationElement> pWindowElem;
    HRESULT hr = m_pUIA->ElementFromHandle(hwnd, &pWindowElem);
    if (FAILED(hr) || !pWindowElem) {
        return tabs;
    }

    // Extract Omnibox URL to assign to the active tab
    std::wstring activeUrl = ExtractOmniboxUrl(m_pUIA, pWindowElem);

    // High performance batch query: FindAllBuildCache retrieves all tabs + cached properties in 1 LRPC call
    CComPtr<IUIAutomationElementArray> pElemArray;
    hr = pWindowElem->FindAllBuildCache(TreeScope_Descendants, m_pTabItemCondition, m_pTabCacheRequest, &pElemArray);
    if (FAILED(hr) || !pElemArray) {
        return tabs;
    }

    int count = 0;
    pElemArray->get_Length(&count);

    for (int i = 0; i < count; ++i) {
        CComPtr<IUIAutomationElement> pTabElem;
        if (FAILED(pElemArray->GetElement(i, &pTabElem)) || !pTabElem) {
            continue;
        }

        auto tab = std::make_shared<TabInfo>();
        tab->browserHwnd = hwnd;
        tab->uiaElement = pTabElem;

        // Retrieve cached tab title
        BSTR bstrName = nullptr;
        if (SUCCEEDED(pTabElem->get_CachedName(&bstrName)) && bstrName) {
            tab->title = bstrName;
            SysFreeString(bstrName);
        }

        // Filter out empty or close button tabs
        if (tab->title.empty() || tab->title == L"Close") {
            continue;
        }

        // Retrieve cached selection status
        BOOL isSel = FALSE;
        CComPtr<IUIAutomationSelectionItemPattern> pSelPattern;
        if (SUCCEEDED(pTabElem->GetCachedPatternAs(
                UIA_SelectionItemPatternId, 
                IID_IUIAutomationSelectionItemPattern, 
                reinterpret_cast<void**>(&pSelPattern))) && pSelPattern) {
            pSelPattern->get_CachedIsSelected(&isSel);
        }
        tab->isSelected = (isSel == TRUE);

        // If this is the active selected tab, attach the extracted Omnibox URL
        if (tab->isSelected && !activeUrl.empty()) {
            tab->url = activeUrl;
        }

        // Retrieve cached bounding rectangle (used for fallback routing)
        RECT rc = { 0, 0, 0, 0 };
        pTabElem->get_CachedBoundingRectangle(&rc);
        tab->boundingRect = rc;

        // Extract favicon
        tab->hFavicon = ExtractBrowserFavicon(hwnd, L"");

        tabs.push_back(std::move(tab));
    }

    return tabs;
}

std::vector<std::shared_ptr<TabInfo>> TabExtractor::ScanAllChromiumTabs() {
    std::vector<std::shared_ptr<TabInfo>> allTabs;

    auto windows = EnumChromiumWindows();
    for (HWND hwnd : windows) {
        BrowserType type;
        if (IsChromiumBrowserWindow(hwnd, type)) {
            auto windowTabs = ScanWindowTabs(hwnd, type);
            allTabs.insert(allTabs.end(), 
                           std::make_move_iterator(windowTabs.begin()), 
                           std::make_move_iterator(windowTabs.end()));
        }
    }

    return allTabs;
}

} // namespace ChromiumAltTab

