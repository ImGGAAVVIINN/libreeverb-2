# Standalone Fruity Reeverb 2 VST2 plugin
#
# Extracted from LYNX-SHARE (https://github.com/Ka1-S/LYNX-SHARE): this tree contains only
# the dependency closure of the FruityReeverb2 VST2 target, established with linker map
# analysis of the working LYNX build. src/ holds exactly the sources the plugin pulls in
# (the DSP/engine core plus the JUCE frontend the editor reuses); third_party/ holds the
# vendored libraries those sources use. JUCE itself is fetched, not vendored.

cmake_minimum_required(VERSION 3.22)
project(FruityReeverb2 VERSION 0.1.23 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# FP contraction stays off everywhere: the Reeverb 2 DSP is verified sample by sample
# against the original DLL and fusing a*b+c into an FMA changes results.
if(NOT MSVC)
    add_compile_options(-ffp-contract=off)
endif()

# ---------------------------------------------------------------------------
# JUCE 8.0.4 (GPLv3) — fetched, then patched for MinGW before it is configured
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
option(LYNX_PATCH_JUCE "Apply cmake/LynxPatchJuce.cmake to the JUCE sources" ON)

FetchContent_GetProperties(juce)
if(NOT juce_POPULATED)
    FetchContent_Populate(juce)
    if(LYNX_PATCH_JUCE)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LynxPatchJuce.cmake)
        lynx_patch_juce("${juce_SOURCE_DIR}")
    endif()
    add_subdirectory(${juce_SOURCE_DIR} ${juce_BINARY_DIR})
endif()

# ---------------------------------------------------------------------------
# lynx_core: the DSP + engine library the plugin's frontend uses. The file set is the
# linker-proven closure (208 members) of the FruityReeverb2 target; this tree contains
# no other core sources, so a recursive glob is the curated list.
file(GLOB_RECURSE LYNX_CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/flp/*.cpp)
add_library(lynx_core STATIC ${LYNX_CORE_SOURCES})
target_include_directories(lynx_core PUBLIC src third_party/sse2neon)

# ---------------------------------------------------------------------------
# Vendored third-party static libraries (same definitions as the LYNX root build)

# pocketpy 2.2 (MIT; third_party/pocketpy): a small embeddable Python 3 interpreter,
# sandboxed (no os module / files) and interruptible (the watchdog)
add_library(pocketpy STATIC third_party/pocketpy/pocketpy.c)
target_include_directories(pocketpy PUBLIC third_party/pocketpy)
target_compile_definitions(pocketpy PUBLIC PK_ENABLE_OS=0 PK_ENABLE_WATCHDOG=1)
set_target_properties(pocketpy PROPERTIES C_STANDARD 11)
target_compile_options(pocketpy PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>)
target_link_libraries(lynx_core PUBLIC pocketpy)

# Monocypher 4.0.2 (BSD-2 / CC0; third_party/monocypher): Argon2id, BLAKE2b and
# XChaCha20-Poly1305 for the live sessions' end-to-end encryption
add_library(monocypher STATIC third_party/monocypher/monocypher.c)
target_include_directories(monocypher PUBLIC third_party/monocypher)
target_compile_options(monocypher PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>)
target_link_libraries(lynx_core PUBLIC monocypher)

# LAME's libmp3lame (LGPL; third_party/lame, encoder only, configured by third_party/lame/src/config.h)
file(GLOB LAME_SOURCES third_party/lame/src/*.c)
add_library(lame_encoder STATIC ${LAME_SOURCES})
target_include_directories(lame_encoder PUBLIC third_party/lame/include PRIVATE third_party/lame/src)
target_compile_definitions(lame_encoder PRIVATE HAVE_CONFIG_H)
target_compile_options(lame_encoder PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>)
target_link_libraries(lynx_core PUBLIC lame_encoder)

# WavPack decoder (BSD; read-only) for the .wv samples in the FL packs
file(GLOB WAVPACK_SOURCES third_party/wavpack/src/*.c)
add_library(wavpack_decoder STATIC ${WAVPACK_SOURCES})
target_include_directories(wavpack_decoder PUBLIC third_party/wavpack/include PRIVATE third_party/wavpack/src)
target_compile_definitions(wavpack_decoder PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:HAVE___BUILTIN_CLZ=1>)
target_link_libraries(lynx_core PUBLIC wavpack_decoder)

# libogg + libvorbis decoder (BSD; third_party/vorbis, mdct.c modified as FL builds it) for
# FL's Ogg-in-WAV samples; no FMA contraction so the float operation order stays libvorbis'.
# Every symbol gets a lynx_ prefix (JUCE links its own copy with the same C names).
file(GLOB VORBIS_SOURCES third_party/vorbis/src/*.c)
list(FILTER VORBIS_SOURCES EXCLUDE REGEX "/lookup\\.c$") # (included by lsp.c)
add_library(vorbis_decoder STATIC ${VORBIS_SOURCES})
target_include_directories(vorbis_decoder PUBLIC third_party/vorbis/include PRIVATE third_party/vorbis/src)
target_compile_options(vorbis_decoder PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off -w>
    $<IF:$<C_COMPILER_ID:MSVC>,/FI${CMAKE_CURRENT_SOURCE_DIR}/third_party/vorbis/include/lynx_vorbis_prefix.h,-include${CMAKE_CURRENT_SOURCE_DIR}/third_party/vorbis/include/lynx_vorbis_prefix.h>)
target_link_libraries(lynx_core PUBLIC vorbis_decoder)

# ---------------------------------------------------------------------------
# The VST2 plugin
add_subdirectory(src/vst2)

# Tests
add_subdirectory(tests)
