# Changelog

All notable changes to the **AltTabs** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-11-24

### Changed
- **Native Tab Strip Only**: Tabs are read from the browser's tab strip with a search that skips web page content. Previously, tab widgets inside web pages could show up as extra entries (e.g. 14 entries for a 5-tab window).
- **Browser-Only Hotkeys**: `Alt + Q` and ``Alt + ` `` are only taken over while an enabled browser is focused. `AltGr` (Ctrl+Alt) combinations are left alone, so e.g. `AltGr + Q` still types `@` on German layouts.
- **Site Labels**: The label next to a tab is the site name from the end of its title (e.g. `YouTube`), instead of a guessed `.com` domain.
- **Mod Settings**: "Group tabs by site" now works. Removed the Vivaldi setting, since its tab bar is a web page.

### Added
- Scrolling with the selection, `Page Up` / `Page Down`, the mouse wheel, and a scroll bar.
- `Shift + Alt + Q` cycles backwards, `Ctrl + Backspace` clears the filter.
- Multi-word filtering, a match count, and a marker for the current tab.
- DPI scaling, and keeping the switcher inside the browser's monitor.

### Fixed
- The keyboard hook runs on its own thread, so UI Automation calls can no longer delay keyboard input system-wide.
- `Alt + F2` no longer opens the switcher, and `Alt + \` isn't taken over anymore.
- Clicking a row in the lower part of the list selected the wrong tab.
- Clicking another app while the switcher was open tried to pull the focus back to the browser.
- `Alt + F4` destroyed the switcher window, after which the hotkeys stopped working.
- Titles containing `&` were drawn with an underlined letter.
- UI Automation objects were released after COM was uninitialized on exit.

## [1.1.1] - 2026-11-04

### Added
- **Dual Universal Shortcuts**: Support for both `Alt + Q` (universal on all physical keyboards without Shift) and `Alt + ` ` / `Alt + ~`.
- **Fallback Browser Card**: Added automatic window title fallback to ensure HUD always displays even before Chromium builds the accessibility tree.
- **Activation Guard**: Added 400ms dismissal protection against premature keyup / Alt-release focus transitions.
- **Comprehensive Documentation**: Complete overhaul of README, installation guides, architecture diagrams, and security policy.

### Fixed
- **Keyboard Hook Registration**: Replaced `GetModuleHandleW(nullptr)` with DLL module handle `HINST_THISCOMPONENT` (`&__ImageBase`), resolving hook installation rejections on Windows.
- **COM Initialization**: Initialized COM Apartment (`CoInitializeEx`) on dedicated Win32 worker thread, resolving `CO_E_NOTINITIALIZED` (`0x800401F0`) errors.
- **Hook Blocking**: Replaced synchronous UI calls in the low-level keyboard hook callback with non-blocking `PostMessageW`.

## [1.1.0] - 2026-11-04

### Added
- **Floating Tab Switcher HUD**: Dedicated Win32 dark-mode HUD window with Windows 11 rounded corners.
- **Real-Time Fuzzy Search**: Filter open tabs as you type.
- **Domain Tagging**: Automatic extraction and tagging of root website domains.
- **Direct Tab Activation**: Multi-tier pattern activation via `IUIAutomationSelectionItemPattern` and MSAA fallbacks.

## [1.0.0] - 2026-11-03

### Initial Release
- Proof of concept for Chromium tab switching in Windows.

