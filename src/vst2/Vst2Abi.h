#pragma once

// Clean-room VST 2.4 binary interface (ABI) definitions.
// Matches the exact layout used by LYNX's VST2 host (src/engine/Vst2Format.cpp) and test plugin
// (tools/testplugin/lynx_test_vst2.c). No Steinberg SDK headers are included or redistributed.

#include <cstdint>

namespace lynx::vst2
{

// --- Opcodes ----------------------------------------------------------------

// Effect dispatcher opcodes (plugin -> host calls these via dispatcher)
enum DispatcherOp : std::int32_t
{
    effOpen                 = 0,
    effClose                = 1,
    effSetProgram           = 2,
    effGetProgram           = 3,
    effSetProgramName       = 4,
    effGetProgramName       = 5,
    effGetParamLabel        = 6,
    effGetParamDisplay      = 7,
    effGetParamName         = 8,
    effGetParamNormalized   = 44,
    effSetSampleRate        = 10,
    effSetBlockSize         = 11,
    effMainsChanged         = 12,
    effEditGetRect          = 13,
    effEditOpen             = 14,
    effEditClose            = 15,
    effEditIdle             = 19,
    effGetChunk             = 23,
    effSetChunk             = 24,
    effProcessEvents        = 25,
    effGetProgramNameIndexed= 29,
    effGetPlugCategory      = 35,
    effGetEffectName        = 45,
    effGetVendorString      = 47,
    effGetProductString     = 48,
    effGetVendorVersion     = 49,
    effCanDo                = 51,
    effGetTailSize          = 52,
    effGetVstVersion        = 58,
    effBeginSetProgram      = 67,
    effEndSetProgram        = 68,
    effShellGetNextPlugin   = 70,
    effStartProcess         = 71,
    effStopProcess          = 72,
};

// Host callback opcodes (plugin -> host via audioMaster callback)
enum HostOp : std::int32_t
{
    audioMasterAutomate     = 0,
    audioMasterVersion      = 1,
    audioMasterCurrentId    = 2,
    audioMasterIdle         = 3,
    audioMasterWantMidi     = 6,
    audioMasterGetTime      = 7,
    audioMasterProcessEvents= 8,
    audioMasterIOChanged    = 13,
    audioMasterSizeWindow   = 15,
    audioMasterGetSampleRate= 16,
    audioMasterGetBlockSize = 17,
    audioMasterGetInputLatency = 18,
    audioMasterGetOutputLatency = 19,
    audioMasterGetCurrentProcessLevel = 23,
    audioMasterGetAutomationState = 24,
    audioMasterGetVendorString = 32,
    audioMasterGetProductString = 33,
    audioMasterGetVendorVersion = 34,
    audioMasterCanDo        = 37,
    audioMasterGetLanguage  = 38,
    audioMasterGetDirectory = 41,
    audioMasterUpdateDisplay = 42,
    audioMasterBeginEdit    = 43,
    audioMasterEndEdit      = 44,
};

// --- Flags ------------------------------------------------------------------

constexpr std::int32_t kEffectMagic = 0x56737450; // 'VstP'

// Canonical VST2 fixed string buffer sizes (number of usable characters; hosts allocate
// length + 1 bytes). Writing beyond these overflows the host's buffers.
constexpr std::int32_t kVstMaxProgNameLen   = 24; // effGetProgramName / effSetProgramName
constexpr std::int32_t kVstMaxParamStrLen   = 8;  // effGetParamName / Label / Display
constexpr std::int32_t kVstMaxVendorStrLen  = 64; // effGetVendorString
constexpr std::int32_t kVstMaxProductStrLen = 64; // effGetProductString
constexpr std::int32_t kVstMaxEffectNameLen = 32; // effGetEffectName

constexpr std::int32_t kEffFlagsHasEditor       = 1 << 0;
constexpr std::int32_t kEffFlagsCanReplacing    = 1 << 4;
constexpr std::int32_t kEffFlagsProgramChunks   = 1 << 5;
constexpr std::int32_t kEffFlagsIsSynth         = 1 << 8;

constexpr std::int32_t kPlugCategoryEffect      = 0;
constexpr std::int32_t kPlugCategorySynth       = 2;
constexpr std::int32_t kPlugCategoryShell       = 10;

// TimeInfo flags
constexpr std::int32_t kTransportChanged        = 1;
constexpr std::int32_t kTransportPlaying        = 2;
constexpr std::int32_t kPpqValid                = 1 << 9;
constexpr std::int32_t kTempoValid              = 1 << 10;
constexpr std::int32_t kBarsValid               = 1 << 11;
constexpr std::int32_t kTimeSigValid            = 1 << 13;

// CanDo strings (host -> plugin queries)
inline constexpr const char* kCanDoReceiveVstEvents      = "receiveVstEvents";
inline constexpr const char* kCanDoReceiveVstTimeInfo    = "receiveVstTimeInfo";
inline constexpr const char* kCanDoOffline               = "offline";
inline constexpr const char* kCanDoMidiProgramNames      = "midiProgramNames";
inline constexpr const char* kCanDoBypass                = "bypass";
inline constexpr const char* kCanDoShellCategory         = "shellCategory";
inline constexpr const char* kCanDoSendVstEvents         = "sendVstEvents";
inline constexpr const char* kCanDoSendVstMidiEvents     = "sendVstMidiEvents";
inline constexpr const char* kCanDoSendVstTimeInfo       = "sendVstTimeInfo";
inline constexpr const char* kCanDoReceiveVstMidiEvent   = "receiveVstMidiEvent";
inline constexpr const char* kCanDoReceiveVstMidiEvents  = "receiveVstMidiEvents";
inline constexpr const char* kCanDoSupplyIdle            = "supplyIdle";

// --- Structures -------------------------------------------------------------

struct MidiEvent
{
    std::int32_t type = 1;          // MIDI
    std::int32_t byteSize = 32;
    std::int32_t deltaFrames = 0;
    std::int32_t flags = 1;         // real-time
    std::int32_t noteLength = 0;
    std::int32_t noteOffset = 0;
    char data[4] {};
    char detune = 0;
    char noteOffVelocity = 0;
    char reserved1 = 0;
    char reserved2 = 0;
};
static_assert(sizeof(MidiEvent) == 32, "MidiEvent must be 32 bytes");

struct Events
{
    std::int32_t numEvents = 0;
    std::intptr_t reserved = 0;
    MidiEvent* events[2] = { nullptr, nullptr };
};

struct Rect
{
    std::int16_t top = 0, left = 0, bottom = 0, right = 0;
};

struct TimeInfo
{
    double samplePos;
    double sampleRate;
    double nanoSeconds;
    double ppqPos;
    double tempo;
    double barStartPos;
    double cycleStartPos;
    double cycleEndPos;
    std::int32_t timeSigNumerator;
    std::int32_t timeSigDenominator;
    std::int32_t smpteOffset;
    std::int32_t smpteFrameRate;
    std::int32_t samplesToNextClock;
    std::int32_t flags;
};

// --- AEffect (plugin instance) ---------------------------------------------

struct AEffect;

using HostCallback = std::intptr_t (*)(AEffect*, std::int32_t opcode, std::int32_t index, std::intptr_t value, void* ptr, float opt);
using Dispatcher = std::intptr_t (*)(AEffect*, std::int32_t opcode, std::int32_t index, std::intptr_t value, void* ptr, float opt);
using Process = void (*)(AEffect*, float** inputs, float** outputs, std::int32_t frames);
using SetParameter = void (*)(AEffect*, std::int32_t index, float value);
using GetParameter = float (*)(AEffect*, std::int32_t index);
using Entry = AEffect* (*)(HostCallback);

struct AEffect
{
    std::int32_t magic;
    Dispatcher dispatcher;
    Process process;                 // accumulating (deprecated)
    SetParameter setParameter;
    GetParameter getParameter;
    std::int32_t numPrograms;
    std::int32_t numParams;
    std::int32_t numInputs;
    std::int32_t numOutputs;
    std::int32_t flags;
    std::intptr_t hostReserved1;
    std::intptr_t hostReserved2;
    std::int32_t initialDelay;
    std::int32_t realQualities;
    std::int32_t offQualities;
    float ioRatio;
    void* object;
    void* user;
    std::int32_t uniqueId;
    std::int32_t version;
    Process processReplacing;
    void* processDoubleReplacing;
    char future[56];
};
static_assert(sizeof(AEffect) == 192, "AEffect must be 192 bytes on x64 (VST 2.4 ABI)");

} // namespace lynx::vst2
