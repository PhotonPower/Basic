// Smoke test of the Z80 emulator harness: hand-written programs in the shape the compiler will
// produce (platform layer with plat_putc), run on the z80-test and cpc platforms.

#include "test_util.hpp"
#include "z80_runner.hpp"

using namespace basic::harness;

int main() {
    {  // z80-test: console on port &01, end with HALT
        const char* src =
            "        ORG &4000\n"
            "start:  LD HL,msg\n"
            "loop:   LD A,(HL)\n"
            "        OR A\n"
            "        JR Z,done\n"
            "        CALL plat_putc\n"
            "        INC HL\n"
            "        JR loop\n"
            "done:   DI\n"
            "        HALT\n"
            "plat_putc:\n"
            "        OUT (&01),A\n"
            "        RET\n"
            "msg:    DB \"Hallo Z80\",13,10,0\n";
        const Z80Run run = runZ80Source(src, Z80Platform::Test);
        CHECK(run.error.empty());
        CHECK(run.finished);
        CHECK_EQ(run.console, std::string("Hallo Z80\r\n"));
        CHECK(run.cycles > 0);
    }
    {  // cpc: TXT OUTPUT &BB5A, RET from the entry ends the program
        const char* src =
            "        ORG &4000\n"
            "        LD HL,msg\n"
            "loop:   LD A,(HL)\n"
            "        OR A\n"
            "        RET Z\n"
            "        CALL &BB5A\n"
            "        INC HL\n"
            "        JR loop\n"
            "msg:    DB \"Hallo CPC\",0\n";
        const Z80Run run = runZ80Source(src, Z80Platform::Cpc);
        CHECK(run.error.empty());
        CHECK(run.finished);
        CHECK_EQ(run.console, std::string("Hallo CPC"));
    }
    {  // an unknown firmware call is reported instead of silently running into RAM
        const Z80Run run = runZ80Source("        ORG &4000\n        CALL &BB18\n        RET\n", Z80Platform::Cpc);
        CHECK(!run.finished);
        CHECK(run.error.find("&BB18") != std::string::npos);
    }
    {  // assembler errors are passed on
        const Z80Run run = runZ80Source("        ORG &4000\n        LD Q,1\n", Z80Platform::Test);
        CHECK(!run.finished);
        CHECK(!run.error.empty());
    }
    {  // endless loop: instruction limit
        Z80Options o;
        o.maxInstructions = 1000;
        const Z80Run run = runZ80Source("        ORG &4000\nspin:   JR spin\n", Z80Platform::Test, o);
        CHECK(!run.finished);
        CHECK_EQ(run.error, std::string("stopped: instruction_limit at &4000"));
        CHECK_EQ(run.instructions, std::uint64_t{1000});
    }
    return report("harness_z80");
}
