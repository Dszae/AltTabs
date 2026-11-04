# Changelog

All notable changes to the **AltTabs** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
