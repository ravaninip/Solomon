# Contributing to Solomon

Thanks for your interest in Solomon! Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and requesting features

- Search [existing issues](https://github.com/ravaninip/Solomon/issues) first.
- Use the **Bug report** or **Feature request** template. For bugs, include your OS, DAW and version,
  the Solomon version and the steps to reproduce.
- For security problems, follow [SECURITY.md](SECURITY.md) instead of opening a public issue.

## Development setup

```bash
git clone https://github.com/ravaninip/Solomon.git
cd Solomon

# Engine only: fast, no JUCE download
cmake -B build-engine -DCHORDMATE_BUILD_PLUGIN=OFF
cmake --build build-engine --parallel
ctest --test-dir build-engine --output-on-failure

# Full plugin + host-less smoke test
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCHORDMATE_BUILD_PLUGIN_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Windows see [docs/BUILDING_WINDOWS.md](docs/BUILDING_WINDOWS.md).

## Code guidelines

- C++17, formatted like the surrounding code (see `.editorconfig`).
- Domain logic belongs in `Source/Engine/` and must not depend on JUCE, so it stays unit-testable.
- New engine behaviour needs tests in `Tests/engine_tests.cpp`.
- Keep the engine deterministic: the same inputs and seed must produce the same output.
- Treat anything loaded from a project or received as MIDI as untrusted: clamp values and bound sizes.
- Never allocate, lock or do file I/O on the audio thread (`processBlock`).
- Do not add network access or telemetry.
- When writing about VST, follow the Steinberg VST Usage Guidelines: write "VST® 3" on first use, never
  coin variants such as "VSTi", and keep the trademark attribution.
- Do not change `PLUGIN_MANUFACTURER_CODE` or `PLUGIN_CODE` in `CMakeLists.txt`: hosts would no longer
  recognise saved sessions.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change and describe what it does and why.
3. Make sure the build and all tests pass locally; CI builds and tests on Windows and macOS.
4. Add an entry under *Unreleased* in [CHANGELOG.md](CHANGELOG.md) for user-visible changes.

By contributing you agree that your contributions are licensed under the
[GNU AGPL v3.0](LICENSE), the license of this project.

## Releasing (maintainers)

1. Update `VERSION` in the `project()` call of `CMakeLists.txt` and move the *Unreleased* changelog entries
   under the new version.
2. Tag and push: `git tag v0.2.0 && git push origin v0.2.0`.
3. The **Build Solomon** workflow builds and tests on Windows and macOS and publishes a GitHub Release with
   the zipped plugins. A failing test blocks the release.
