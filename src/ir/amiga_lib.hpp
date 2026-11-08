#pragma once

// AmigaOS library functions that GFA-BASIC 3.5 calls directly by name (reference chapter 11), with
// their offsets (LVO) and argument registers from the FD files of Kickstart 1.2/1.3. Only the
// functions whose FD entries are certain are listed; others are rejected by sema.
//
// Registers: "d0".."d7", "a0".."a5", one per argument, in the order of the BASIC call.
// lib: 'e' exec (base at 4), 'd' dos, 'i' intuition, 'g' graphics.

#include <cctype>
#include <cstring>
#include <string>

namespace amiga {

struct LibFn {
    const char* name;
    char lib;
    int lvo;
    const char* regs;  // e.g. "a0d0d1"
};

inline constexpr LibFn kLibFns[] = {
    // exec.library
    {"Disable", 'e', -120, ""},
    {"Enable", 'e', -126, ""},
    {"Forbid", 'e', -132, ""},
    {"Permit", 'e', -138, ""},
    {"AllocMem", 'e', -198, "d0d1"},
    {"FreeMem", 'e', -210, "a1d0"},
    {"AvailMem", 'e', -216, "d1"},
    {"FindTask", 'e', -294, "a1"},
    {"SetTaskPri", 'e', -300, "a1d0"},
    {"SetSignal", 'e', -306, "d0d1"},
    {"Wait", 'e', -318, "d0"},
    {"Signal", 'e', -324, "a1d0"},
    {"AllocSignal", 'e', -330, "d0"},
    {"FreeSignal", 'e', -336, "d0"},
    {"PutMsg", 'e', -366, "a0a1"},
    {"GetMsg", 'e', -372, "a0"},
    {"ReplyMsg", 'e', -378, "a1"},
    {"WaitPort", 'e', -384, "a0"},
    {"FindPort", 'e', -390, "a1"},
    {"OldOpenLibrary", 'e', -408, "a1"},
    {"CloseLibrary", 'e', -414, "a1"},
    {"OpenLibrary", 'e', -552, "a1d0"},
    {"CopyMem", 'e', -624, "a0a1d0"},
    {"CopyMemQuick", 'e', -630, "a0a1d0"},
    // intuition.library
    {"ClearMenuStrip", 'i', -54, "a0"},
    {"ClearPointer", 'i', -60, "a0"},
    {"CloseScreen", 'i', -66, "a0"},
    {"CloseWindow", 'i', -72, "a0"},
    {"CurrentTime", 'i', -84, "a0a1"},
    {"DisplayAlert", 'i', -90, "d0a0d1"},
    {"DisplayBeep", 'i', -96, "a0"},
    {"DrawBorder", 'i', -108, "a0a1d0d1"},
    {"DrawImage", 'i', -114, "a0a1d0d1"},
    {"ItemAddress", 'i', -144, "a0d0"},
    {"ModifyIDCMP", 'i', -150, "a0d0"},
    {"MoveScreen", 'i', -162, "a0d0d1"},
    {"MoveWindow", 'i', -168, "a0d0d1"},
    {"OffMenu", 'i', -180, "a0d0"},
    {"OnMenu", 'i', -192, "a0d0"},
    {"OpenScreen", 'i', -198, "a0"},
    {"OpenWindow", 'i', -204, "a0"},
    {"PrintIText", 'i', -216, "a0a1d0d1"},
    {"RefreshGadgets", 'i', -222, "a0a1a2"},
    {"ScreenToBack", 'i', -246, "a0"},
    {"ScreenToFront", 'i', -252, "a0"},
    {"SetMenuStrip", 'i', -264, "a0a1"},
    {"SetPointer", 'i', -270, "a0a1d0d1d2d3"},
    {"SetWindowTitles", 'i', -276, "a0a1a2"},
    {"ShowTitle", 'i', -282, "a0d0"},
    {"SizeWindow", 'i', -288, "a0d0d1"},
    {"ViewAddress", 'i', -294, ""},
    {"ViewPortAddress", 'i', -300, "a0"},
    {"WindowToBack", 'i', -306, "a0"},
    {"WindowToFront", 'i', -312, "a0"},
    {"WindowLimits", 'i', -318, "a0d0d1d2d3"},
    {"WBenchToBack", 'i', -336, ""},
    {"WBenchToFront", 'i', -342, ""},
    {"MakeScreen", 'i', -378, "a0"},
    {"RemakeDisplay", 'i', -384, ""},
    {"RethinkDisplay", 'i', -390, ""},
    {"ActivateWindow", 'i', -450, "a0"},
    {"ActivateGadget", 'i', -462, "a0a1a2"},
    // graphics.library
    {"ClearEOL", 'g', -42, "a1"},
    {"ClearScreen", 'g', -48, "a1"},
    {"TextLength", 'g', -54, "a1a0d0"},
    {"Text", 'g', -60, "a1a0d0"},
    {"SetFont", 'g', -66, "a1a0"},
    {"OpenFont", 'g', -72, "a0"},
    {"CloseFont", 'g', -78, "a1"},
    {"AskSoftStyle", 'g', -84, "a1"},
    {"SetSoftStyle", 'g', -90, "a1d0d1"},
    {"DrawEllipse", 'g', -180, "a1d0d1d2d3"},
    {"LoadRGB4", 'g', -192, "a0a1d0"},
    {"WaitBlit", 'g', -228, ""},
    {"SetRast", 'g', -234, "a1d0"},
    {"Move", 'g', -240, "a1d0d1"},
    {"Draw", 'g', -246, "a1d0d1"},
    {"WaitTOF", 'g', -270, ""},
    {"SetRGB4", 'g', -288, "a0d0d1d2d3"},
    {"RectFill", 'g', -306, "a1d0d1d2d3"},
    {"ReadPixel", 'g', -318, "a1d0d1"},
    {"WritePixel", 'g', -324, "a1d0d1"},
    {"Flood", 'g', -330, "a1d2d0d1"},
    {"PolyDraw", 'g', -336, "a1d0a0"},
    {"SetAPen", 'g', -342, "a1d0"},
    {"SetBPen", 'g', -348, "a1d0"},
    {"SetDrMd", 'g', -354, "a1d0"},
    {"ScrollRaster", 'g', -396, "a1d0d1d2d3d4d5"},
    {"GetRGB4", 'g', -582, "a0d0"},
    // dos.library
    {"Open", 'd', -30, "d1d2"},
    {"Close", 'd', -36, "d1"},
    {"Read", 'd', -42, "d1d2d3"},
    {"Write", 'd', -48, "d1d2d3"},
    {"Input", 'd', -54, ""},
    {"Output", 'd', -60, ""},
    {"Seek", 'd', -66, "d1d2d3"},
    {"DeleteFile", 'd', -72, "d1"},
    {"Rename", 'd', -78, "d1d2"},
    {"Lock", 'd', -84, "d1d2"},
    {"UnLock", 'd', -90, "d1"},
    {"DupLock", 'd', -96, "d1"},
    {"Examine", 'd', -102, "d1d2"},
    {"ExNext", 'd', -108, "d1d2"},
    {"Info", 'd', -114, "d1d2"},
    {"CreateDir", 'd', -120, "d1"},
    {"CurrentDir", 'd', -126, "d1"},
    {"IoErr", 'd', -132, ""},
    {"SetComment", 'd', -180, "d1d2"},
    {"SetProtection", 'd', -186, "d1d2"},
    {"DateStamp", 'd', -192, "d1"},
    {"Delay", 'd', -198, "d1"},
    {"WaitForChar", 'd', -204, "d1d2"},
    {"ParentDir", 'd', -210, "d1"},
    {"IsInteractive", 'd', -216, "d1"},
    {"Execute", 'd', -222, "d1d2d3"},
};
inline constexpr int kLibFnCount = static_cast<int>(sizeof kLibFns / sizeof kLibFns[0]);

// the index of a library function by name (without regard to case), -1 if there is none
inline int libFunction(const std::string& name) {
    for (int k = 0; k < kLibFnCount; ++k) {
        const char* n = kLibFns[k].name;
        if (std::strlen(n) != name.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < name.size() && same; ++i)
            same = std::toupper(static_cast<unsigned char>(n[i])) == std::toupper(static_cast<unsigned char>(name[i]));
        if (same) return k;
    }
    return -1;
}

inline int argCount(const LibFn& f) { return static_cast<int>(std::strlen(f.regs) / 2); }

// register number of argument k: 0..7 d0..d7, 8..13 a0..a5
inline int argRegister(const LibFn& f, int k) {
    const char* r = f.regs + 2 * k;
    return (r[0] == 'a' ? 8 : 0) + (r[1] - '0');
}

}  // namespace amiga
