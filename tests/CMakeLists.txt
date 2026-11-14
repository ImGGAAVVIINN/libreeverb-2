# Preset state round-trip test executable for Fruity Reeverb 2 VST2 plugin.
# Tests VST2 chunk persistence, parameter automation, audio determinism, FL discovery, and preset listing.

cmake_minimum_required(VERSION 3.22)

add_executable(PresetStateTest
    PresetStateTest.cpp
    ../src/vst2/FruityReeverb2Vst2.cpp
)

target_include_directories(PresetStateTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/vst2
)

target_link_libraries(PresetStateTest PRIVATE
    lynx_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_opengl
)

if(WIN32)
    target_link_libraries(PresetStateTest PRIVATE user32 gdi32 opengl32 ksguid winhttp)
    if(NOT MSVC)
        target_link_options(PresetStateTest PRIVATE -static)
    endif()
endif()

target_compile_definitions(PresetStateTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_APPLICATION_VERSION_STRING="0.1.23"
)

if(WIN32)
    target_compile_definitions(PresetStateTest PRIVATE
        WIN32
        _WINDOWS
        JUCE_ASIO=0
        JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    )
    target_include_directories(PresetStateTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/asiosdk/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/onnxruntime
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/clap/include
    )
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../ui/StemExtract.cpp PROPERTIES
        COMPILE_DEFINITIONS "_stdcall=__stdcall")
endif()

if(NOT MSVC)
    target_compile_options(PresetStateTest PRIVATE
        -Wall -Wextra -Wpedantic -ffp-contract=off -march=x86-64-v3
    )
else()
    target_compile_options(PresetStateTest PRIVATE
        /W4 /permissive- /fp:strict /fp:except- /arch:AVX2
    )
endif()