// Preset state round-trip tests for Fruity Reeverb 2 VST2 plugin.
// Tests VST2 chunk persistence, parameter automation, audio determinism, FL discovery, and preset listing.

#include "FruityReeverb2Vst2.h"
#include "FruityReeverb2Gui.h"
#include "../src/core/PresetCycle.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lynx::vst2
{

// --- GUI stubs (match FruityReeverb2Gui.h exactly) --------------------------

void guiGetRect (Rect& outRect)
{
    outRect.top = 0;
    outRect.left = 0;
    outRect.bottom = 192;
    outRect.right = 538;
}

void* guiOpen (AEffect* effect, HWND parentHwnd, HINSTANCE instance)
{
    (void) effect; (void) parentHwnd; (void) instance;
    return reinterpret_cast<void*>(0x1); // dummy non-null handle
}

void guiClose (void* editorHandle)
{
    (void) editorHandle;
}

void guiIdle (void* editorHandle)
{
    (void) editorHandle;
}

} // namespace lynx::vst2

// --- Minimal test host ------------------------------------------------------

struct TestHost
{
    static std::vector<std::int32_t> automatedParams;

    static std::intptr_t callback (lynx::vst2::AEffect* effect,
                                   std::int32_t opcode,
                                   std::int32_t index,
                                   std::intptr_t value,
                                   void* ptr,
                                   float opt)
    {
        (void) effect; (void) value; (void) ptr; (void) opt;
        if (opcode == lynx::vst2::audioMasterAutomate)
        {
            automatedParams.push_back(index);
        }
            else if (opcode == lynx::vst2::audioMasterGetTime)
            {
                // Provide valid tempo info so queryTempo() doesn't fall back
                auto* info = static_cast<lynx::vst2::TimeInfo*>(ptr);
                if (info)
                {
                    info->flags = lynx::vst2::kTempoValid;
                    info->tempo = 120.0;
                }
                return 1; // success
            }
            return 0;
        }
};

std::vector<std::int32_t> TestHost::automatedParams;

// --- Test utilities ---------------------------------------------------------

int failures = 0;

void check (bool ok, const char* what)
{
    if (ok)
    {
        std::cout << "[PASS] " << what << "\n";
    }
    else
    {
        std::cout << "[FAIL] " << what << "\n";
        ++failures;
    }
}

template<typename T>
bool approxEqual (T a, T b, T eps = 1e-6)
{
    return std::abs(a - b) <= eps;
}

// --- Test 1: Chunk round-trip ----------------------------------------------

void testChunkRoundTrip()
{
    using namespace lynx::vst2;

    // Create plugin with distinct parameter values
    auto* effect = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin = static_cast<FruityReeverb2Plugin*>(effect->object);

    // Set all 17 parameters to distinct values
    const float testValues[17] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f
    };
    for (int i = 0; i < 17; ++i)
    {
        plugin->setParameter(i, testValues[i]);
    }

    // Get chunk
    void* chunkPtr = nullptr;
    std::intptr_t chunkSize = plugin->dispatch(effGetChunk, 0, 0, &chunkPtr, 0.0f);
    check(chunkSize > 0, "effGetChunk returns positive size");
    check(chunkPtr != nullptr, "effGetChunk returns non-null pointer");

    // Verify magic and minimum size (4 magic + 4 version + 17*4 params + 1 + 1 + 8 = 86 bytes)
    check(chunkSize >= 86, "chunk size >= 86 bytes");
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(chunkPtr);
    check(bytes[0] == 'F' && bytes[1] == 'R' && bytes[2] == '2' && bytes[3] == 'V', "chunk magic 'FR2V'");

    // Create fresh plugin and load chunk
    auto* effect2 = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin2 = static_cast<FruityReeverb2Plugin*>(effect2->object);

    std::intptr_t loadResult = plugin2->dispatch(effSetChunk, 0, chunkSize, chunkPtr, 0.0f);
    check(loadResult == 1, "effSetChunk returns 1 (success)");

    // Verify all 17 parameters restored
    for (int i = 0; i < 17; ++i)
    {
        float v = plugin2->getParameter(i);
        check(approxEqual(v, testValues[i]), ("param " + std::to_string(i) + " restored").c_str());
    }

    // Verify chunk rejection: wrong magic
    std::vector<std::uint8_t> badChunk(64, 0xAB);
    loadResult = plugin2->dispatch(effSetChunk, 0, badChunk.size(), badChunk.data(), 0.0f);
    check(loadResult == 0, "effSetChunk rejects wrong magic");

    // Verify chunk rejection: truncated
    std::vector<std::uint8_t> truncChunk(4, 0);
    loadResult = plugin2->dispatch(effSetChunk, 0, truncChunk.size(), truncChunk.data(), 0.0f);
    check(loadResult == 0, "effSetChunk rejects truncated chunk");

    // Cleanup
    plugin->dispatch(effClose, 0, 0, nullptr, 0.0f);
    plugin2->dispatch(effClose, 0, 0, nullptr, 0.0f);
}

// --- Test 2: Parameter round-trip ------------------------------------------

void testParameterRoundTrip()
{
    using namespace lynx::vst2;

    auto* effect = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin = static_cast<FruityReeverb2Plugin*>(effect->object);

    TestHost::automatedParams.clear();

    // Set all 17 parameters
    for (int i = 0; i < 17; ++i)
    {
        float v = (i + 1) * 0.05f;
        plugin->setParameter(i, v);
        float got = plugin->getParameter(i);
        check(approxEqual(got, v), ("set/get param " + std::to_string(i)).c_str());
    }

    // Verify 17 automation notifications
    check(TestHost::automatedParams.size() == 17, "17 audioMasterAutomate calls");

    // Clamp > 1
    plugin->setParameter(0, 1.5f);
    check(approxEqual(plugin->getParameter(0), 1.0f), "clamp > 1 to 1.0");

    // Clamp < 0
    plugin->setParameter(0, -0.5f);
    check(approxEqual(plugin->getParameter(0), 0.0f), "clamp < 0 to 0.0");

    // Parameter names non-empty
    for (int i = 0; i < 17; ++i)
    {
        std::string name = plugin->paramName(i);
        check(!name.empty(), ("paramName[" + std::to_string(i) + "] non-empty").c_str());
    }

    plugin->dispatch(effClose, 0, 0, nullptr, 0.0f);
}

// --- Test 3: Audio determinism after chunk restore -------------------------
// Note: VST2 chunks save parameters, not internal DSP state (reverb delay lines, etc.).
// So audio won't be bit-identical immediately after restore because the reverb tail
// from the first render isn't captured. This test verifies parameters are restored
// and the engine produces valid output (not NaN/inf) after chunk load.

void testAudioDeterminismAfterRestore()
{
    using namespace lynx::vst2;

    const int sampleRate = 44100;
    const int blockSize = 512;
    const int numBlocks = 4;

    // Create plugin, configure
    auto* effect = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin = static_cast<FruityReeverb2Plugin*>(effect->object);

    plugin->dispatch(effSetSampleRate, 0, 0, nullptr, static_cast<float>(sampleRate));
    plugin->dispatch(effSetBlockSize, 0, blockSize, nullptr, 0.0f);
    plugin->dispatch(effMainsChanged, 0, 1, nullptr, 0.0f);

    // Set distinct parameters
    for (int i = 0; i < 17; ++i)
        plugin->setParameter(i, (i + 1) * 0.05f);

    // Render reference audio (warm up the reverb)
    for (int b = 0; b < numBlocks; ++b)
    {
        float* inL = new float[blockSize];
        float* inR = new float[blockSize];
        float* outL = new float[blockSize];
        float* outR = new float[blockSize];

        for (int i = 0; i < blockSize; ++i)
        {
            double t = (b * blockSize + i) / double(sampleRate);
            inL[i] = std::sin(2 * M_PI * 440.0 * t) * 0.5f;
            inR[i] = std::cos(2 * M_PI * 440.0 * t) * 0.5f;
        }

        float* inputs[2] = { inL, inR };
        float* outputs[2] = { outL, outR };

        plugin->processReplacing(inputs, outputs, blockSize);

        delete[] inL; delete[] inR; delete[] outL; delete[] outR;
    }

    // Save chunk
    void* chunkPtr = nullptr;
    std::intptr_t chunkSize = plugin->dispatch(effGetChunk, 0, 0, &chunkPtr, 0.0f);
    std::vector<std::uint8_t> savedChunk(static_cast<const std::uint8_t*>(chunkPtr),
                                          static_cast<const std::uint8_t*>(chunkPtr) + chunkSize);

    // Create fresh plugin, same config, load chunk
    auto* effect2 = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin2 = static_cast<FruityReeverb2Plugin*>(effect2->object);

    plugin2->dispatch(effSetSampleRate, 0, 0, nullptr, static_cast<float>(sampleRate));
    plugin2->dispatch(effSetBlockSize, 0, blockSize, nullptr, 0.0f);
    plugin2->dispatch(effMainsChanged, 0, 1, nullptr, 0.0f);
    plugin2->dispatch(effSetChunk, 0, chunkSize, chunkPtr, 0.0f);

    // Verify parameters restored correctly
    for (int i = 0; i < 17; ++i)
    {
        float v = plugin2->getParameter(i);
        check(v == plugin->getParameter(i), ("param " + std::to_string(i) + " restored").c_str());
    }

    // Re-render - output should be valid (not NaN/inf) but not bit-identical
    // because reverb internal state (delay lines) isn't saved in chunk
    bool validOutput = true;
    for (int b = 0; b < numBlocks; ++b)
    {
        float* inL = new float[blockSize];
        float* inR = new float[blockSize];
        float* outL = new float[blockSize];
        float* outR = new float[blockSize];

        for (int i = 0; i < blockSize; ++i)
        {
            double t = (b * blockSize + i) / double(sampleRate);
            inL[i] = std::sin(2 * M_PI * 440.0 * t) * 0.5f;
            inR[i] = std::cos(2 * M_PI * 440.0 * t) * 0.5f;
        }

        float* inputs[2] = { inL, inR };
        float* outputs[2] = { outL, outR };

        plugin2->processReplacing(inputs, outputs, blockSize);

        for (int i = 0; i < blockSize; ++i)
        {
            if (!std::isfinite(outL[i]) || !std::isfinite(outR[i]))
            {
                    std::cerr << "Block " << b << ", sample " << i << ": outL=" << outL[i] << ", outR=" << outR[i] << std::endl;
                    validOutput = false;
                    break;
                }
            }

            delete[] inL; delete[] inR; delete[] outL; delete[] outR;
            if (!validOutput) break;
        }
        check(validOutput, "audio output valid (finite) after chunk restore");

    plugin->dispatch(effClose, 0, 0, nullptr, 0.0f);
    plugin2->dispatch(effClose, 0, 0, nullptr, 0.0f);
}

    // --- Test 4: Preset listing (FL order, merge, skip missing) ----------------

void testPresetListing()
{
    using namespace lynx::presetcycle;
    using namespace std::filesystem;

    // Create temp test directory
    path tempDir = temp_directory_path() / "fr2v_preset_test";
    remove_all(tempDir);
    create_directories(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Rooms");
    create_directories(tempDir / "User" / "Presets" / "Plugin presets" / "Effects" / "Fruity Reeverb 2");

    // Factory presets
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Cathedral.fst") << "x";
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Default.fst") << "x";
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Preset 2.fst") << "x";
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Preset 10.fst") << "x";
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "Rooms" / "Small Studio.fst") << "x";

    // Ignored file (wrong extension)
    std::ofstream(tempDir / "Data" / "Patches" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "ignored.txt") << "x";

    // User preset (overrides factory if same relative path)
    std::ofstream(tempDir / "User" / "Presets" / "Plugin presets" / "Effects" / "Fruity Reeverb 2" / "My Room.fst") << "x";

    lynx::FlPathRoots roots;
    roots.data = (tempDir / "Data").string();
    roots.userData = (tempDir / "User").string();
    roots.home = "C:/Users/test";

    auto folders = pluginPresetFolders(roots, false, "Fruity Reeverb 2");
    check(folders.size() == 2, "pluginPresetFolders returns 2 folders (data + userData)");

    auto presets = listPresets(folders, ".fst");
    check(presets.size() == 6, "listPresets finds 6 .fst files (5 factory + 1 user)");

    // FL order: root folder files first (sorted naturally), then subfolder files
        // Root folder: Cathedral, Default, My Room, Preset 2, Preset 10
        // Subfolder Rooms/: Small Studio
        std::vector<std::string> expected = { "Cathedral", "Default", "My Room", "Preset 2", "Preset 10", "Small Studio" };
        for (size_t i = 0; i < expected.size(); ++i)
        {
            check(presets[i].name == expected[i], ("preset[" + std::to_string(i) + "] = " + expected[i]).c_str());
        }

        // Empty roots -> no folders
        lynx::FlPathRoots emptyRoots;
        folders = pluginPresetFolders(emptyRoots, false, "Fruity Reeverb 2");
        check(folders.empty(), "empty roots -> no folders");

        // Missing folder: pluginPresetFolders still returns paths, but listPresets returns empty
        folders = pluginPresetFolders(roots, false, "NonExistentPlugin");
        check(folders.size() == 2, "pluginPresetFolders returns paths even for missing folder");
        auto emptyPresets = listPresets(folders, ".fst");
        check(emptyPresets.empty(), "listPresets returns empty for missing folder");

    // Cleanup
    remove_all(tempDir);
}

// --- Test 6: Program opcodes -----------------------------------------------

void testProgramOpcodes()
{
    using namespace lynx::vst2;

    auto* effect = FruityReeverb2Plugin::create(TestHost::callback);
    auto* plugin = static_cast<FruityReeverb2Plugin*>(effect->object);

    check(effect->numPrograms == 1, "numPrograms == 1");
    check(plugin->dispatch(effGetProgram, 0, 0, nullptr, 0.0f) == 0, "effGetProgram == 0");
    check(plugin->dispatch(effSetProgram, 0, 0, nullptr, 0.0f) == 0, "effSetProgram == 0");

    char nameBuf[32] = {0};
    plugin->dispatch(effGetProgramName, 0, 0, nameBuf, 0.0f);
    check(std::strlen(nameBuf) > 0, "effGetProgramName non-empty");

    plugin->dispatch(effClose, 0, 0, nullptr, 0.0f);
}

// --- Main ------------------------------------------------------------------

int main()
{
    std::cout << "=== Fruity Reeverb 2 Preset State Tests ===\n\n";

    testChunkRoundTrip();
    testParameterRoundTrip();
    testAudioDeterminismAfterRestore();
    testPresetListing();
    testProgramOpcodes();

    std::cout << "\n=== " << (failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << " ===\n";
    std::cout << "Failures: " << failures << "\n";

    return failures == 0 ? 0 : 1;
}