#pragma once

#include "Common.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>

namespace ChromiumAltTab {

class DomainGrouper {
public:
    DomainGrouper();
    ~DomainGrouper() = default;

    // Normalizes and extracts root domain from URL or tab title
    std::wstring ExtractDomain(const std::wstring& url, const std::wstring& title, BrowserType browserType);

    // Groups a flat list of extracted tabs by root domain
    std::unordered_map<std::wstring, std::shared_ptr<DomainGroup>> GroupTabs(
        std::vector<std::shared_ptr<TabInfo>>& flatTabs
    );

    // Strips browser brand suffixes (e.g., " - Google Chrome", " - Brave")
    std::wstring CleanTabTitle(const std::wstring& rawTitle, BrowserType browserType);

private:
    std::wstring ExtractDomainFromUrl(const std::wstring& url);
    std::wstring ExtractDomainFromTitle(const std::wstring& cleanTitle);
    std::wstring GetRootDomain(const std::wstring& host);

    // Precompiled regex patterns for speed
    std::wregex m_urlDomainRegex;
    std::wregex m_titleSeparatorRegex;
    std::vector<std::pair<std::wstring, std::wstring>> m_serviceDomainMap;
};

} // namespace ChromiumAltTab

