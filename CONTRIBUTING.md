# Contributing to AltTabs (Chromium in Windows Alt-Tab)

Thank you for your interest in making AltTabs better! We welcome contributions of all kinds: bug reports, PRs, domain classification additions, and translations.

## Development Setup

1. **Install Windhawk**: Download from [windhawk.net](https://windhawk.net).
2. **Clone this repository**:
   ```bash
   git clone https://github.com/Dszae/AltTabs.git
   ```
3. **Open Windhawk Mod Editor**:
   - Navigate to **Advanced** -> **Create New Mod**.
   - Paste the code from `ChromiumAltTabInjector.wh.cpp` or link your local workspace.
4. **Compile & Test**:
   - Use Windhawk's built-in compiler (GCC / Clang / MSVC).
   - Test with Google Chrome, Brave, Edge, and Vivaldi under heavy tab counts (50+ tabs) to verify memory and CPU remain at 0% idle.

## Code Standards
- Modern C++20 standard.
- Safe Win32 memory management (RAII wrappers for `HICON`, `BSTR`, and COM interfaces via `CComPtr`).
- Zero UI thread blocking: All UIA traversals must remain on the background MTA worker thread.
- Avoid forcing renderer accessibility in Chromium unless necessary.

## Submitting a Pull Request
1. Fork the repo and create your branch from `main`.
2. Ensure the code compiles cleanly with `/W3` or `-Wall`.
3. Open a PR with a clear summary of changes and before/after screenshots or GIFs.

