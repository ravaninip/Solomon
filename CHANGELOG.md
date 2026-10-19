# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- Knobs now have the same size everywhere (as large as their area allows, up to a maximum) instead of filling
  whatever space the panel left, which made the LFO knob much larger than the others. In a layout, a knob sits
  in the middle of its rectangle.

### Added
- Four-voice paraphonic synthesizer with 17 oscillator models, multi-mode filter, two ADSR envelopes,
  LFO and a 4 x 4 modulation matrix.
- Band-limited resampling from the 48 kHz core to any host sample rate.
- Automatable parameters, MIDI (notes, bend, mod wheel, aftertouch, sustain), resizable editor.
- Presets: 13 factory presets, user presets saved as files, previous/next buttons.
- Editor: control names that follow the selected oscillator model, live filter response curve, envelope
  shape views, voice lamps and output meter, live modulation source bars, bipolar matrix sliders.
- Colour themes: three built-in themes, a theme menu in the editor and user themes loaded from JSON files
  (strictly validated, readability-checked), designed with the separate Theme Studio.
- Layouts: every element of the editor can be moved, resized or hidden by a layout file made with the Theme
  Studio; a *Layout* menu, strict validation, and a command that lists all elements for the Studio.
- Vendored MIT-licensed oscillator code with a reproducible import script.
- Windows build: GCC-only attributes in the vendored oscillator code are removed on MSVC.
- Initial project structure.

[Unreleased]: https://github.com/ravaninip/Solomon/commits/main
