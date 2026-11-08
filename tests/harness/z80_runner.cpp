#include "z80_runner.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>

#include "z80/assembler.hpp"
#include "z80/cpu.hpp"
#include "z80/json.hpp"
#include "z80/memory.hpp"
#include "z80/session.hpp"

namespace basic::harness {

namespace {

constexpr std::uint16_t kSentinel = 0x0000;     // return address of the entry: reaching it ends the run
constexpr std::uint16_t kCpcStackTop = 0xBFF0;  // below the screen; the firmware area is not used by the stubs
constexpr std::uint16_t kTestStackTop = 0xFFF0;
constexpr std::uint16_t kFirmwareFirst = 0xB900;  // CPC firmware jump blocks
constexpr std::uint16_t kFirmwareLast = 0xBDFF;
constexpr std::uint16_t kTxtOutput = 0xBB5A;
constexpr std::uint16_t kKmWaitChar = 0xBB06;
constexpr std::uint16_t kKmReadChar = 0xBB09;
constexpr std::uint16_t kKlTimePlease = 0xBD0D;
constexpr std::uint16_t kTxtSetCursor = 0xBB75;
constexpr std::uint16_t kTxtClearWindow = 0xBB6C;
constexpr std::uint16_t kCasInOpen = 0xBC77, kCasInClose = 0xBC7A, kCasInAbandon = 0xBC7D, kCasInChar = 0xBC80;
constexpr std::uint16_t kCasOutOpen = 0xBC8C, kCasOutClose = 0xBC8F, kCasOutAbandon = 0xBC92, kCasOutChar = 0xBC95;
constexpr std::uint16_t kKlFindCommand = 0xBCD4, kKlFarPchl = 0x001B;
constexpr std::uint16_t kEraAddress = 0xC123;  // what KL FIND COMMAND returns for |ERA (only called through KL FAR PCHL)
constexpr std::uint8_t kTestConsolePort = 0x01;
constexpr std::uint8_t kTestFilePort = 0x10;

// z80-test files: a RAM disk as on m68k-test (platform/m68k-test/plat_file.s). The program writes a
// request block and OUT (C),L with B = H, C = &10 (block address HL): +0 command, +1 mode or
// handle, +2 status (0 or the AmigaDOS error), +4 name address or byte, +6 name length; results
// (handle, byte, size) as 32 bits at +4.
// cpc files: AMSDOS as the firmware calls see it. One input and one output file; files written with
// CAS OUT CHAR are ASCII files without a header (reading ends at the first &1A); CAS OUT CLOSE turns
// an older file of the same name into NAME.BAK; |ERA deletes (no result, as on the CPC).
class Amsdos {
public:
    // address: a stubbed call -> true if handled (registers set as the firmware returns)
    bool call(z80::Word address, z80::Registers& r, z80::Memory& m) {
        auto ok = [&](bool good, z80::Byte a) {  // CF set = success; on failure Z clear (no ESC), A = code
            r.setAf(static_cast<z80::Word>(a << 8 | (good ? 0x01 : 0x00)));
        };
        switch (address) {
            case kCasInOpen: {
                const std::string n = name(m, r.hl(), r.bc() >> 8);
                const auto it = files_.find(n);
                if (in_ || it == files_.end()) return ok(false, 0x92), true;
                in_ = true;
                inData_ = it->second;
                inPos_ = 0;
                r.setBc(static_cast<z80::Word>(inData_.size()));
                return ok(true, 0x16), true;  // ASCII file
            }
            case kCasInChar:
                if (!in_ || inPos_ >= inData_.size() || inData_[inPos_] == 0x1A) return ok(false, 0x1A), true;
                return ok(true, inData_[inPos_++]), true;
            case kCasInClose:
            case kCasInAbandon:
                in_ = false;
                return ok(true, 0), true;
            case kCasOutOpen:
                if (out_) return ok(false, 0x0E), true;
                out_ = true;
                outName_ = name(m, r.hl(), r.bc() >> 8);
                outData_.clear();
                return ok(true, 0), true;
            case kCasOutChar:
                if (!out_) return ok(false, 0x0E), true;
                outData_.push_back(r.a());
                return ok(true, r.a()), true;
            case kCasOutClose:
                if (out_) {
                    if (const auto it = files_.find(outName_); it != files_.end()) {
                        const std::size_t dot = outName_.find('.');
                        files_[outName_.substr(0, dot) + ".BAK"] = it->second;
                    }
                    files_[outName_] = outData_;
                }
                out_ = false;
                return ok(true, 0), true;
            case kCasOutAbandon:
                out_ = false;
                return ok(true, 0), true;
            case kKlFindCommand: {  // HL -> name, the last character with bit 7
                std::string n;
                for (z80::Word a = r.hl(); n.size() < 16; ++a) {
                    n += static_cast<char>(m.read(a) & 0x7F);
                    if (m.read(a) & 0x80) break;
                }
                if (n != "ERA") return ok(false, 0), true;
                r.setHl(kEraAddress);
                r.setBc(static_cast<z80::Word>((r.bc() & 0xFF00) | 7));
                return ok(true, 0), true;
            }
            case kKlFarPchl: {  // only |ERA: A = 1, IX -> address of the string descriptor (length, address)
                if (r.hl() != kEraAddress || r.a() != 1) return false;
                const z80::Word d = static_cast<z80::Word>(m.read(r.ix) | m.read(static_cast<z80::Word>(r.ix + 1)) << 8);
                files_.erase(name(m, static_cast<z80::Word>(m.read(static_cast<z80::Word>(d + 1)) | m.read(static_cast<z80::Word>(d + 2)) << 8), m.read(d)));
                return true;
            }
            default:
                return false;
        }
    }

private:
    static std::string name(z80::Memory& m, z80::Word a, unsigned n) {
        std::string s;
        for (unsigned k = 0; k < n; ++k) s += static_cast<char>(std::toupper(m.read(static_cast<z80::Word>(a + k))));
        return s;
    }
    std::map<std::string, std::vector<std::uint8_t>> files_;
    bool in_ = false, out_ = false;
    std::vector<std::uint8_t> inData_, outData_;
    std::size_t inPos_ = 0;
    std::string outName_;
};

class RamDisk {
public:
    void request(z80::Memory& m, z80::Word block) {
        auto rd = [&](unsigned k) { return m.read(static_cast<z80::Word>(block + k)); };
        auto rw = [&](unsigned k) { return static_cast<unsigned>(rd(k) | rd(k + 1) << 8); };
        auto result = [&](unsigned status, std::uint32_t v) {
            m.write(static_cast<z80::Word>(block + 2), static_cast<z80::Byte>(status));
            for (unsigned k = 0; k < 4; ++k) m.write(static_cast<z80::Word>(block + 4 + k), static_cast<z80::Byte>(v >> (8 * k)));
        };
        std::string name;
        for (unsigned k = 0; k < rw(6) && k < 32; ++k)
            name += static_cast<char>(std::toupper(m.read(static_cast<z80::Word>(rw(4) + k))));
        const unsigned arg = rd(1);
        Handle* h = arg >= 1 && arg <= kHandles && handles_[arg - 1].used ? &handles_[arg - 1] : nullptr;
        switch (rd(0)) {
            case 1: {  // open: mode 0 read, 1 write, 2 append
                auto it = files_.find(name);
                if (arg == 0 && it == files_.end()) return result(205, 0);  // object not found
                if (it != files_.end() && (it->second.writer || (arg != 0 && it->second.readers != 0))) return result(202, 0);
                unsigned k = 0;
                while (k < kHandles && handles_[k].used) ++k;
                if (k == kHandles) return result(221, 0);
                File& f = files_[name];
                if (arg == 1) f.data.clear();
                handles_[k] = {true, name, arg == 2 ? f.data.size() : 0, arg != 0};
                (arg == 0 ? f.readers : f.writer) += 1;
                return result(0, k + 1);
            }
            case 2: {  // getc: status 1 at the end
                if (h == nullptr) return result(1, 0);
                const File& f = files_[h->name];
                if (h->pos >= f.data.size()) return result(1, 0);
                return result(0, f.data[h->pos++]);
            }
            case 3: {  // putc
                if (h == nullptr) return result(221, 0);
                File& f = files_[h->name];
                if (h->pos >= kCapacity) return result(221, 0);  // disk full
                if (h->pos >= f.data.size()) f.data.resize(h->pos + 1);
                f.data[h->pos++] = rd(4);
                return result(0, 0);
            }
            case 4:  // close
                if (h != nullptr) {
                    File& f = files_[h->name];
                    (h->write ? f.writer : f.readers) -= 1;
                    h->used = false;
                }
                return result(0, 0);
            case 5:  // size
                return result(0, h == nullptr ? 0 : static_cast<std::uint32_t>(files_[h->name].data.size()));
            case 6:  // exists: 1 or 0
                return result(0, files_.count(name) != 0 ? 1 : 0);
            case 7: {  // delete
                auto it = files_.find(name);
                if (it == files_.end()) return result(205, 0);
                if (it->second.readers != 0 || it->second.writer != 0) return result(202, 0);
                files_.erase(it);
                return result(0, 0);
            }
            default:
                return result(255, 0);
        }
    }

private:
    static constexpr unsigned kHandles = 16;
    static constexpr std::size_t kCapacity = 16384;
    struct File {
        std::vector<std::uint8_t> data;
        int readers = 0, writer = 0;
    };
    struct Handle {
        bool used = false;
        std::string name;
        std::size_t pos = 0;
        bool write = false;
    };
    std::map<std::string, File> files_;
    Handle handles_[kHandles];
};

std::string hex4(unsigned v) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "&%04X", v & 0xFFFFu);
    return buf;
}

}  // namespace

namespace {
// address -> routine name (the label at or before it; "module__x" -> "module", program code -> "program")
using Routines = std::map<std::uint32_t, std::string>;
thread_local const Routines* g_routines = nullptr;
}  // namespace

Z80Run runZ80Image(const std::vector<std::uint8_t>& image, Z80Platform platform, const Z80Options& options) {
    Z80Run result;
    if (image.size() + options.origin > 0x10000) {
        result.error = "image does not fit into 64 KiB";
        return result;
    }
    z80::Memory memory;
    if (options.fill != 0)
        for (unsigned a = 0; a < 0x10000; ++a) memory.write(static_cast<z80::Word>(a), options.fill);
    for (std::size_t i = 0; i < image.size(); ++i)
        memory.write(static_cast<z80::Word>(options.origin + i), image[i]);

    z80::Cpu cpu(memory);
    z80::Registers& r = cpu.regs();
    r.pc = options.origin;
    r.sp = platform == Z80Platform::Cpc ? kCpcStackTop : kTestStackTop;

    std::string firmwareError;
    if (platform == Z80Platform::Test) {
        cpu.setPortHandlers([&options, inputPos = std::size_t{0}](z80::Word port) mutable -> z80::Byte {
                                if ((port & 0xFF) == 0x03) return inputPos < options.input.size() ? 1 : 0;
                                if ((port & 0xFF) == 0x02 && inputPos < options.input.size())
                                    return static_cast<z80::Byte>(options.input[inputPos++]);
                                return 0xFF;
                            },
                            [&result, &memory, disk = RamDisk{}](z80::Word port, z80::Byte value) mutable {
                                if ((port & 0xFF) == kTestConsolePort) result.console += static_cast<char>(value);
                                if ((port & 0xFF) == kTestFilePort) disk.request(memory, static_cast<z80::Word>((port & 0xFF00) | value));
                            });
    } else {
        // The program is CALLed like from BASIC: RET from the entry lands on the sentinel.
        r.sp = static_cast<z80::Word>(r.sp - 2);
        memory.write(r.sp, static_cast<z80::Byte>(kSentinel & 0xFF));
        memory.write(static_cast<z80::Word>(r.sp + 1), static_cast<z80::Byte>(kSentinel >> 8));
        cpu.addBreakpoint(kSentinel);
        cpu.setExecHook(kKlFarPchl, kFirmwareLast, [&, inputPos = std::size_t{0}, time = std::uint32_t{0}, dos = Amsdos{}](z80::Cpu& c) mutable {
            z80::Registers& regs = c.regs();
            const z80::Word address = regs.pc;
            if (address < kFirmwareFirst && address != kKlFarPchl) return false;  // the program itself
            if (dos.call(address, regs, c.memory())) {
            } else if (address == kTxtOutput) {
                result.console += static_cast<char>(regs.a());
            } else if (address == kKmWaitChar || address == kKmReadChar) {
                // the next byte of the input with CF set; at its end CF clear (KM WAIT CHAR never
                // returns that on a real CPC; plat_getc takes it as the end of the input)
                const bool have = inputPos < options.input.size();
                const z80::Byte a = have ? static_cast<z80::Byte>(options.input[inputPos++]) : regs.a();
                regs.setAf(static_cast<z80::Word>(a << 8 | ((regs.f() & ~1) | (have ? 1 : 0))));
            } else if (address == kTxtSetCursor) {  // written as the Amiga console does (ESC [ line ; column H)
                result.console += "\x1b[" + std::to_string(regs.hl() & 0xFF) + ";" + std::to_string(regs.hl() >> 8) + "H";
            } else if (address == kTxtClearWindow) {
                result.console += "\x1b[H\x1b[J";
            } else if (address == kKlTimePlease) {  // a clock that advances by 3 (1/100 s) per call
                time += 3;
                regs.setDe(static_cast<z80::Word>(time >> 16));
                regs.setHl(static_cast<z80::Word>(time));
            } else if (firmwareError.empty()) {
                firmwareError = "firmware call " + hex4(address) + " is not emulated";
                regs.pc = kSentinel;  // stop at the sentinel breakpoint
                return true;
            }
            // emulate RET
            const z80::Word ret = static_cast<z80::Word>(c.memory().read(regs.sp) |
                                                         (c.memory().read(static_cast<z80::Word>(regs.sp + 1)) << 8));
            regs.sp = static_cast<z80::Word>(regs.sp + 2);
            regs.pc = ret;
            return true;
        });
    }

    std::map<std::string, std::uint64_t> spent;
    if (g_routines != nullptr) {
        cpu.setTrace([&spent, last = std::uint64_t{0}, where = std::string()](const z80::Cpu& c) mutable {
            if (!where.empty()) spent[where] += c.cycles() - last;
            last = c.cycles();
            auto it = g_routines->upper_bound(c.regs().pc);
            where = it == g_routines->begin() ? "?" : std::prev(it)->second;
        });
    }
    z80::RunLimits limits;
    limits.maxInstructions = options.maxInstructions;
    const z80::RunResult run = cpu.run(limits);
    if (g_routines != nullptr) {
        std::vector<std::pair<std::uint64_t, std::string>> top;
        std::uint64_t total = 0;
        for (const auto& [name, t] : spent) {
            top.emplace_back(t, name);
            total += t;
        }
        std::sort(top.rbegin(), top.rend());
        char buf[160];
        for (std::size_t k = 0; k < top.size() && k < 30; ++k) {
            std::snprintf(buf, sizeof buf, "%14llu %5.1f%%  %s\n", static_cast<unsigned long long>(top[k].first),
                          total ? 100.0 * static_cast<double>(top[k].first) / static_cast<double>(total) : 0.0, top[k].second.c_str());
            result.profile += buf;
        }
    }
    result.instructions = cpu.instructionCount();  // since reset: this run only
    result.cycles = cpu.cycles();

    if (!firmwareError.empty()) {
        result.error = firmwareError;
    } else if (platform == Z80Platform::Test && run.reason == z80::StopReason::Halted) {
        result.finished = true;
    } else if (platform == Z80Platform::Cpc && run.reason == z80::StopReason::Breakpoint && run.pc == kSentinel) {
        result.finished = true;
    } else {
        result.error = std::string("stopped: ") + z80::toString(run.reason) + " at " + hex4(run.pc);
    }
    return result;
}

Z80Run runZ80Source(const std::string& source, Z80Platform platform, const Z80Options& options) {
    z80::AssemblerOptions asmOptions;
    asmOptions.origin = options.origin;
    const z80::AssemblyResult assembled = z80::Assembler(asmOptions).assemble(source);
    if (!assembled.ok()) {
        Z80Run failed;
        failed.error = assembled.formatDiagnostics("<source>");
        return failed;
    }
    if (assembled.empty()) return runZ80Image({}, platform, options);
    if (assembled.lowAddress() != options.origin) {
        Z80Run failed;
        failed.error = "the program must start at the origin " + hex4(options.origin);
        return failed;
    }
    std::vector<std::uint8_t> image = assembled.flatten();
    if (options.withoutBss)
        for (const auto& [name, sym] : assembled.symbols)
            if ((name == "PRG_BSS" || name == "prg_bss") && sym.value >= options.origin)
                image.resize(std::min<std::size_t>(image.size(), static_cast<std::size_t>(sym.value - options.origin)));
    Routines routines;
    if (options.profile) {
        for (const auto& [name, sym] : assembled.symbols) {
            std::string n = name;
            for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (const auto u = n.find("__"); u != std::string::npos) n.resize(u);
            if (n.rfind("rt_", 0) != 0 && n.rfind("fp_", 0) != 0 && n.rfind("zi", 0) != 0 && n.rfind("ix_", 0) != 0 &&
                n.rfind("plat_", 0) != 0)
                n = "program";
            routines[static_cast<std::uint32_t>(sym.value)] = n;
        }
        g_routines = &routines;
    }
    Z80Run run = runZ80Image(image, platform, options);
    g_routines = nullptr;
    for (const auto& [name, sym] : assembled.symbols)
        if (name == "PRG_END" || name == "prg_end") run.end = static_cast<std::uint32_t>(sym.value);
    return run;
}

// ---- the real firmware --------------------------------------------------------------------------

struct CpcFirmware::Impl {
    z80::Session session{z80::SessionOptions{true}};
    z80::Json call(const std::string& line) { return z80::Json::parse(session.handleLine(line)); }
    static bool ok(const z80::Json& j) {
        const z80::Json* o = j.find("ok");
        return o != nullptr && o->asBool();
    }
};

CpcFirmware::CpcFirmware(const std::string& romFile) : impl_(new Impl) {
    auto step = [&](const std::string& line) {
        const z80::Json r = impl_->call(line);
        if (!Impl::ok(r) && error_.empty()) error_ = line + ": " + r.dump();
        return r;
    };
    step(R"({"cmd":"machine","type":"cpc"})");
    z80::Json load = z80::Json::object();
    load.set("cmd", "load_rom").set("file", romFile).set("which", "both");
    step(load.dump());
    step(R"({"cmd":"set","regs":{"PC":0,"SP":"0xBFFF"}})");
    step(R"({"cmd":"run","irq_every_cycles":13312,"max_instructions":30000000})");  // to the BASIC prompt
    step(R"({"cmd":"checkpoint","name":"booted"})");
    const z80::Json probe = step(R"({"cmd":"call","addr":"0xB915","regs":{"C":0},"max_instructions":100000})");  // KL PROBE ROM
    if (const z80::Json* regs = probe.find("regs"))
        if (const z80::Json* hl = regs->find("HL")) version_ = static_cast<int>(std::stoul(hl->asString(), nullptr, 0) >> 8);
    step(R"({"cmd":"rollback","name":"booted"})");
    booted_ = error_.empty();
}

CpcFirmware::~CpcFirmware() { delete impl_; }

Z80Run CpcFirmware::run(const std::string& source, std::uint16_t origin, std::uint64_t maxInstructions,
                        const std::string& screenPng) {
    Z80Run result;
    z80::AssemblerOptions asmOptions;
    asmOptions.origin = origin;
    const z80::AssemblyResult assembled = z80::Assembler(asmOptions).assemble(source);
    if (!assembled.ok()) {
        result.error = assembled.formatDiagnostics("<source>");
        return result;
    }
    std::vector<std::uint8_t> image = assembled.flatten();
    for (const auto& [name, sym] : assembled.symbols) {
        if ((name == "PRG_BSS" || name == "prg_bss") && sym.value >= origin)
            image.resize(std::min<std::size_t>(image.size(), static_cast<std::size_t>(sym.value - origin)));
        if (name == "PRG_END" || name == "prg_end") result.end = static_cast<std::uint32_t>(sym.value);
    }
    impl_->call(R"({"cmd":"rollback","name":"booted"})");
    for (std::size_t k = 0; k < image.size(); ++k) impl_->session.memory().write(static_cast<z80::Word>(origin + k), image[k]);
    z80::Cpu& cpu = impl_->session.cpu();
    cpu.setExecHook(kTxtOutput, kTxtSetCursor, [&result](z80::Cpu& c) {  // watch, the firmware does the work
        const z80::Registers& r = c.regs();
        if (r.pc == kTxtOutput) result.console += static_cast<char>(r.a());
        else if (r.pc == kTxtSetCursor) result.console += "\x1b[" + std::to_string(r.hl() & 0xFF) + ";" + std::to_string(r.hl() >> 8) + "H";
        else if (r.pc == kTxtClearWindow) result.console += "\x1b[H\x1b[J";
        return false;
    });
    const z80::Json off = impl_->call(R"({"cmd":"call","addr":"0xB909","irq_every_cycles":13312,"max_instructions":100000})");  // KL L ROM DISABLE
    if (!Impl::ok(off)) {
        cpu.clearExecHook();
        result.error = "KL L ROM DISABLE: " + off.dump();
        return result;
    }
    const std::uint64_t before = cpu.instructionCount();
    const z80::Json r = impl_->call("{\"cmd\":\"call\",\"addr\":" + std::to_string(origin) +
                                    ",\"irq_every_cycles\":13312,\"max_instructions\":" + std::to_string(maxInstructions) + "}");
    cpu.clearExecHook();
    result.instructions = cpu.instructionCount() - before;
    const z80::Json* returned = r.find("returned");
    result.finished = Impl::ok(r) && returned != nullptr && returned->asBool();
    if (!result.finished) result.error = "did not return: " + r.dump().substr(0, 400);
    if (!screenPng.empty()) {
        const z80::Json shot = impl_->call(R"({"cmd":"screen"})");
        if (const z80::Json* b64 = shot.find("png_base64")) {
            static const std::string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string bytes;
            unsigned acc = 0;
            int bits = 0;
            for (char ch : b64->asString()) {
                const std::size_t v = abc.find(ch);
                if (v == std::string::npos) continue;
                acc = acc << 6 | static_cast<unsigned>(v);
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    bytes += static_cast<char>((acc >> bits) & 0xFF);
                }
            }
            if (std::FILE* f = std::fopen(screenPng.c_str(), "wb")) {
                std::fwrite(bytes.data(), 1, bytes.size(), f);
                std::fclose(f);
            }
        }
    }
    return result;
}

}  // namespace basic::harness
