# Third-party notices

Solomon is licensed under the [GNU AGPL v3.0](LICENSE). The distributed plug-in binaries also contain the
following third-party components. Versions are pinned in `CMakeLists.txt` (`SOLOMON_JUCE_TAG`).

| Component | Version | License | Used for |
| :--- | :--- | :--- | :--- |
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.15 | [AGPLv3](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md) (or commercial) | Plug-in framework, UI |
| [VST 3 SDK](https://github.com/steinbergmedia/vst3sdk) (bundled in JUCE) | 3.8.0 | [MIT](https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt) | VST 3 plug-in format |
| [AudioUnitSDK](https://github.com/apple/AudioUnitSDK) (bundled in JUCE, macOS only) | as bundled | Apache 2.0 | AU plug-in format |
| [HarfBuzz](https://github.com/harfbuzz/harfbuzz) (bundled in JUCE) | as bundled | Old MIT | Text shaping |
| [SheenBidi](https://github.com/Tehreer/SheenBidi) (bundled in JUCE) | as bundled | Apache 2.0 | Bidirectional text |
| [libpng](http://www.libpng.org/) (bundled in JUCE) | as bundled | libpng/zlib | PNG images |
| [IJG libjpeg](https://ijg.org/) (bundled in JUCE) | as bundled | IJG License | JPEG images |
| [zlib](https://zlib.net/) (bundled in JUCE) | as bundled | zlib | Compression |

The full license texts are included in the JUCE source tree (`LICENSE.md` and the linked files) that CMake
downloads into `build/_deps/juce-src`.

## Trademarks

VST is a registered trademark of Steinberg Media Technologies GmbH. The VST Compatible Logo in
`docs/images/` is provided by Steinberg and is used unaltered under the
[Steinberg VST Usage Guidelines](https://github.com/steinbergmedia/vst3sdk/blob/master/VST3_Usage_Guidelines.pdf).
Audio Units is a trademark of Apple Inc.
