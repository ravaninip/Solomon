# Security Policy

## Supported versions

| Version | Supported |
| :--- | :--- |
| Latest release | Yes |
| Older releases | No – please update |

## Reporting a vulnerability

Please **do not** open a public issue for security problems (for example a crafted host session or MIDI input that
crashes the host or corrupts memory).

Report it privately through GitHub's
[private vulnerability reporting](https://github.com/ravaninip/Solomon/security/advisories/new).
Include the affected version, your OS and host, the steps to reproduce and, if possible, a sample file.

## What happens next

| Step | Target time |
| :--- | :--- |
| Acknowledgement of your report | 7 days |
| Initial assessment and severity | 14 days |
| Fix released for confirmed issues | 90 days (sooner for critical issues) |

Fixes are published as a new release together with a
[GitHub Security Advisory](https://github.com/ravaninip/Solomon/security/advisories) that credits the reporter
(unless you prefer to stay anonymous). This process follows coordinated vulnerability disclosure as described in
ISO/IEC 29147 and the EU Cyber Resilience Act.

## Security design

- **No network access.** Solomon is built with `JUCE_WEB_BROWSER=0` and `JUCE_USE_CURL=0` and makes no
  network connections; it collects no telemetry or personal data.
- **Untrusted input is bounded.** Plug-in state loaded from a project file is size-limited, every field is
  range-checked, and note lists and positions are capped (see `engine::State::deserialize`). Tests cover hostile input,
  and the engine tests run under AddressSanitizer and UndefinedBehaviorSanitizer in CI.
- **Real-time safety.** The audio thread never blocks: it exchanges data with the UI through atomics
  (and, where needed, a lock-free FIFO or a try-lock).
- **File access.** The plug-in writes only to locations the user picks (and, if it offers drag-and-drop export,
  to `<system temp>/Solomon`). It never executes files.
- **Supply chain.** Dependencies are pinned (`SOLOMON_JUCE_TAG`), GitHub Actions are pinned to commit SHAs
  and kept up to date by Dependabot, and the workflow token is read-only by default. Third-party components
  are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
