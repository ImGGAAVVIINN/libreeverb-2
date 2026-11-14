# Small fixes LYNX applies to the JUCE sources before they are compiled (idempotent: a patched file is left alone).
#
# 1. Linux drag & drop from a Wayland file manager (issue #11, round 165s). On GNOME (mutter) and KDE (KWin) Wayland the
#    compositor's XWayland bridge sends XdndEnter with XDND version 5 to every X11 window, whatever version the window
#    announced in XdndAware. JUCE 8.0.4 (and JUCE's develop branch, 2026-10) ignores any XdndEnter above version 3, so
#    no drop from Nautilus/Dolphin ever reached LYNX. Versions 4 and 5 only add fields a version 3 target may ignore
#    (XdndProxy, XdndFinished's result), so versions 3..5 are accepted now.
# 2. GTK 4 apps (Nautilus 4x+) offer a file drag as "text/plain;charset=utf-8" BEFORE "text/uri-list"; JUCE took the
#    first offered type it knows, got the paths as plain text and delivered a text drag that no file drop target wants.
#    "text/uri-list" is preferred whenever the source offers it.
# 3. A drop JUCE could not take (no type it knows, or the version check above) left `finishAfterDropDataReceived` set,
#    so the NEXT drag was "finished" as soon as its data arrived on XdndEnter (JUCE asks for it early) and dropped at
#    the enter message's meaningless position: one failed drag made the next good one land nowhere. Reset on enter.
#    (1-3: juce_gui_basics/native/juce_DragAndDrop_linux.cpp, compiled on Linux/BSD only.)
# 4. Every platform, juce_gui_basics/windows/juce_ComponentPeer.cpp: a file drop that no component took (an unsupported
#    file, or dropped where nothing accepts files) kept `lastDragAndDropCompUnderMouse`, so the next drag that starts
#    over that same component (the Playlist next to the file manager) never looked for a target and was refused.
# 5. Every platform, juce_gui_basics/menus/juce_PopupMenu.cpp (issue #18, round 167p): LYNX opens a submenu beside its
#    parent menu's outer edge (Theme.cpp preparePopupMenuWindow), so in a two-column menu the mouse crosses the other
#    column on its way. JUCE keeps the open submenu only while the mouse stays in a thin triangle towards it; a pixel
#    of drift highlighted the other column's item under the mouse and closed the submenu. The way across now also
#    counts as "moving towards the submenu": the band of the item's row (one row above and below), between the item
#    and the submenu, while the mouse moves towards it. Resting there (JUCE's 350 ms) still picks the item under it.
# 6. Every platform, juce_PopupMenu.cpp (issue #45, round 168z): a submenu took the display under the item that opened
#    it, so a tall submenu reaching into the monitor below (or a confused XWayland display list) sent the next submenu
#    to another monitor. A submenu now stays on the display of its ROOT menu, i.e. of the window the menu came from.
# 7. Linux/BSD only, juce_PopupMenu.cpp (issue #45): JUCE closes a menu ("the app lost the focus") as soon as none of
#    its windows has the X focus and the mouse is not over the menu. With KWin/XWayland and more than one LYNX window
#    (an undocked panel, a settings window) the focus moves between our windows, and through the menu window itself
#    (fix 8), for a moment after the click that opens a menu, so most menus closed before they were seen. The mouse
#    over one of LYNX's own windows now also counts as "still LYNX's".
# 8. Linux/BSD only, juce_gui_basics/native/juce_Windowing_linux.cpp (issue #45): a window that ignores key presses
#    (a popup menu, a tooltip) never takes the X focus. When a LYNX window got the focus while a menu was open, JUCE
#    handed the focus to the menu's override-redirect window, which the compositor gives back: the focus ping-ponged
#    between the two and the menu saw "no focus". Keys still reach the menu (JUCE sends them to the modal component).
# 9. Every platform, juce_gui_basics/desktop/juce_Displays.cpp (issue #45, round 168z2): Displays::updateToLogical
#    places each monitor beside one it touches exactly (in physical pixels). A monitor that touches none (a gap or an
#    overlap, e.g. XWayland's rounding with KDE's fractional 115 %, or Xvfb's Xinerama screens) was left with an empty
#    totalArea and its userArea at 0,0, a phantom screen over the main one; its windows and menus had no display. It
#    now gets its physical rectangle divided by its scale (exact when all monitors share one scale).
# 10. Linux/BSD only, juce_gui_basics/native/juce_XWindowSystem_linux.cpp (issue #45, round 168z3): toFront (false)
#    raises the window without activating it. JUCE always sent _NET_ACTIVE_WINDOW, so the WM gave the focus to every
#    window LYNX only meant to keep above the main one (an undocked panel, after each click in the main window): the
#    main window lost the focus right after the click on its menu bar, and the menu closed (0 of 20 opened with KWin).
# 11. Linux/BSD only, same file (issue #45, round 168z3): a ConfigureNotify on a window with a native title bar (or on
#    the WM frame around one of our windows) dismissed every open popup menu, meant for "the title bar is being
#    dragged". KWin also sends it when it only restacks or re-sends a frame (with an undocked panel: while a menu of the
#    main window opens), so the menu closed at once. Only a window that really moved or changed size dismisses now.
# 12. Every platform, juce_graphics/native/juce_RenderingHelpers.h: GCC 13 (MinGW) fails to compile juceaide because
#    the file uses memset without including <cstring> (it relied on a transitive include that other toolchains still
#    provide). The include is added unconditionally; harmless everywhere else.
# 13. Windows/MinGW only, modules/juce_core/juce_core.h, juce_graphics.h and juce_gui_basics.h: JUCE 8 dropped MinGW
#    support and removed the mingwLibs metadata from its module declarations, so on MinGW no Windows system libraries
#    are linked at all (juceaide and every JUCE target fail with ~65 undefined references). The three mingwLibs lines
#    LYNX carries in its vendored JUCE checkout are restored; JUCE's own CMake module support links them (MinGW branch
#    of JUCEModuleSupport.cmake).
#
# Used by CMake (lynx_patch_juce on the fetched JUCE) and by scripts/build-windows-zig.sh, which runs this file with
# `cmake -DJUCE_DIR=<a copy of JUCE's juce_gui_basics module> -P cmake/LynxPatchJuce.cmake`.

# One fix: `old` (lines joined with "|NL|") replaced by `new` unless `marker` is already in the file.
macro(lynx_juce_fix name marker old new)
    string(FIND "${text}" "${marker}" at_marker)
    if(at_marker EQUAL -1)
        string(REPLACE "|NL|" "${nl}" old_text "${old}")
        string(REPLACE "|NL|" "${nl}" new_text "${new}")
        string(FIND "${text}" "${old_text}" at_old)
        if(at_old EQUAL -1)
            message(WARNING "LYNX: ${file} changed; re-check fix '${name}' in cmake/LynxPatchJuce.cmake")
        else()
            string(REPLACE "${old_text}" "${new_text}" text "${text}")
            set(changed TRUE)
            message(STATUS "LYNX: JUCE fix applied: ${name}")
        endif()
    endif()
endmacro()

function(lynx_patch_juce_file file)
    if(NOT EXISTS "${file}")
        message(WARNING "LYNX: ${file} not found; JUCE's drag & drop fixes (issue #11) are not applied")
        return()
    endif()

    file(READ "${file}" text)
    set(nl "\n")
    string(FIND "${text}" "\r\n" at_crlf)
    if(NOT at_crlf EQUAL -1)
        set(nl "\r\n") # JUCE's sources use CRLF in some checkouts
    endif()
    set(changed FALSE)

    if(file MATCHES "juce_DragAndDrop_linux.cpp$")
        lynx_juce_fix("XDND 5 sources" "LYNX patch (issue #11): mutter/KWin"
            "if (dndCurrentVersion < 3 || dndCurrentVersion > XWindowSystemUtilities::Atoms::DndVersion)"
            "// LYNX patch (issue #11): mutter/KWin's XWayland bridges always send version 5|NL|        if (dndCurrentVersion < 3 || dndCurrentVersion > 5)")

        lynx_juce_fix("text/uri-list first" "LYNX patch (issue #11): files as text/uri-list"
            "        for (int i = 0; i < srcMimeTypeAtomList.size() && dragAndDropCurrentMimeType == 0; ++i)|NL|            for (int j = 0;"
            "        // LYNX patch (issue #11): files as text/uri-list whenever offered (GTK 4 lists text/plain first)|NL|        for (int i = 0; i < srcMimeTypeAtomList.size() && dragAndDropCurrentMimeType == 0; ++i)|NL|            if (srcMimeTypeAtomList[i] == atoms.allowedMimeTypes[3])|NL|                dragAndDropCurrentMimeType = atoms.allowedMimeTypes[3];|NL||NL|        for (int i = 0; i < srcMimeTypeAtomList.size() && dragAndDropCurrentMimeType == 0; ++i)|NL|            for (int j = 0;")

        lynx_juce_fix("no stale drop state" "LYNX patch (issue #11): not left over"
            "        dragInfo.clear();|NL|        srcMimeTypeAtomList.clear();|NL||NL|        dragAndDropCurrentMimeType = 0;"
            "        dragInfo.clear();|NL|        srcMimeTypeAtomList.clear();|NL|        finishAfterDropDataReceived = false; // LYNX patch (issue #11): not left over from a failed drop|NL||NL|        dragAndDropCurrentMimeType = 0;")
    elseif(file MATCHES "juce_PopupMenu.cpp$")
        lynx_juce_fix("submenu corridor" "LYNX patch (issue #18): the way across"
            "        return areaTowardsSubMenu.contains (newGlobalPos.toFloat());|NL|    }"
            "        if (areaTowardsSubMenu.contains (newGlobalPos.toFloat()))|NL|            return true;|NL||NL|        // LYNX patch (issue #18): the way across a two-column menu to a submenu beside its outer edge|NL|        if (auto* item = window.currentChild.getComponent())|NL|        {|NL|            const auto itemBounds = item->getScreenBounds();|NL|            const auto towardsRight = itemScreenBounds.getX() > window.getX();|NL|            const auto from = towardsRight ? itemBounds.getRight() : itemScreenBounds.getRight();|NL|            const auto to = towardsRight ? itemScreenBounds.getX() : itemBounds.getX();|NL|            return newGlobalPos.y >= itemBounds.getY() - itemBounds.getHeight() && newGlobalPos.y < itemBounds.getBottom() + itemBounds.getHeight()|NL|                   && newGlobalPos.x >= from - 2 && newGlobalPos.x <= to + 2|NL|                   && (towardsRight ? newGlobalPos.x >= lastMousePos.x : newGlobalPos.x <= lastMousePos.x);|NL|        }|NL||NL|        return false;|NL|    }")

        lynx_juce_fix("submenu on the root menu's display" "LYNX patch (issue #45): a submenu stays"
            "    Rectangle<int> getParentArea (Point<int> targetPoint, Component* relativeTo = nullptr)|NL|    {|NL|"
            "    Rectangle<int> getParentArea (Point<int> targetPoint, Component* relativeTo = nullptr)|NL|    {|NL|        // LYNX patch (issue #45): a submenu stays on the display of its root menu (the window it came from)|NL|        if (parent != nullptr && relativeTo == nullptr && options.getParentComponent() == nullptr)|NL|        {|NL|            auto* root = parent;|NL|            while (root->parent != nullptr)|NL|                root = root->parent;|NL|            targetPoint = root->getBounds().getCentre();|NL|        }|NL||NL|")

        lynx_juce_fix("menu kept while the mouse is over LYNX" "LYNX patch (issue #45): the mouse over"
            "    bool doesAnyJuceCompHaveFocus()|NL|    {|NL|"
            "    bool doesAnyJuceCompHaveFocus()|NL|    {|NL|       #if JUCE_LINUX || JUCE_BSD|NL|        // LYNX patch (issue #45): the mouse over one of our windows = still ours (X focus moving between them)|NL|        {|NL|            const auto mousePos = Desktop::getInstance().getMainMouseSource().getScreenPosition();|NL||NL|            for (int i = ComponentPeer::getNumPeers(); --i >= 0;)|NL|            {|NL|                auto& comp = ComponentPeer::getPeer (i)->getComponent();|NL||NL|                if (comp.isShowing() && comp.getLocalBounds().toFloat().contains (comp.getLocalPoint (nullptr, mousePos)))|NL|                    return true;|NL|            }|NL|        }|NL|       #endif|NL||NL|")
    elseif(file MATCHES "juce_Windowing_linux.cpp$")
        lynx_juce_fix("no X focus for menu windows" "LYNX patch (issue #45): never takes the X focus"
            "    void grabFocus() override|NL|    {|NL|"
            "    void grabFocus() override|NL|    {|NL|        // LYNX patch (issue #45): never takes the X focus when it ignores key presses (a popup menu, a tooltip)|NL|        if ((getStyleFlags() & windowIgnoresKeyPresses) != 0)|NL|            return;|NL||NL|")
    elseif(file MATCHES "juce_Displays.cpp$")
        lynx_juce_fix("monitors that touch no other" "LYNX patch (issue #45): a monitor that touches"
            "        // All of the nodes should have a parent|NL|        jassert (node.parent != nullptr);|NL|"
            "        // LYNX patch (issue #45): a monitor that touches no other one keeps its own place (physical / scale)|NL|        if (node.parent == nullptr)|NL|            node.logicalArea = node.display->totalArea.toDouble() / node.display->scale;|NL|")
    elseif(file MATCHES "juce_XWindowSystem_linux.cpp$")
        lynx_juce_fix("raise without activating" "LYNX patch (issue #45): raise only"
            "void XWindowSystem::toFront (::Window windowH, bool) const|NL|{|NL|    jassert (windowH != 0);|NL|"
            "void XWindowSystem::toFront (::Window windowH, bool makeActive) const|NL|{|NL|    jassert (windowH != 0);|NL||NL|    // LYNX patch (issue #45): raise only, without activating, when the window should not become active|NL|    if (! makeActive)|NL|    {|NL|        XWindowSystemUtilities::ScopedXLock xLock;|NL|        XWindowAttributes atts;|NL||NL|        if (X11Symbols::getInstance()->xGetWindowAttributes (display, windowH, &atts) && atts.map_state == IsViewable)|NL|            X11Symbols::getInstance()->xMapRaised (display, windowH);|NL||NL|        X11Symbols::getInstance()->xSync (display, False);|NL|        return;|NL|    }|NL||NL|")

        lynx_juce_fix("menus kept when a window is only restacked" "LYNX patch (issue #45): only a window that moved"
            "void XWindowSystem::dismissBlockingModals (LinuxComponentPeer* peer) const|NL|{|NL|"
            "// LYNX patch (issue #45): only a window that moved or changed size dismisses the menus (not a restack)|NL|static bool lynxWindowMovedOrResized (const XConfigureEvent& event)|NL|{|NL|    static std::map<std::pair<::Window, bool>, Rectangle<int>> lastGeometry;|NL|    const Rectangle<int> geometry (event.x, event.y, event.width, event.height);|NL|    const auto [it, isNew] = lastGeometry.try_emplace (std::make_pair (event.window, event.send_event != 0), geometry);|NL||NL|    if (isNew)|NL|        return false;|NL||NL|    const auto changed = it->second != geometry;|NL|    it->second = geometry;|NL|    return changed;|NL|}|NL||NL|void XWindowSystem::dismissBlockingModals (LinuxComponentPeer* peer) const|NL|{|NL|")

        lynx_juce_fix("title bar dragged, not restacked" "LYNX patch (issue #45): a title bar that moved"
            "    if ((peer->getStyleFlags() & ComponentPeer::windowHasTitleBar) != 0)|NL|        dismissBlockingModals (peer);"
            "    // LYNX patch (issue #45): a title bar that moved, not a restack|NL|    if ((peer->getStyleFlags() & ComponentPeer::windowHasTitleBar) != 0 && lynxWindowMovedOrResized (confEvent))|NL|        dismissBlockingModals (peer);")

        lynx_juce_fix("frame moved, not restacked" "LYNX patch (issue #45): a frame that moved"
            "    if (configure.window != peerHandle && isParentWindowOf (configure.window, peerHandle))|NL|        dismissBlockingModals (peer);"
            "    // LYNX patch (issue #45): a frame that moved, not a restack|NL|    if (configure.window != peerHandle && isParentWindowOf (configure.window, peerHandle) && lynxWindowMovedOrResized (configure))|NL|        dismissBlockingModals (peer);")
    elseif(file MATCHES "juce_RenderingHelpers.h$")
        lynx_juce_fix("MinGW memset include" "LYNX patch: MinGW GCC 13 needs memset declared"
            "namespace juce|NL|{|NL|    struct GraphicsFontHelpers"
            "// LYNX patch: MinGW GCC 13 needs memset declared (juceaide)|NL|#include <cstring>|NL||NL|namespace juce|NL|{|NL|    struct GraphicsFontHelpers")
    elseif(file MATCHES "juce_BasicNativeHeaders.h$")
        lynx_juce_fix("MinGW C library headers" "LYNX patch: MinGW needs the C library headers explicitly"
            " #define NOMINMAX|NL||NL| #define _WINSOCK_DEPRECATED_NO_WARNINGS 1"
            " #define NOMINMAX|NL||NL| // LYNX patch: MinGW needs the C library headers explicitly (MSVC gets them transitively)|NL| #include <cstring>|NL| #include <cmath>|NL| #include <cstdlib>|NL| #include <cstdio>|NL| #include <cwchar>|NL||NL| #define _WINSOCK_DEPRECATED_NO_WARNINGS 1")
        elseif(file MATCHES "juce_StandardHeader.h$")
            lynx_juce_fix("MinGW C library headers in StandardHeader" "MinGW needs C library headers explicitly"
                "JUCE_END_IGNORE_WARNINGS_MSVC"
                "#if JUCE_WINDOWS && !JUCE_MSVC|NL| // MinGW needs C library headers explicitly (MSVC gets them transitively via windows.h)|NL| #include <cstring>|NL| #include <cmath>|NL| #include <cstdlib>|NL| #include <cstdio>|NL| #include <cwchar>|NL|#endif|NL||NL|JUCE_END_IGNORE_WARNINGS_MSVC")
        elseif(file MATCHES "juce_CompilerWarnings.h$")
            lynx_juce_fix("MinGW GCC pragma support" "LYNX patch: MinGW GCC doesn't support _Pragma in lambda initializers"
                "#if JUCE_CLANG || JUCE_GCC"
                "#if JUCE_CLANG || (JUCE_GCC && !JUCE_MINGW)")
    elseif(file MATCHES "juce_UIASelectionProvider_windows.h$")
            # MinGW's uiautomationclient.h predates WinRT extensions; define ISelectionProvider2 ourselves.
            # (Newer mingw-w64 headers define it; the stub is guarded and then a no-op.)
            lynx_juce_fix("MinGW accessiblity: ISelectionProvider2 stub" "LYNX patch: MinGW's uiautomationcore.h predates ISelectionProvider2"
                "JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE (\"-Wlanguage-extension-token\")"
                "// LYNX patch: MinGW's uiautomationcore.h predates ISelectionProvider2|NL|#ifndef __ISelectionProvider2_INTERFACE_DEFINED__|NL|#define __ISelectionProvider2_INTERFACE_DEFINED__|NL|DEFINE_GUID(IID_ISelectionProvider2, 0xacd9a70e, 0x6e5b, 0x49e8, 0xa3, 0x2f, 0xca, 0x63, 0xd6, 0xc7, 0x66, 0x47);|NL|interface ISelectionProvider2 : public ISelectionProvider {} ;|NL|#endif|NL||NL|JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE (\"-Wlanguage-extension-token\")")
    elseif(file MATCHES "juce_UIATextProvider_windows.h$")
            # MinGW's uiautomation headers lack the CaretPosition enum (the UIA_* attribute IDs come from <uiautomation.h>).
            lynx_juce_fix("MinGW accessibility: CaretPosition enum" "LYNX patch: MinGW's uiautomation headers lack the CaretPosition enum"
                "namespace juce|NL|{|NL||NL|//==============================================================================|NL|class UIATextProvider"
                "namespace juce|NL|{|NL||NL|// LYNX patch: MinGW's uiautomation headers lack the CaretPosition enum used below|NL|#ifndef JUCE_LYNX_CARETPOSITION_DEFINED|NL|#define JUCE_LYNX_CARETPOSITION_DEFINED|NL|enum CaretPosition { CaretPosition_BeginningOfLine = 0, CaretPosition_EndOfLine = 1, CaretPosition_Unknown = 2 };|NL|#endif|NL||NL|//==============================================================================|NL|class UIATextProvider")
    elseif(file MATCHES "juce_JPEGLoader.cpp$")
            # MinGW: when rpcndr.h is already included (windows.h chain), jconfig.h skips its boolean typedef, so
            # jpeglibNamespace::boolean does not exist and the using-declaration fails. ::boolean is visible then.
            lynx_juce_fix("MinGW jpeg boolean using-declaration" "LYNX patch: jpeg boolean with rpcndr.h"
                "   #if ! (JUCE_WINDOWS && (JUCE_MSVC || JUCE_CLANG))|NL|    using jpeglibNamespace::boolean;|NL|   #endif"
                "   // LYNX patch: jpeg boolean with rpcndr.h|NL|   #if ! (JUCE_WINDOWS && (JUCE_MSVC || JUCE_CLANG)) && ! defined (__RPCNDR_H__)|NL|    using jpeglibNamespace::boolean;|NL|   #endif")
    elseif(file MATCHES "juce_TargetPlatform.h$")
            # MinGW is supported via __MINGW32__/__MINGW64__ but JUCE emits a fatal #warning; strip it.
            lynx_juce_fix("MinGW target platform support" "LYNX patch: remove MinGW removal warning"
                "    #define JUCE_MINGW 1|NL|    #warning Support for MinGW has been removed. Please use an alternative compiler.|NL|    #ifdef __MINGW64__"
                "    #define JUCE_MINGW 1|NL|    #ifdef __MINGW64__")
    elseif(file MATCHES "juce_SystemStats_windows.cpp$")
            # MinGW needs <intrin.h> for __cpuid when not falling through MSVC path.
            lynx_juce_fix("MinGW cpuid intrinsics" "LYNX patch: include intrin.h for __cpuid"
                " #else|NL|static void callCPUID (int result[4], int infoType)|NL|{|NL|    __cpuid (result, infoType);|NL|}"
                " #elif defined(__GNUC__) || defined(__clang__)|NL|#include <intrin.h>|NL|static void callCPUID (int result[4], int infoType)|NL|{|NL|    __cpuid (result, infoType);|NL|}")
    elseif(file MATCHES "modules/juce_core/juce_core\.h$")
        # JUCE 8 dropped MinGW support and removed the mingwLibs metadata; without it no Windows
        # system libraries are linked on MinGW (~65 undefined references in juceaide). JUCE's own
        # CMake module support links mingwLibs on MinGW (JUCEModuleSupport.cmake), so only the
        # metadata lines from LYNX's vendored JUCE checkout are restored here. `mingwLibs:` is the
        # marker: absent in pristine JUCE, present after the first application.
        lynx_juce_fix("MinGW core link libraries" "mingwLibs:"
            "  linuxLibs:          rt dl pthread"
            "  linuxLibs:          rt dl pthread|NL|  mingwLibs:          ws2_32 wininet winmm version dbghelp shlwapi")
    elseif(file MATCHES "modules/juce_graphics/juce_graphics\.h$")
        lynx_juce_fix("MinGW graphics link libraries" "mingwLibs:"
            "  linuxPackages:      freetype2 fontconfig"
            "  linuxPackages:      freetype2 fontconfig|NL|  mingwLibs:          d2d1 d3d11 dxgi dcomp dwrite windowscodecs")
    elseif(file MATCHES "modules/juce_gui_basics/juce_gui_basics\.h$")
        lynx_juce_fix("MinGW gui basics link libraries" "mingwLibs:"
            "  WeakiOSFrameworks:  Metal MetalKit UniformTypeIdentifiers"
            "  WeakiOSFrameworks:  Metal MetalKit UniformTypeIdentifiers|NL|  mingwLibs:          dwmapi imm32 comctl32 uxtheme msimg32 wintrust sapi shlwapi")
    elseif(file MATCHES "juce_Direct2DResources_windows\.cpp$")
        # MinGW's __uuidof cannot take a ComSmartPtr variable (only a type with a known GUID), so
        # the interface type is passed instead, as LYNX's vendored JUCE does. The type names are
        # the markers: absent in pristine JUCE, present after the first application.
        lynx_juce_fix("MinGW uuidof on ComSmartPtr (device)" "__uuidof (IDXGIDevice)"
            "__uuidof (device)"
            "__uuidof (IDXGIDevice)")
        lynx_juce_fix("MinGW uuidof on ComSmartPtr (surface)" "__uuidof (IDXGISurface)"
            "__uuidof (surface)"
            "__uuidof (IDXGISurface)")
    elseif(file MATCHES "extras/Build/juceaide/CMakeLists.txt$")
        # MinGW embeds no default manifest (MSVC's linker does), and juceaide is built with
        # _NO_RESOURCERC, so juceaide.exe binds comctl32 v5 and the TaskDialogIndirect import from
        # juce_gui_basics fails to resolve at load time (0xC0000139). Embed a Common-Controls v6
        # manifest via windres, as the MSVC default manifest would. The manifest files are written
        # here at patch time (macro arguments cannot carry ${...} through: they are re-expanded
        # when the fix is applied), and only a plain target_sources line is injected.
        get_filename_component(_lynx_juceaide_dir "${file}" DIRECTORY)
        file(WRITE "${_lynx_juceaide_dir}/LynxJuceaideManifest.xml" "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n  <dependency>\n    <dependentAssembly>\n      <assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>\n    </dependentAssembly>\n  </dependency>\n</assembly>\n")
        file(WRITE "${_lynx_juceaide_dir}/LynxJuceaideManifest.rc" "1 24 \"${_lynx_juceaide_dir}/LynxJuceaideManifest.xml\"\n")
        lynx_juce_fix("juceaide Common-Controls v6 manifest" "LYNX patch: juceaide Common-Controls v6 manifest"
            "    target_sources(juceaide PRIVATE Main.cpp)"
            "    target_sources(juceaide PRIVATE Main.cpp)|NL||NL|    # LYNX patch: juceaide Common-Controls v6 manifest (MinGW embeds no default manifest; without|NL|    # v6 comctl32 the TaskDialogIndirect import fails to resolve and juceaide exits 0xC0000139)|NL|    target_sources(juceaide PRIVATE LynxJuceaideManifest.rc)")
    else()
        lynx_juce_fix("drop target search after an unused drop" "LYNX patch (issue #11): a drop nobody took"
            "bool ComponentPeer::handleDragDrop (const ComponentPeer::DragInfo& info)|NL|{|NL|    handleDragMove (info);|NL|"
            "bool ComponentPeer::handleDragDrop (const ComponentPeer::DragInfo& info)|NL|{|NL|    handleDragMove (info);|NL|    // LYNX patch (issue #11): a drop nobody took must not leave the component under the mouse remembered|NL|    const ScopeGuard forgetComponentUnderMouse { [this] { lastDragAndDropCompUnderMouse = nullptr; } };|NL|")
    endif()

    if(changed)
        file(WRITE "${file}" "${text}")
    endif()
endfunction()

# `juce_dir`: JUCE's checkout (or any folder with modules/juce_gui_basics)
function(lynx_patch_juce juce_dir)
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/native/juce_DragAndDrop_linux.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/windows/juce_ComponentPeer.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/menus/juce_PopupMenu.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/native/juce_Windowing_linux.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/desktop/juce_Displays.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/native/juce_XWindowSystem_linux.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_graphics/native/juce_RenderingHelpers.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_core/native/juce_BasicNativeHeaders.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_core/system/juce_StandardHeader.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/native/accessibility/juce_UIASelectionProvider_windows.h")
    lynx_patch_juce_file("${juce_dir}/extras/Build/juceaide/CMakeLists.txt")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/native/accessibility/juce_UIATextProvider_windows.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_core/system/juce_TargetPlatform.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_core/juce_core.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_graphics/juce_graphics.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_gui_basics/juce_gui_basics.h")
    lynx_patch_juce_file("${juce_dir}/modules/juce_core/native/juce_SystemStats_windows.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_graphics/image_formats/juce_JPEGLoader.cpp")
    lynx_patch_juce_file("${juce_dir}/modules/juce_graphics/native/juce_Direct2DResources_windows.cpp")
endfunction()

# run as a script: cmake -DJUCE_DIR=<dir> -P cmake/LynxPatchJuce.cmake
if(CMAKE_SCRIPT_MODE_FILE AND DEFINED JUCE_DIR)
    lynx_patch_juce("${JUCE_DIR}")
endif()
