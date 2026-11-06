// Smoke test of the MC68000 emulator harness: a bare program on m68k-test and an AmigaDOS
// hunk program that prints through dos.library on the amiga platform.

#include "m68k_runner.hpp"
#include "test_util.hpp"

using namespace basic::harness;

int main() {
    {  // m68k-test: MOVE.B to $00FF0000, end with STOP
        const char* src =
            "        ORG $1000\n"
            "start:  LEA     msg(PC),A0\n"
            ".next:  MOVE.B  (A0)+,D0\n"
            "        BEQ.S   .done\n"
            "        BSR.S   plat_putc\n"
            "        BRA.S   .next\n"
            ".done:  STOP    #$2700\n"
            "plat_putc:\n"
            "        MOVE.B  D0,$00FF0000\n"
            "        RTS\n"
            "msg:    DC.B    'Hallo 68000',10,0\n";
        const M68kRun run = runM68kSource(src, M68kPlatform::Test);
        CHECK_EQ(run.error, std::string());
        CHECK(run.finished);
        CHECK_EQ(run.console, std::string("Hallo 68000\n"));
    }
    {  // amiga: hunk program, OpenLibrary("dos.library"), Output(), Write()
        const char* src =
            "        SECTION code,CODE\n"
            "start:  MOVE.L  4.W,A6\n"
            "        LEA     dosname(PC),A1\n"
            "        MOVEQ   #0,D0\n"
            "        JSR     -552(A6)\n"
            "        MOVE.L  D0,A6\n"
            "        JSR     -60(A6)\n"
            "        MOVE.L  D0,D1\n"
            "        LEA     msg(PC),A0\n"
            "        MOVE.L  A0,D2\n"
            "        MOVEQ   #12,D3\n"
            "        JSR     -48(A6)\n"
            "        MOVEQ   #0,D0\n"
            "        RTS\n"
            "dosname: DC.B   'dos.library',0\n"
            "msg:    DC.B    'Hallo Amiga',10\n"
            "        EVEN\n";
        const M68kRun run = runM68kSource(src, M68kPlatform::Amiga);
        CHECK_EQ(run.error, std::string());
        CHECK(run.finished);
        CHECK_EQ(run.console, std::string("Hallo Amiga\n"));
        CHECK(run.unemulatedCalls.empty());
    }
    {  // m68k-test must not use SECTION, amiga must
        CHECK(!runM68kSource("        SECTION c,CODE\n        RTS\n", M68kPlatform::Test).error.empty());
        CHECK(!runM68kSource("        ORG $1000\n        RTS\n", M68kPlatform::Amiga).error.empty());
    }
    return report("harness_m68k");
}
