#pragma once

#include <windows.h>
#include <uiautomation.h>
#include <psapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <atlbase.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <regex>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

namespace ChromiumAltTab {

// Represents an extracted Chromium browser tab
struct TabInfo {
    std::wstring title;
    std::wstring url;
    std::wstring domain;
    HWND browserHwnd = nullptr;
    RECT boundingRect = { 0, 0, 0, 0 };
    bool isSelected = false;
    
    // UI Automation reference for activation routing
    CComPtr<IUIAutomationElement> uiaElement;
    
    // Cached icon handle (if extracted)
    HICON hFavicon = nullptr;

    TabInfo() = default;
    ~TabInfo() {
        if (hFavicon) {
            DestroyIcon(hFavicon);
            hFavicon = nullptr;
        }
    }

    // Move semantics to properly manage HICON ownership
    TabInfo(TabInfo&& other) noexcept 
        : title(std::move(other.title)),
          url(std::move(other.url)),
          domain(std::move(other.domain)),
          browserHwnd(other.browserHwnd),
          boundingRect(other.boundingRect),
          isSelected(other.isSelected),
          uiaElement(std::move(other.uiaElement)),
          hFavicon(other.hFavicon) {
        other.hFavicon = nullptr;
    }

    TabInfo& operator=(TabInfo&& other) noexcept {
        if (this != &other) {
            if (hFavicon) {
                DestroyIcon(hFavicon);
            }
            title = std::move(other.title);
            url = std::move(other.url);
            domain = std::move(other.domain);
            browserHwnd = other.browserHwnd;
            boundingRect = other.boundingRect;
            isSelected = other.isSelected;
            uiaElement = std::move(other.uiaElement);
            hFavicon = other.hFavicon;
            other.hFavicon = nullptr;
        }
        return *this;
    }

    // Disable copy
    TabInfo(const TabInfo&) = delete;
    TabInfo& operator=(const TabInfo&) = delete;
};

// Represents a cluster of tabs belonging to the same root domain
struct DomainGroup {
    std::wstring domain;
    std::vector<std::shared_ptr<TabInfo>> tabs;
    HWND proxyHwnd = nullptr;
    HICON groupIcon = nullptr;
    size_t activeTabIndex = 0;

    DomainGroup() = default;
    ~DomainGroup() {
        if (groupIcon) {
            DestroyIcon(groupIcon);
            groupIcon = nullptr;
        }
    }
};

// Supported browser identifiers
enum class BrowserType {
    Unknown,
    Chrome,
    Brave,
    Edge,
    Opera,
    Vivaldi
};

struct BrowserProcessInfo {
    DWORD processId;
    HWND topHwnd;
    BrowserType type;
    std::wstring executableName;
};

} // namespace ChromiumAltTab

