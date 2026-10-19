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
- **Untrusted input is bounded.** A project's saved state is checked before it is parsed (size limit and
  a limit on the number of XML tags, because the parser is recursive), restored through the host-parameter
  system, and every value is clamped again (non-finite values replaced) before it reaches the audio engine
  (`Params::sanitize`). Tests cover oversized, deeply nested and out-of-range states, and the engine tests
  run under AddressSanitizer and UndefinedBehaviorSanitizer in CI.
- **Real-time safety.** The audio thread never blocks: it exchanges data with the UI through atomics
  (and, where needed, a lock-free FIFO or a try-lock).
- **File access.** Session data is handled by the host. The only files the plug-in touches are user presets
  (`*.solomonpreset`) in its own folder (`%APPDATA%\Solomon\Presets`, `~/Library/Application Support/Solomon/Presets`,
  `~/.config/Solomon/Presets`): they are written only there, with file names cleaned so a name can never point
  outside the folder, and every preset file is read through the same size, structure and range checks as a host
  session. It never executes files.
- **Theme files.** Colour themes (`*.json`) are read from `Themes` next to the presets folder. They are treated as
  untrusted input like presets: at most 8 KiB, a strict format with a version number, every colour validated, a
  nesting limit before parsing, at most 100 files, symbolic links ignored. A theme whose text would be unreadable
  on its own panels is refused, so a theme can never hide the controls needed to switch it. The chosen theme is
  remembered in a one-line text file next to the themes folder.
- **Layout files.** Layouts (`*.json` in `Layouts`, next to the themes) are untrusted input as well: at most 32 KiB,
  strict format and version, the plug-in name and window size must match, element ids restricted to a safe
  alphabet, at most 256 elements and 100 files, every rectangle clamped into the window, symbolic links ignored.
  The theme and layout menus cannot be hidden by a layout and are always on top.
- **Supply chain.** Dependencies are pinned (`SOLOMON_JUCE_TAG`; the vendored oscillator code is pinned by commit
  in `tools/vendor_mutable.sh` and each file keeps its license header), GitHub Actions are pinned to commit SHAs
  and kept up to date by Dependabot, and the workflow token is read-only by default. Third-party components
  are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
