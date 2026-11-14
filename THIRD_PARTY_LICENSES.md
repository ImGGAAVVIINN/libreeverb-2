# Third-party components

This repository is an extraction of the Fruity Reeverb 2 VST2 plugin from
[LYNX-SHARE](https://github.com/Ka1-S/LYNX-SHARE) (GNU AGPL-3.0, see `LICENSE`). The
LYNX-derived sources under `src/` inherit the AGPL-3.0.

## Vendored in `third_party/`

| Component | Directory | License | Used by |
|---|---|---|---|
| pocketpy 2.2 | `third_party/pocketpy` | MIT | embedded Python (lynx_core) |
| Monocypher 4.0.2 | `third_party/monocypher` | BSD-2-Clause / CC0 | live-session crypto (lynx_core) |
| LAME (libmp3lame, encoder) | `third_party/lame` | LGPL-2.0+ | MP3 encoding (lynx_core) |
| WavPack (decoder) | `third_party/wavpack` | BSD-3-Clause | .wv sample decoding (lynx_core) |
| libogg + libvorbis | `third_party/vorbis` | BSD-3-Clause | Ogg-in-WAV sample decoding (lynx_core) |
| minimp3 / minimp3_ex | `third_party/minimp3` | CC0 / Unlicense | MP3 decoding (src/core/Mp3Decoder.cpp) |
| stb_vorbis | `third_party/stb_vorbis` | Public domain (Unlicense/MIT) | SoundFont sample decoding (src/core/SoundFontLoad.cpp) |
| TinySoundFont | `third_party/tinysoundfont` | MIT | SoundFont synthesis (src/core/SoundFontLoad.cpp) |
| Signalsmith Stretch | `third_party/signalsmith` | MIT | time stretching (src/core/TimeStretch.cpp) |
| sse2neon | `third_party/sse2neon` | MIT | SIMD intrinsics portability (src/core/Simd.h) |
| onnxruntime C API header | `third_party/onnxruntime` | MIT | StemExtract.cpp (header only, loaded at run time) |
| CLAP ABI headers | `third_party/clap` | MIT | CLAP plugin hosting (src/engine/ClapFormat.cpp) |
| ASIO SDK headers | `third_party/asiosdk` | Steinberg ASIO SDK licence | ASIO device support (src/engine/DllAsioDevice.cpp) |

License texts are included in each component's directory.

## Fetched at build time (not vendored)

| Component | Version | License |
|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.4 | GNU AGPL-3.0 (commercial licenses available from JUCE) |

JUCE is fetched via CMake FetchContent during configuration and patched by
`cmake/LynxPatchJuce.cmake` (MinGW compatibility fixes).
