# Security Policy

## Supported Versions

Only the latest release on the `main` branch receives security updates.

| Version | Supported          |
| ------- | ------------------ |
| 1.1.x   | :white_check_mark: |
| < 1.1.0 | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability within AltTabs, please do **not** open a public issue.

Instead, please send an advisory or report via [GitHub Security Advisories](https://github.com/Dszae/AltTabs/security/advisories/new) or directly contact the maintainer at `dszae` on GitHub.

### Security Architecture

AltTabs operates under strict security and privacy principles:
- **Zero Network Access**: The mod does not request or link any networking libraries (`ws2_32`, `wininet`, `winhttp`).
- **No Page DOM Inspection**: Operates exclusively via Windows UI Automation on top-level window frames.
- **Isolated Process Execution**: Operates inside a dedicated Windhawk tool process, preventing cross-process injection vulnerabilities.

