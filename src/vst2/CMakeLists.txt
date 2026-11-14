# Fruity Reeverb 2 VST2 plugin target.
#
# The editor is LYNX's existing JUCE frontend (ui/PluginEditors.cpp, makeNativeEffectEditor):
# its transitive closure reaches most of the app's ui/engine/platform sources, so the plugin
# compiles the linker-proven closure of the app source set minus src/Main.cpp (its
# START_JUCE_APPLICATION entry point would collide with the plugin's DllMain). The frontend
# files themselves are reused unchanged. There is no fallback editor: JUCE is required.

# Compiler flags matching LYNX core. FP contraction stays off: the DSP is verified
# sample by sample against the original DLL and fusing a*b+c into an FMA changes results.
if(MSVC)
    add_compile_options(/W4 /permissive- /fp:strict /fp:except- /arch:AVX2)
    add_link_options(/DYNAMICBASE /NXCOMPAT /MACHINE:X64)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -ffp-contract=off -march=x86-64-v3)
    # Static libgcc/libstdc++/winpthread: the plugin must not need MinGW runtime DLLs on the
    # user's system (LMMS loads it from a plain Windows install with no MinGW present).
    add_link_options(-Wl,--no-undefined -static-libgcc -static-libstdc++)
endif()

set(EDITOR_SOURCES
    FruityReeverb2JuceEditor.cpp
    # platform
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/CrashGuard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/NativeEmbed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/Package.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/Paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/LinuxRealtime.cpp
    # engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/AudioLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/ChannelRackProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/ClapFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/DllAsioDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/Exporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/FlStudio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/Metronome.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/MidiFileIo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/MidiSyncIo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/Mp3Format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/PipeWireDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/PluginHost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/PluginSandbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/PreviewPlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/ProjectFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/SampleLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/StreamPrefetcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/Vst2Format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/WavPackFormat.cpp
    # ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/AudioEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/AudioEditorTools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/AutomationUi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/AutoSamplerDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/BrowserComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ChannelRackComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ChannelSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ClipProperties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ColourPicker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ControllerEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/Controllers164.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/DanceView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/Dialogs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/DynamicsDisplays.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/EditorTools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/EventEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ExportDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/FacetEqEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/FlExtraEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/HintBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/I18n.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/Icons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/LayerEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/LoopStarterWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/MidiOutEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/MixerComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/NewtoneEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ParametricEqEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PatcherEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PlaylistComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginHeader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginPicker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginPresetMenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginSkin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/PluginWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ProEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/QuickPick.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/SoundFontEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/SpeechDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/StemExtract.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/Theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ToolWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/TrackIcons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ValueControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/VfxEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/VideoPlayerView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/VisualEditors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/VisualGl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/VoltageEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/WaveformView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/WaveTravellerEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/WindowOwner.cpp)

# VST2 Plugin target
add_library(FruityReeverb2 MODULE
    FruityReeverb2Vst2.cpp
    ${EDITOR_SOURCES}
)

target_include_directories(FruityReeverb2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# App sources compiled into the plugin: onnxruntime_c_api.h (StemExtract.cpp, loaded at run
# time, never linked) and the CLAP ABI headers (ClapFormat.cpp).
target_include_directories(FruityReeverb2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/onnxruntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/clap/include
)
# onnxruntime_c_api.h spells the stdcall keyword the MSVC way (_stdcall); MinGW GCC only
# knows __stdcall (clang accepts both, so the zig app build needs no such shim). Scoped to
# the one translation unit that includes the header.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../ui/StemExtract.cpp PROPERTIES
    COMPILE_DEFINITIONS "_stdcall=__stdcall")

if(WIN32)
    target_link_libraries(FruityReeverb2 PRIVATE user32 gdi32 comctl32 msimg32)
    # Fully static MinGW runtime (libgcc, libstdc++, winpthread) so the DLL has no MinGW
    # runtime DLL dependencies. LMMS loads the plugin from a plain Windows install where
    # libwinpthread-1.dll / libstdc++-6.dll are not present.
    if(NOT MSVC)
        target_link_options(FruityReeverb2 PRIVATE -static)
    endif()
endif()

# JUCE editor: link the JUCE modules and the LYNX core the frontend uses.
target_link_libraries(FruityReeverb2 PRIVATE
    lynx_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_opengl)
# juce_opengl declares no Windows libs in its module metadata, so opengl32 must be
# linked explicitly (wgl* / gl* imports used by juce_opengl.cpp). ksguid provides the
# KSDATAFORMAT_SUBTYPE_* GUIDs juce_audio_devices' WASAPI code references (MSVC gets
# them from ksguid.lib; MinGW ships libksguid.a but does not link it by default).
if(WIN32)
    target_link_libraries(FruityReeverb2 PRIVATE opengl32 ksguid)
    # LiveLink.cpp's WebSocket transport (same as the LYNX app target)
    target_link_libraries(FruityReeverb2 PRIVATE winhttp)
endif()
target_compile_definitions(FruityReeverb2 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=0
    # App sources (ClapFormat, ...) embed the app version; mirrors project(FruityReeverb2 VERSION ...)
    # in the root CMakeLists.txt.
    JUCE_APPLICATION_VERSION_STRING="0.1.23")
if(WIN32)
    # DllAsioDevice.cpp includes the ASIO SDK headers (same as the LYNX app target)
    target_include_directories(FruityReeverb2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/asiosdk/common)
    # JUCE_ASIO stays 0: JUCE's own ASIO implementation uses MSVC-only SEH (__except) and
    # cannot compile with MinGW GCC. DllAsioDevice.cpp is guarded by JUCE_WINDOWS, not
    # JUCE_ASIO, and talks to the ASIO SDK directly, so it does not need JUCE_ASIO=1.
    # JUCE_USE_WINDOWS_MEDIA_FORMAT=0: the WMA codec needs NS_E_* constants missing from
    # MinGW's SDK headers; the plugin never reads WMA files.
    target_compile_definitions(FruityReeverb2 PRIVATE JUCE_ASIO=0 JUCE_USE_WINDOWS_MEDIA_FORMAT=0)
endif()

# Windows-specific settings for VST2 DLL
if(WIN32)
    target_compile_definitions(FruityReeverb2 PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        FRUITYREEVERB2_EXPORTS
    )
    # VST2 plugins export their entry point via __declspec(dllexport)
    # Output as .dll with VST2 naming convention
    set_target_properties(FruityReeverb2 PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
        OUTPUT_NAME "FruityReeverb2"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst2"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst2"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst2"
    )
endif()

# Installation
install(TARGETS FruityReeverb2
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
    ARCHIVE DESTINATION "."
)
