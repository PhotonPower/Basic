#include "link/availability.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>

namespace basic::link {

namespace {

// Can the library provide `symbol` with everything its module requires? Memoised; a symbol on a
// cycle counts as available while it is being checked.
bool available(const Library& lib, const std::string& symbol, std::map<std::string, bool>& memo) {
    if (const auto it = memo.find(symbol); it != memo.end()) return it->second;  // also the program's own
    const auto m = lib.bySymbol.find(symbol);
    if (m == lib.bySymbol.end()) return memo[symbol] = false;
    memo[symbol] = true;
    for (const std::string& r : lib.modules[m->second].requires_)
        if (!available(lib, r, memo)) return memo[symbol] = false;
    return true;
}

std::string groupOf(const Library& lib, const Library& reference, const std::string& symbol) {
    for (const Library* l : {&lib, &reference})
        if (const auto m = l->bySymbol.find(symbol); m != l->bySymbol.end() && !l->modules[m->second].group.empty())
            return l->modules[m->second].group;
    return "";
}

}  // namespace

void checkAvailability(const lir::Program& program, const Program& code, const Library& lib,
                       const Library& reference, std::string_view target, Diagnostics& diags) {
    std::map<std::string, bool> memo;
    for (const std::string& s : code.provides) memo[s] = true;
    for (const char* s : {"prg_bss", "prg_end"}) memo[s] = true;  // set by the layout (Layout::PlainZ80)
    const std::set<std::string> uses(code.uses.begin(), code.uses.end());
    std::set<std::pair<int, std::string>> reported;
    for (const lir::Function& f : program.functions) {
        int line = 0;
        for (const lir::Instr& i : f.code) {
            if (i.kind == lir::Kind::Line) line = i.imm;
            if (i.kind != lir::Kind::RtCall || uses.count(i.op) == 0 || available(lib, i.op, memo)) continue;
            const std::string what = i.text.empty() ? "the runtime routine " + i.op : i.text;
            if (!reported.emplace(line, what).second) continue;
            const std::string group = groupOf(lib, reference, i.op);
            diags.push_back({line, 0, what + " is not available on the target " + std::string(target) +
                                          (group.empty() ? std::string() : " (group " + group + ")")});
        }
    }
}

}  // namespace basic::link
