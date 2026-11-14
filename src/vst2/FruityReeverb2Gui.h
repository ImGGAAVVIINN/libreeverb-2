#pragma once

// Standalone Win32 editor for the Fruity Reeverb 2 VST2 plugin.
//
// The editor is deliberately decoupled from the DSP: it talks to the plugin *only* through the
// public AEffect vtable (dispatcher / getParameter / setParameter), exactly like a host would.
// This keeps the GUI off the audio path and guarantees the plugin keeps working with the editor
// closed. No Steinberg SDK types are used; only the clean-room Vst2Abi.h definitions.

#include "Vst2Abi.h"

#include <windows.h>

namespace lynx::vst2
{

// Editor lifecycle. Each function is safe to call with a null handle.
//   guiGetRect  - fills the editor size (device pixels) for effEditGetRect.
//   guiOpen     - creates the editor window parented to parentHwnd; returns the editor handle
//                 (store it in AEffect::user) or nullptr on failure.
//   guiClose    - destroys the editor window and releases its state.
//   guiIdle     - refreshes control positions/labels from the plugin (host automation, etc.).
void  guiGetRect (Rect& outRect);
void* guiOpen (AEffect* effect, HWND parentHwnd, HINSTANCE instance);
void  guiClose (void* editorHandle);
void  guiIdle (void* editorHandle);

} // namespace lynx::vst2
