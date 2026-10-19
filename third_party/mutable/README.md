# third_party/mutable

Oscillator code from the open-source firmware of Mutable Instruments' *Plaits* module, together with
the `stmlib` helper library it depends on. Both are © Emilie Gillet and released under the **MIT
license**; every vendored file keeps its original license header, and `stmlib/LICENSE` is included.

Solomon is an independent project and is not affiliated with or endorsed by Mutable Instruments.
The "Mutable Instruments" and "Plaits" names are not used for anything in the plug-in.

| Upstream | Commit |
|---|---|
| https://github.com/pichenettes/eurorack (`plaits/`) | see `tools/vendor_mutable.sh` |
| https://github.com/pichenettes/stmlib | see `tools/vendor_mutable.sh` |

## What is included

Only the files listed in `MANIFEST.txt` (the dependency closure of
`Source/Engine/PlaitsOscillator.cc`). Deliberately **not** included:

- the six-operator FM models (they need external patch banks),
- the speech model (its word and phoneme data has unclear origin),
- the drum models, the module firmware, UI and bootloader code,
- Plaits' desktop test program, which is GPL licensed.

## Updating or re-creating

`tools/vendor_mutable.sh` fetches the pinned upstream commits and copies exactly the manifest files.
To change the set of models, edit `Source/Engine/PlaitsOscillator.cc` and regenerate `MANIFEST.txt`
(dependency closure of headers and matching `.cc` files), then run the script.

## Local modification

`stmlib/utils/random.h` and `random.cc`: the generator state is `thread_local`. Upstream keeps it in
a global, which would be shared (and raced on) by plug-in instances running on different audio threads.
The script applies this change automatically.

## Build note

stmlib chooses between ARM-specific code and portable code with the `TEST` macro; the CMake build
defines it for these sources and for `PlaitsOscillator.cc` only.
