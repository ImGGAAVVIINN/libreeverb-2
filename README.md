# Fruity Reeverb 2 (standalone VST2)

A standalone 64-bit Windows VST2 audio effect extracted from
[LYNX-SHARE](https://github.com/Ka1-S/LYNX-SHARE)'s Fruity Reeverb 2 reconstruction.

The plugin reuses LYNX's existing Reeverb 2 DSP implementation unchanged (all 15 original
parameters, sample-rate-dependent behaviour, stereo processing and tempo-synchronised
predelay) and embeds LYNX's existing JUCE frontend (`makeNativeEffectEditor` in
`src/ui/PluginEditors.cpp`) as its editor — no hand-written replacement GUI. The editor
tracks LYNX's current orange Reeverb 2 theme: the frontend sources under `src/ui/`,
`src/core/` and their `.inc` assets are direct ports of the corresponding LYNX
implementation (upstream commit `efb8825`), not a re-skin.

## Requirements

- Windows x64
- CMake ≥ 3.22
- A C++20 compiler. Tested with MinGW-w64 GCC (UCRT) 16.2.0 + Ninja. MSVC should also work.
- Network access on the first configure (JUCE 8.0.4 is fetched via FetchContent)

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target FruityReeverb2
```

The plugin is produced at `build/vst2/FruityReeverb2.dll`.

On the first configure, JUCE 8.0.4 is downloaded into `build/_deps/` and patched by
`cmake/LynxPatchJuce.cmake` (MinGW compatibility fixes, including the juceaide
Common-Controls v6 manifest fix). The patch step is idempotent and can be disabled with
`-DLYNX_PATCH_JUCE=OFF`.

## Layout

- `src/vst2/` — the VST2 implementation: ABI wrapper, dispatcher, parameter handling,
  state persistence, and the JUCE editor glue that embeds LYNX's frontend.
- `src/core/`, `src/flp/` — the LYNX DSP/engine library (`lynx_core`), reduced to the
  linker-proven dependency closure of the plugin (Reeverb 2 DSP, effects, song/worker
  infrastructure, FL project parsing, and everything the frontend pulls in).
- `src/engine/`, `src/ui/`, `src/platform/` — the LYNX app sources the JUCE frontend
  requires (the closure of `makeNativeEffectEditor`), reused unchanged.
- `third_party/` — vendored libraries used by those sources (see
  `THIRD_PARTY_LICENSES.md`). JUCE itself is not vendored; it is fetched at configure time.

## License

LYNX-SHARE is licensed under the GNU AGPL-3.0; this extraction inherits it (see `LICENSE`).
Third-party components carry their own licenses, listed in `THIRD_PARTY_LICENSES.md`.
JUCE 8 is licensed under the GNU AGPL-3.0 (commercial licenses available from JUCE).
