#pragma once

// Fruity Reeverb 2 VST2 plugin wrapper.
// Wraps lynx::reeverb2::Engine as a standalone VST2 audio effect.

#include "Vst2Abi.h"
#include "../core/Reeverb2.h"
#include "../core/Denormals.h"

#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace lynx::vst2
{

// --- Parameter indices (matching reeverb2::Settings layout) -----------------
// Authoritative names from Effects.cpp:2703 and ranges from Reeverb2.cpp:19

enum ParamIndex : std::int32_t
{
    kParamLowCut        = 0,  // 19..3000 Hz
    kParamHighCut       = 1,  // 5..221 (0.5..22.1 kHz)
    kParamPredelay      = 2,  // 0..384 (ms / beats)
    kParamRoomSize      = 3,  // 1..100
    kParamDiffusion     = 4,  // 0..100
    kParamDecay         = 5,  // 1..200 (0.1..20.0 s)
    kParamHighDamping   = 6,  // 5..221 (0.5..22.1 kHz) - "High damping"
    kParamBassMult      = 7,  // 20..300 (x0.20..x3.00) - "Bass mult."
    kParamCrossover     = 8,  // 25..2000 Hz
    kParamStereoSep     = 9,  // -64..64
    kParamDry           = 10, // 0..160 (0..125%)
    kParamEarlyRef      = 11, // 0..160 (0..125%)
    kParamWet           = 12, // 0..160 (0..125%)
    kParamModSpeed      = 13, // 0..2048 (0..100%)
    kParamModDepth      = 14, // 0..2048 (0..100%)
    kParamTempoPredelay = 15, // 0/1 (off/on)
    kParamMidSide       = 16, // 0/1 (off/on)

    kNumParams          = 17
};

// --- VST2 plugin class ------------------------------------------------------

class FruityReeverb2Plugin
{
public:
    FruityReeverb2Plugin(HostCallback hostCallback);
    ~FruityReeverb2Plugin();

    // VST2 entry point
    static AEffect* create(HostCallback hostCallback);

    // Dispatcher handlers
    std::intptr_t dispatch(std::int32_t opcode, std::int32_t index, std::intptr_t value, void* ptr, float opt);
    void processReplacing(float** inputs, float** outputs, std::int32_t frames);
    void setParameter(std::int32_t index, float value);
    float getParameter(std::int32_t index) const;

    // Display helpers. Exposed so the bundled editor can show full (untruncated) names/values;
    // the VST2 dispatcher still clamps these to kVstMaxParamStrLen for the host.
    std::string paramDisplay(int index, float normalized) const;
    std::string paramLabel(int index) const;
    std::string paramName(int index) const;

    // Host tempo as of the last processed block (queryTempo's 120 fallback before audio starts).
    // Read by the JUCE editor glue for the frontend's tempo-synced predelay display.
    double hostBpm() const noexcept { return currentBpm_; }

private:
    // VST2 effect structure (must be first member; host holds a pointer to it)
    AEffect effect_;

    // Persistent editor rectangle. effEditGetRect hands the host a pointer to this (ERect**),
    // so it must outlive the call and stay at a stable address.
    Rect editRect_ {};

    // Host callback
    HostCallback host_ = nullptr;

    // DSP engine
    reeverb2::Engine engine_;
    reeverb2::Settings currentSettings_;
    double currentBpm_ = 120.0;
    int sampleRate_ = 44100;
    int maxBlockSize_ = 8192;
    bool prepared_ = false;

    // Parameter state in FL's own normalised representation (raw − min) / (max − min), stored in the
    // shared EffectParams layout so the exact reeverb2::fromParams/toParams mapping is reused verbatim.
    EffectParams paramValues_ {};
    EffectParams paramTargets_ {};

    // State chunk (program data)
    std::vector<std::uint8_t> chunkData_;

    // Helpers
    void prepare(int sampleRate, int maxBlock);
    double queryTempo();
    void getTimeInfo(TimeInfo* info);
    std::intptr_t canDo(const char* text) const;
    void buildChunk();
    bool loadChunk(const void* data, std::intptr_t byteSize);
};

} // namespace lynx::vst2