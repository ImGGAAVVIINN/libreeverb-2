// Fruity Reeverb 2 VST2 plugin implementation.
// Wraps lynx::reeverb2::Engine as a standalone VST2 audio effect.

#include "FruityReeverb2Vst2.h"
#include "FruityReeverb2Gui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lynx::vst2
{

// Copy a parameter string into the host's fixed buffer. Hosts allocate only
// kVstMaxParamStrLen + 1 (== 9) bytes for effGetParamName/Label/Display, so writing more than
// 8 characters (plus the terminator at index 8) overflows the host's memory and can crash it
// during the plugin scan. Truncate to exactly 8 characters and always null-terminate at [8].
static void copyParamString (void* ptr, const char* text)
{
    if (!ptr)
        return;
    char* dst = static_cast<char*> (ptr);
    int i = 0;
    for (; i < kVstMaxParamStrLen && text[i] != '\0'; ++i)
        dst[i] = text[i];
    dst[i] = '\0';
}

// --- VST2 entry point -------------------------------------------------------

AEffect* FruityReeverb2Plugin::create(HostCallback hostCallback)
{
    // Make process DPI-aware so VST window renders at correct size
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        using FnSetDPI = BOOL (WINAPI*)();
        auto fn = (FnSetDPI)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (fn) fn();
    }
    auto* plugin = new FruityReeverb2Plugin(hostCallback);
    return &plugin->effect_;
}

FruityReeverb2Plugin::FruityReeverb2Plugin(HostCallback hostCallback)
    : host_(hostCallback)
{
    // Initialize AEffect structure
    std::memset(&effect_, 0, sizeof(effect_));
    effect_.magic = kEffectMagic;
    effect_.dispatcher = [](AEffect* eff, std::int32_t op, std::int32_t idx, std::intptr_t val, void* ptr, float opt) -> std::intptr_t {
        auto* self = static_cast<FruityReeverb2Plugin*>(eff->object);
        return self->dispatch(op, idx, val, ptr, opt);
    };
    effect_.processReplacing = [](AEffect* eff, float** ins, float** outs, std::int32_t frames) {
        auto* self = static_cast<FruityReeverb2Plugin*>(eff->object);
        self->processReplacing(ins, outs, frames);
    };
    effect_.setParameter = [](AEffect* eff, std::int32_t idx, float val) {
        auto* self = static_cast<FruityReeverb2Plugin*>(eff->object);
        self->setParameter(idx, val);
    };
    effect_.getParameter = [](AEffect* eff, std::int32_t idx) -> float {
        auto* self = static_cast<FruityReeverb2Plugin*>(eff->object);
        return self->getParameter(idx);
    };
    effect_.numPrograms = 1;
    effect_.numParams = kNumParams;
    effect_.numInputs = 2;
    effect_.numOutputs = 2;
    effect_.flags = kEffFlagsCanReplacing | kEffFlagsProgramChunks | kEffFlagsHasEditor;
    effect_.uniqueId = 0x46523256; // 'FR2V' - Fruity Reeverb 2 VST
    effect_.version = 1;
    effect_.object = this;

    // Initialize parameter values to FL's defaults, using the exact toParams mapping so the
    // normalised values match LYNX's own representation bit-for-bit.
    const reeverb2::Settings defaults;
    reeverb2::toParams (defaults, paramValues_);
    paramTargets_ = paramValues_;
    currentSettings_ = defaults;
}

FruityReeverb2Plugin::~FruityReeverb2Plugin()
{
    // Close the editor if the host forgot to send effEditClose, so the window and its state
    // do not leak. The editor only ever touches the plugin through the AEffect vtable, so this
    // is safe here (the object is still fully constructed).
    if (effect_.user)
    {
        guiClose (effect_.user);
        effect_.user = nullptr;
    }
}

// --- Dispatcher -------------------------------------------------------------

std::intptr_t FruityReeverb2Plugin::dispatch(std::int32_t opcode, std::int32_t index, std::intptr_t value, void* ptr, float opt)
{
    switch (opcode)
    {
        case effOpen:
            return 1;

        case effClose:
            delete this;
            return 0;

        case effSetProgram:
            // Only one program
            return 0;

        case effGetProgram:
            return 0;

        case effGetProgramName:
            if (ptr) std::strcpy(static_cast<char*>(ptr), "Default");
            return 1;

        case effGetParamName:
            copyParamString (ptr, paramName(index).c_str());
            return 1;

        case effGetParamLabel:
            copyParamString (ptr, paramLabel(index).c_str());
            return 1;

        case effGetParamDisplay:
            copyParamString (ptr, paramDisplay(index, paramValues_[index]).c_str());
            return 1;

        case effGetParamNormalized:
            // FL Studio (and other hosts) use this opcode to read the current normalised
            // parameter value for GUI display / automation snapping. Without it the host has
            // no way to discover per-parameter values and falls back to 0 — which is why all
            // wheels were staying at their bottom/rest position while sliders (which receive
            // direct setParameter events on drag) appeared to work.
            return static_cast<std::intptr_t>(static_cast<long> (getParameter (index) * 1000));

        case effSetSampleRate:
            prepare(static_cast<int>(opt), maxBlockSize_);
            return 1;

        case effSetBlockSize:
            // Reserve scratch buffers for the new block size and force a re-prepare on the next
            // block so the engine's internal buffers match (reserve only grows, so this is safe
            // to call repeatedly).
            maxBlockSize_ = static_cast<int>(value);
            engine_.reserve(sampleRate_, maxBlockSize_);
            prepared_ = false;
            return 1;

        case effMainsChanged:
            if (value != 0)
            {
                // Mains on: ensure buffers are reserved. The engine is (re)prepared on the first
                // process block (with FL's flush), matching Reeverb2Effect's prepare/reset/process
                // sequence, so no explicit flush is needed here.
                engine_.reserve(sampleRate_, maxBlockSize_);
                prepared_ = false;
            }
            return 1;

        case effGetEffectName:
            if (ptr) std::strcpy(static_cast<char*>(ptr), "Fruity Reeverb 2");
            return 1;

        case effGetVendorString:
            if (ptr) std::strcpy(static_cast<char*>(ptr), "LYNX-SHARE");
            return 1;

        case effGetProductString:
            if (ptr) std::strcpy(static_cast<char*>(ptr), "Fruity Reeverb 2");
            return 1;

        case effGetVendorVersion:
            return 1000; // 1.0.0

        case effCanDo:
            return canDo(static_cast<const char*>(ptr));

        case effGetVstVersion:
            return 2400; // VST 2.4

        case effGetPlugCategory:
            return kPlugCategoryEffect;

        case effGetChunk:
            buildChunk();
            if (ptr) *static_cast<void**>(ptr) = chunkData_.data();
            return static_cast<std::intptr_t>(chunkData_.size());

        case effSetChunk:
            if (value >= 0 && ptr)
                return loadChunk(ptr, value) ? 1 : 0;
            return 0;

        case effProcessEvents:
            // Not handling MIDI events for this effect
            return 0;

        case effStartProcess:
            return 1;

        case effStopProcess:
            return 1;

        case effEditGetRect:
            // Spec: [ptr] is an ERect** that RECEIVES a pointer to the editor rect. The rect must
            // live at a stable address (we store it in editRect_), not be copied into the host's
            // pointer variable. Copying the struct here made the host dereference garbage, so the
            // editor window never sized/opened correctly.
            guiGetRect (editRect_);
            if (ptr)
                *static_cast<Rect**> (ptr) = &editRect_;
            return 1;

        case effEditOpen:
        {
            // ptr is the parent HWND (host window). value is unused.
            void* editor = guiOpen (&effect_, static_cast<HWND> (ptr), GetModuleHandleA (nullptr));
            if (editor)
            {
                effect_.user = editor; // store editor handle in AEffect::user
                return 1;
            }
            return 0;
        }

        case effEditClose:
        {
            void* editor = effect_.user;
            if (editor)
            {
                guiClose (editor);
                effect_.user = nullptr;
            }
            return 0;
        }

        case effEditIdle:
        {
            void* editor = effect_.user;
            if (editor)
                guiIdle (editor);
            return 0;
        }

        default:
            return 0;
    }
}

// --- Audio Processing -------------------------------------------------------

void FruityReeverb2Plugin::processReplacing(float** inputs, float** outputs, std::int32_t frames)
{
    // Get input/output buffers (stereo)
    float* inL = inputs[0];
    float* inR = inputs[1];
    float* outL = outputs[0];
    float* outR = outputs[1];

    // Copy input to output first (in-place processing by engine)
    if (inL != outL) std::memcpy(outL, inL, frames * sizeof(float));
    if (inR != outR) std::memcpy(outR, inR, frames * sizeof(float));

    // Mirror LYNX's Reeverb2Effect::process (Reeverb2.cpp:991) exactly: rebuild the settings from
    // the current normalised params every block, query the host tempo, then prepare on the first
    // block and set() on every later block. Engine::set is a no-op when nothing changed, so this
    // is bit-identical to the reference while still tracking host tempo changes with no param move.
    currentSettings_ = reeverb2::fromParams (paramTargets_);
    double bpm = queryTempo();
    if (!prepared_)
    {
        engine_.prepare(sampleRate_, currentSettings_, bpm);
        prepared_ = true;
    }
    else
    {
        engine_.set(currentSettings_, bpm);
    }

    // Process through reverb engine (in-place). No denormal flushing here: LYNX's own
    // Reeverb2Effect::process (Reeverb2.cpp:991) calls engine.process directly, so matching it
    // exactly keeps the output bit-identical to the reference goldens.
    engine_.process(outL, outR, frames);
}

// --- Parameter Handling -----------------------------------------------------

void FruityReeverb2Plugin::setParameter(std::int32_t index, float value)
{
    if (index < 0 || index >= kNumParams) return;
    value = std::clamp(value, 0.0f, 1.0f);
    paramTargets_[index] = value;
    paramValues_[index] = value;
    // Notify host of automation change
    if (host_)
        host_(&effect_, audioMasterAutomate, index, 0, nullptr, value);
}

float FruityReeverb2Plugin::getParameter(std::int32_t index) const
{
    if (index < 0 || index >= kNumParams) return 0.0f;
    return paramValues_[index];
}

// --- Internal Helpers -------------------------------------------------------

void FruityReeverb2Plugin::prepare(int sampleRate, int maxBlock)
{
    sampleRate_ = std::max(1, sampleRate);
    maxBlockSize_ = std::max(1, maxBlock);

    // Reserve the engine's buffers for this rate/block size (off the audio thread), then require a
    // fresh engine.prepare() on the next process block — exactly like Reeverb2Effect::prepare which
    // reserves and clears its "ready" flag, deferring the actual prepare() to the first block.
    engine_.reserve(sampleRate_, maxBlockSize_);
    prepared_ = false;
}

double FruityReeverb2Plugin::queryTempo()
{
    // Do not assume the host supplies valid tempo: fall back to 120 BPM like the original effect
    // (bpm > 0 ? bpm : 120.0). Only trust the host value when kTempoValid is set and it is positive.
    TimeInfo timeInfo;
    getTimeInfo(&timeInfo);
    double bpm = (timeInfo.flags & kTempoValid) ? (double) timeInfo.tempo : 0.0;
    if (!(bpm > 0.0)) bpm = 120.0;
    currentBpm_ = bpm;
    return bpm;
}

void FruityReeverb2Plugin::getTimeInfo(TimeInfo* info)
{
    if (!host_ || !info) return;
    *info = TimeInfo {};
    host_(&effect_, audioMasterGetTime, 0, 0, info, 0.0f);
}

std::intptr_t FruityReeverb2Plugin::canDo(const char* text) const
{
    if (!text) return 0;
    if (std::strcmp(text, kCanDoReceiveVstTimeInfo) == 0) return 1;
    if (std::strcmp(text, kCanDoReceiveVstEvents) == 0) return 1;
    if (std::strcmp(text, kCanDoReceiveVstMidiEvents) == 0) return 1;
    if (std::strcmp(text, kCanDoSendVstTimeInfo) == 0) return 1;
    if (std::strcmp(text, kCanDoBypass) == 0) return 1;
    return 0;
}

void FruityReeverb2Plugin::buildChunk()
{
    // Chunk format: 'FR2V' + version (int32) + 17 params (float) + tempoPredelay (bool) + midSide (bool) + bpm (double)
    // Uses the exact FL normalised representation so state round-trips match LYNX's own chunk format.
    chunkData_.clear();
    chunkData_.reserve(4 + 4 + 17 * 4 + 1 + 1 + 8);

    auto write = [&](const void* data, size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        chunkData_.insert(chunkData_.end(), bytes, bytes + size);
    };

    const char magic[4] = { 'F', 'R', '2', 'V' };
    write(magic, 4);
    std::int32_t version = 1;
    write(&version, 4);
    for (int i = 0; i < kNumParams; ++i)
        write(&paramValues_[i], 4);
    char tp = currentSettings_.tempoPredelay ? 1 : 0;
    char ms = currentSettings_.midSide ? 1 : 0;
    write(&tp, 1);
    write(&ms, 1);
    write(&currentBpm_, 8);
}

bool FruityReeverb2Plugin::loadChunk(const void* data, std::intptr_t byteSize)
{
    if (byteSize < 4 + 4 + 17 * 4 + 1 + 1 + 8) return false;
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);

    // Check magic
    if (bytes[0] != 'F' || bytes[1] != 'R' || bytes[2] != '2' || bytes[3] != 'V') return false;
    bytes += 4;

    // Version
    std::int32_t version = 0;
    std::memcpy(&version, bytes, 4);
    bytes += 4;
    if (version != 1) return false;

    // Parameters
    for (int i = 0; i < kNumParams; ++i)
    {
        float v = 0;
        std::memcpy(&v, bytes, 4);
        bytes += 4;
        paramValues_[i] = std::clamp(v, 0.0f, 1.0f);
        paramTargets_[i] = paramValues_[i];
    }

    // Options
    currentSettings_.tempoPredelay = bytes[0] != 0;
    currentSettings_.midSide = bytes[1] != 0;
    bytes += 2;

    // BPM
    std::memcpy(&currentBpm_, bytes, 8);

    // Reconstruct settings from the loaded normalised params (exact FL mapping) and force the next
    // process block to re-prepare the engine with the restored state (like loading a preset).
    currentSettings_ = reeverb2::fromParams (paramTargets_);
    prepared_ = false;

    return true;
}

std::string FruityReeverb2Plugin::paramDisplay(int index, float normalized) const
{
    // Reuse LYNX's authoritative display formatting (Reeverb2.cpp: reeverb2ParamText) verbatim.
    return reeverb2ParamText (index, normalized);
}

std::string FruityReeverb2Plugin::paramLabel(int index) const
{
    switch (index)
    {
        case kParamLowCut: return "Hz";
        case kParamHighCut: return "kHz";
        case kParamHighDamping: return "kHz";
        case kParamPredelay: return "ms";
        case kParamDecay: return "s";
        case kParamBassMult: return "x";
        case kParamCrossover: return "Hz";
        case kParamDry: case kParamEarlyRef: case kParamWet: return "%";
        case kParamModSpeed: case kParamModDepth: return "%";
        default: return "";
    }
}

std::string FruityReeverb2Plugin::paramName(int index) const
{
    switch (index)
    {
        case kParamLowCut: return "Low cut";
        case kParamHighCut: return "High cut";
        case kParamPredelay: return "Predelay";
        case kParamRoomSize: return "Room size";
        case kParamDiffusion: return "Diffusion";
        case kParamDecay: return "Decay";
        case kParamHighDamping: return "High damping";
        case kParamBassMult: return "Bass mult.";
        case kParamCrossover: return "Crossover";
        case kParamStereoSep: return "Stereo sep.";
        case kParamDry: return "Dry";
        case kParamEarlyRef: return "Early ref.";
        case kParamWet: return "Wet";
        case kParamModSpeed: return "Mod speed";
        case kParamModDepth: return "Mod depth";
        case kParamTempoPredelay: return "Tempo predelay";
        case kParamMidSide: return "Mid/side input";
        default: return "Param";
    }
}

} // namespace lynx::vst2

// --- Exported VST2 entry points --------------------------------------------

extern "C" {

#if defined(_WIN32)
    __declspec(dllexport)
#endif
lynx::vst2::AEffect* VSTPluginMain (lynx::vst2::HostCallback hostCallback)
{
    return lynx::vst2::FruityReeverb2Plugin::create (hostCallback);
}

// Some older hosts expect 'main' as entry point; use a different name to avoid
// conflict with C++ standard main(). VSTPluginMain is the standard entry point.
#if defined(_WIN32)
    __declspec(dllexport)
#endif
lynx::vst2::AEffect* VSTPluginMain_alias (lynx::vst2::HostCallback hostCallback)
{
    return lynx::vst2::FruityReeverb2Plugin::create (hostCallback);
}

} // extern "C"