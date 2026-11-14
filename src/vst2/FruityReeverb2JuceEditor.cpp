// VST2 editor glue: hosts LYNX's existing JUCE frontend (ui/PluginEditors.cpp,
// makeNativeEffectEditor -> NativeEffectEditor + PluginHeader presets bar) inside the plugin's
// editor window. The frontend is reused unchanged; this file only adapts it to the VST2 editor
// lifecycle (guiGetRect / guiOpen / guiClose / guiIdle from FruityReeverb2Gui.h).
//
// The glue talks to the plugin through the public AEffect vtable exactly like the previous Win32
// editor did, with one read-only typed accessor (hostBpm) for the tempo-synced predelay display.
// Parameters flow both ways through the Song: the host's values are pushed into the mixer slot the
// frontend edits, and frontend edits are forwarded back to the host as automation.

#include "FruityReeverb2Gui.h"
#include "FruityReeverb2Vst2.h"

#include "../core/Effects.h"
#include "../core/Song.h"
#include "../engine/AudioEngine.h"
#include "../ui/PluginEditors.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace lynx::vst2
{

namespace
{
// The embedded frontend must not discover FL Studio (its FL preset lists then come back empty;
// LYNX's own factory presets and user presets are unaffected). Set before any component exists.
struct FlDiscoveryOff
{
    FlDiscoveryOff() { setFlStudioDiscoveryDisabled (true); }
};
const FlDiscoveryOff flDiscoveryOff {};
// The mixer slot the frontend edits: "Insert 1" of LYNX's default mixer (0 is the Master).
constexpr int editorTrack = 1;
constexpr int editorSlot = 0;
// NativeEffectEditor's size for the reeverb2 layout: 538x166 body + 26 presets bar.
constexpr int editorWidth = 538;
constexpr int editorHeight = 192;

struct EditorContext
{
    AEffect* effect = nullptr;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<juce::Component> editor;
        // Mirrors for the two-way sync: what we last pushed to the slot (host is source) and what the
    // host last received (slot is source). Exact float compare is fine: both sides store the same
    // 0..1 values the plugin's getParameter/setParameter use.
    std::array<float, kNumParams> lastPushedToSlot {};
    std::array<float, kNumParams> lastSentToHost {};
    double lastBpm = 0.0;
};

// VST2 editors are single-instance per plugin object and the gui* entry points are stateless, so
// the open editor lives in a file-static unique_ptr (null when the editor is closed).
std::unique_ptr<EditorContext> context;

void syncWithPlugin (EditorContext& ctx, AEffect& effect)
{
    auto& song = ctx.engine->getSong();
    if (editorTrack < 0 || editorTrack >= (int) song.mixer.size())
        return;
    auto& slots = song.mixer[(std::size_t) editorTrack].effects;
    if (editorSlot < 0 || editorSlot >= (int) slots.size())
        return;
    const auto& fx = slots[(std::size_t) editorSlot];
    if (fx.kind != EffectKind::reeverb2)
        return;

    // Host -> slot: the host changed a parameter (automation, preset load, chunk restore).
    std::array<std::pair<int, float>, kNumParams> pending {};
    std::array<bool, kNumParams> hostChanged {};
    int pendingCount = 0;
    for (int i = 0; i < kNumParams; ++i)
    {
        const float host = effect.getParameter (&effect, i);
        if (host != ctx.lastPushedToSlot[(std::size_t) i])
        {
            ctx.lastPushedToSlot[(std::size_t) i] = host;
            ctx.lastSentToHost[(std::size_t) i] = host;
            hostChanged[(std::size_t) i] = true;
                    pending[(std::size_t) pendingCount++] = { i, host };
                }
    }

    // Slot -> host: the frontend wrote the Song (control drag, preset pick); forward it so the
    // host's automation state follows. setParameter fires audioMasterAutomate, as a plugin editor
    // must.
    for (int i = 0; i < kNumParams; ++i)
    {
        if (hostChanged[(std::size_t) i])
            continue;
        const float slotValue = fx.params[(std::size_t) i];
        if (slotValue != ctx.lastSentToHost[(std::size_t) i])
        {
            ctx.lastSentToHost[(std::size_t) i] = slotValue;
                    ctx.lastPushedToSlot[(std::size_t) i] = slotValue;
                    effect.setParameter (&effect, i, slotValue);
                }
    }

    // Host tempo -> Song: the frontend's tempo-synced predelay display reads song.bpm.
    double bpm = 0.0;
    if (auto* plugin = static_cast<FruityReeverb2Plugin*> (effect.object))
        bpm = plugin->hostBpm();
    const bool bpmChanged = bpm > 0.0 && std::abs (bpm - ctx.lastBpm) > 1e-9;

    if (pendingCount == 0 && ! bpmChanged)
        return;

    ctx.lastBpm = bpm;
    const int count = pendingCount;
    ctx.engine->editSong ([pending, count, bpm, bpmChanged] (Song& song) {
        auto& target = song.mixer[(std::size_t) editorTrack].effects[(std::size_t) editorSlot];
        for (int n = 0; n < count; ++n)
            target.params[(std::size_t) pending[(std::size_t) n].first] = pending[(std::size_t) n].second;
        if (bpmChanged)
            song.bpm = bpm;
    },
                     false); // no undo step: the host owns the plugin's undo history
}

} // namespace

void guiGetRect (Rect& outRect)
{
    outRect.top = 0;
    outRect.left = 0;
    outRect.bottom = (std::int16_t) editorHeight;
    outRect.right = (std::int16_t) editorWidth;
}

void* guiOpen (AEffect* effect, HWND parentHwnd, HINSTANCE /*instance*/)
{
    if (effect == nullptr || parentHwnd == nullptr)
        return nullptr;
    if (context != nullptr) // a previous editor was never closed; drop it before opening a new one
        guiClose (context.get());

    // JUCE's message thread must exist before any component is created; the host's GUI thread
    // pumps it from here on (timers, repaints and the frontend's async callbacks all ride the
    // host's message loop, as with any JUCE plugin).
    juce::MessageManager::getInstance();

    auto ctx = std::make_unique<EditorContext>();
    ctx->effect = effect;
    ctx->engine = std::make_unique<AudioEngine> (false); // no audio device: the host plays the audio

    // Place the effect slot the frontend edits and seed it with the plugin's current parameters.
    ctx->engine->editSong ([effect] (Song& song) {
        if (editorTrack < 0 || editorTrack >= (int) song.mixer.size())
            return;
        auto& effects = song.mixer[(std::size_t) editorTrack].effects;
        auto slot = makeEffectSlot (EffectKind::reeverb2);
        for (int i = 0; i < kNumParams && i < (int) slot.params.size(); ++i)
            slot.params[(std::size_t) i] = effect->getParameter (effect, i);
        effects.insert (effects.begin(), std::move (slot));
    },
                     false);

    auto& song = ctx->engine->getSong();
    if (editorTrack >= (int) song.mixer.size() || song.mixer[(std::size_t) editorTrack].effects.empty())
        return nullptr; // could not place the slot (no mixer tracks)

    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = effect->getParameter (effect, i);
        ctx->lastPushedToSlot[(std::size_t) i] = v;
        ctx->lastSentToHost[(std::size_t) i] = v;
    }
    if (auto* plugin = static_cast<FruityReeverb2Plugin*> (effect->object))
        ctx->lastBpm = plugin->hostBpm();

    // The existing LYNX frontend, unchanged.
    ctx->editor = makeNativeEffectEditor (*ctx->engine, editorTrack, editorSlot, 1.0f, 0);
        if (ctx->editor == nullptr)
            return nullptr; // could not build the frontend (should not happen)

        // The frontend paints its whole background (skin::canvas), so it is opaque. Without this the
    // Windows peer picks its per-pixel-transparency path, whose setBounds translates child bounds
    // by the parent's screen position while SetWindowPos on a WS_CHILD window expects parent-client
    // coordinates — the editor window would land outside its parent and the plugin form shows black.
    // LYNX's own top-level windows are opaque too (Main.cpp frame.setOpaque).
    ctx->editor->setOpaque (true);

        // JUCE 8 removed ComponentPeer::windowIsChild; the Windows peer adds WS_CHILD
        // itself when a parent handle is passed (juce_Windowing_windows.cpp:2394).
        ctx->editor->addToDesktop (0, parentHwnd);
        ctx->editor->setVisible (true);

        context = std::move (ctx);
        return context.get();
    }

    void guiClose (void* /*editorHandle*/)
    {
        if (context == nullptr)
            return;
        auto ctx = std::move (context);
        if (ctx->editor != nullptr)
        {
            ctx->editor->setVisible (false);
            ctx->editor->removeFromDesktop();
        }
        ctx->editor.reset(); // the editor's destructor removes its engine change listener first
        ctx->engine.reset();
    }

    void guiIdle (void* /*editorHandle*/)
    {
        if (context == nullptr || context->engine == nullptr || context->effect == nullptr)
            return;
        syncWithPlugin (*context, *context->effect);
    }

} // namespace lynx::vst2
