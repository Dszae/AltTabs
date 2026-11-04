#include "DomainGrouper.h"
#include <algorithm>

namespace ChromiumAltTab {

DomainGrouper::DomainGrouper() {
    // Regex to match protocols and extract hostname
    m_urlDomainRegex = std::wregex(LR"(^(?:https?:\/\/)?(?:www\.)?([a-zA-Z0-9_\-\.]+))", std::regex_constants::optimize);
    
    // Delimiters used in standard web page titles: " | ", " - ", " · ", " • "
    m_titleSeparatorRegex = std::wregex(LR"(\s+[\-–—\|·•]\s+)", std::regex_constants::optimize);

    // Well-known services mapping for accurate domain resolution when only tab title is available
    m_serviceDomainMap = {
        { L"github", L"github.com" },
        { L"gitlab", L"gitlab.com" },
        { L"stack overflow", L"stackoverflow.com" },
        { L"superuser", L"superuser.com" },
        { L"youtube", L"youtube.com" },
        { L"reddit", L"reddit.com" },
        { L"google search", L"google.com" },
        { L"google docs", L"docs.google.com" },
        { L"google sheets", L"sheets.google.com" },
        { L"gmail", L"mail.google.com" },
        { L"google drive", L"drive.google.com" },
        { L"twitter", L"x.com" },
        { L"x", L"x.com" },
        { L"linkedin", L"linkedin.com" },
        { L"wikipedia", L"wikipedia.org" },
        { L"microsoft learn", L"learn.microsoft.com" },
        { L"chatgpt", L"chatgpt.com" },
        { L"claude", L"claude.ai" },
        { L"notion", L"notion.so" },
        { L"figma", L"figma.com" },
        { L"amazon", L"amazon.com" }
    };
}

std::wstring DomainGrouper::CleanTabTitle(const std::wstring& rawTitle, BrowserType browserType) {
    std::wstring title = rawTitle;

    // Remove common Chromium browser title suffixes
    const std::wstring suffixes[] = {
        L" - Google Chrome",
        L" - Brave",
        L" - Chromium",
        L" - Microsoft Edge",
        L" - Opera",
        L" - Vivaldi"
    };

    for (const auto& suffix : suffixes) {
        if (title.length() >= suffix.length()) {
            if (title.compare(title.length() - suffix.length(), suffix.length(), suffix) == 0) {
                title = title.substr(0, title.length() - suffix.length());
                break;
            }
        }
    }

    return title;
}

std::wstring DomainGrouper::ExtractDomainFromUrl(const std::wstring& url) {
    if (url.empty()) return L"";

    std::wsmatch match;
    if (std::regex_search(url, match, m_urlDomainRegex) && match.size() > 1) {
        std::wstring host = match.str(1);
        std::transform(host.begin(), host.end(), host.begin(), ::towlower);
        return GetRootDomain(host);
    }
    return L"";
}

std::wstring DomainGrouper::GetRootDomain(const std::wstring& host) {
    if (host.empty()) return L"";

    // Handle localhost and IP addresses
    if (host == L"localhost" || host.rfind(L"127.", 0) == 0 || host.rfind(L"192.168.", 0) == 0) {
        return host;
    }

    // Known two-level TLDs
    const std::wstring twoLevelTlds[] = {
        L".co.uk", L".com.au", L".co.jp", L".co.in", L".gov.uk", L".org.uk", L".com.br"
    };

    for (const auto& tld : twoLevelTlds) {
        if (host.length() > tld.length() && 
            host.compare(host.length() - tld.length(), tld.length(), tld) == 0) {
            
            size_t prevDot = host.find_last_of(L'.', host.length() - tld.length() - 1);
            if (prevDot != std::wstring::npos) {
                return host.substr(prevDot + 1);
            }
            return host;
        }
    }

    // Default single TLD matching (e.g. sub.domain.com -> domain.com)
    size_t lastDot = host.find_last_of(L'.');
    if (lastDot == std::wstring::npos) return host;

    size_t secondLastDot = host.find_last_of(L'.', lastDot - 1);
    if (secondLastDot == std::wstring::npos) {
        return host; // e.g. "github.com"
    }

    return host.substr(secondLastDot + 1);
}

std::wstring DomainGrouper::ExtractDomainFromTitle(const std::wstring& cleanTitle) {
    if (cleanTitle.empty()) return L"other";

    std::wstring lower = cleanTitle;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // 1. Check known brand keywords
    for (const auto& [service, domain] : m_serviceDomainMap) {
        if (lower.find(service) != std::wstring::npos) {
            return domain;
        }
    }

    // 2. Search for embedded domain strings (e.g., "example.com")
    std::wregex domainPattern(LR"(\b([a-zA-Z0-9_\-]+\.(?:com|org|net|io|dev|ai|app|co|edu|gov))\b)");
    std::wsmatch match;
    if (std::regex_search(lower, match, domainPattern) && match.size() > 1) {
        return match.str(1);
    }

    // 3. Fallback: take the last token after separator (often site name: "Page Title - SiteName")
    std::wsregex_token_iterator iter(cleanTitle.begin(), cleanTitle.end(), m_titleSeparatorRegex, -1);
    std::wsregex_token_iterator end;
    std::wstring lastToken;
    while (iter != end) {
        if (!iter->str().empty()) {
            lastToken = iter->str();
        }
        ++iter;
    }

    if (!lastToken.empty() && lastToken.length() < 30) {
        std::wstring fallback = lastToken;
        std::transform(fallback.begin(), fallback.end(), fallback.begin(), ::towlower);
        // Strip whitespace
        fallback.erase(0, fallback.find_first_not_of(L" \t\r\n"));
        fallback.erase(fallback.find_last_not_of(L" \t\r\n") + 1);
        if (!fallback.empty()) {
            return fallback + L".com";
        }
    }

    return L"browsing";
}

std::wstring DomainGrouper::ExtractDomain(const std::wstring& url, const std::wstring& title, BrowserType browserType) {
    std::wstring domain = ExtractDomainFromUrl(url);
    if (!domain.empty()) {
        return domain;
    }

    std::wstring cleanTitle = CleanTabTitle(title, browserType);
    return ExtractDomainFromTitle(cleanTitle);
}

std::unordered_map<std::wstring, std::shared_ptr<DomainGroup>> DomainGrouper::GroupTabs(
    std::vector<std::shared_ptr<TabInfo>>& flatTabs
) {
    std::unordered_map<std::wstring, std::shared_ptr<DomainGroup>> groups;

    for (auto& tab : flatTabs) {
        if (!tab) continue;

        if (tab->domain.empty()) {
            tab->domain = ExtractDomain(tab->url, tab->title, BrowserType::Chrome);
        }

        auto it = groups.find(tab->domain);
        if (it == groups.end()) {
            auto newGroup = std::make_shared<DomainGroup>();
            newGroup->domain = tab->domain;
            newGroup->tabs.push_back(tab);
            if (tab->isSelected) {
                newGroup->activeTabIndex = 0;
            }
            groups[tab->domain] = newGroup;
        } else {
            it->second->tabs.push_back(tab);
            if (tab->isSelected) {
                it->second->activeTabIndex = it->second->tabs.size() - 1;
            }
        }
    }

    return groups;
}

} // namespace ChromiumAltTab

