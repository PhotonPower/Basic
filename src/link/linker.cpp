#include "linker.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace basic::link {

namespace {

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> r;
    std::istringstream in(s);
    std::string w;
    while (in >> w) r.push_back(w);
    return r;
}

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

bool identChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '$';
}

// The label defined on a source line, or "" (comments and strings are skipped).
std::string labelOf(const std::string& line) {
    if (line.empty() || line[0] == ';' || line[0] == '*') return "";
    if (line[0] != ' ' && line[0] != '\t') {  // label in column 1
        std::size_t k = 0;
        while (k < line.size() && identChar(line[k])) ++k;
        return line.substr(0, k);
    }
    // indented label: "  name: ..."
    std::size_t k = line.find_first_not_of(" \t");
    if (k == std::string::npos) return "";
    const std::size_t start = k;
    while (k < line.size() && identChar(line[k])) ++k;
    if (k > start && k < line.size() && line[k] == ':') return line.substr(start, k - start);
    return "";
}

}  // namespace

Module parseModule(const std::string& text, const std::string& file, Diagnostics& diags) {
    Module m;
    m.file = file;
    m.parts.push_back({Section::Code, ""});
    std::istringstream in(text);
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(";!", 0) == 0) {
            const std::vector<std::string> w = words(line.substr(2));
            if (w.empty()) continue;
            const std::string& key = w[0];
            std::vector<std::string> rest(w.begin() + 1, w.end());
            if (key == "module" && rest.size() == 1) m.name = rest[0];
            else if (key == "provides") m.provides.insert(m.provides.end(), rest.begin(), rest.end());
            else if (key == "requires") m.requires_.insert(m.requires_.end(), rest.begin(), rest.end());
            else if (key == "group" && rest.size() == 1) m.group = rest[0];
            else if (key == "doc") m.doc = trim(line.substr(line.find("doc") + 3));
            else if (key == "clobbers") {
            } else if (key == "section" && rest.size() == 1 && (rest[0] == "code" || rest[0] == "data" || rest[0] == "bss")) {
                m.parts.push_back({rest[0] == "code" ? Section::Code : rest[0] == "data" ? Section::Data : Section::Bss, ""});
            } else {
                diags.push_back({number, 0, file + ": unknown module header line: " + line});
            }
            continue;
        }
        m.parts.back().text += line + "\n";
    }
    if (m.name.empty()) diags.push_back({0, 0, file + ": ;! module missing"});
    if (m.provides.empty()) diags.push_back({0, 0, file + ": ;! provides missing"});
    return m;
}

std::vector<Module> loadModules(const std::string& dir, Diagnostics& diags) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".s") files.push_back(e.path());
    if (ec) diags.push_back({0, 0, "cannot read module directory " + dir});
    std::sort(files.begin(), files.end());
    std::vector<Module> modules;
    for (const fs::path& f : files) {
        std::ifstream in(f, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        modules.push_back(parseModule(ss.str(), f.filename().string(), diags));
    }
    return modules;
}

void checkLabels(const Module& m, Diagnostics& diags) {
    const std::set<std::string> provided(m.provides.begin(), m.provides.end());
    const std::string prefix = m.name + "__";
    // the Z80 assembler ignores the case of labels: two names that differ only in it collide
    const bool z80 = m.file.find("z80") != std::string::npos || m.file.find("cpc") != std::string::npos;
    std::map<std::string, std::string> folded;
    for (const Module::Part& part : m.parts) {
        std::istringstream in(part.text);
        std::string line;
        while (std::getline(in, line)) {
            const std::string l = labelOf(line);
            if (z80 && !l.empty()) {
                std::string f;
                for (char c : l) f += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                const auto [it, fresh] = folded.emplace(f, l);
                if (!fresh && it->second != l)
                    diags.push_back({0, 0, m.file + ": labels '" + it->second + "' and '" + l + "' differ only in case (the Z80 assembler ignores it)"});
            }
            if (l.empty() || l[0] == '.' || provided.count(l) != 0 || l.rfind(prefix, 0) == 0) continue;
            diags.push_back({0, 0, m.file + ": label '" + l + "' is neither provided nor prefixed with '" + prefix + "'"});
        }
    }
}

Library makeLibrary(std::vector<Module> modules, Diagnostics& diags) {
    Library lib;
    lib.modules = std::move(modules);
    for (std::size_t i = 0; i < lib.modules.size(); ++i) {
        for (const std::string& s : lib.modules[i].provides) {
            const auto [it, inserted] = lib.bySymbol.emplace(s, i);
            if (!inserted)
                diags.push_back({0, 0, "symbol '" + s + "' is provided by " + lib.modules[it->second].name + " and " + lib.modules[i].name});
        }
    }
    return lib;
}

Result link(const Library& lib, const Program& program, const std::vector<std::string>& roots, Layout layout,
            const std::string& header, Diagnostics& diags) {
    Result r;
    std::vector<std::size_t> order;
    std::set<std::size_t> taken;
    std::vector<std::pair<std::string, std::string>> work;  // (symbol, requested by)
    for (const std::string& s : roots) work.emplace_back(s, "platform");
    for (const std::string& s : program.uses) work.emplace_back(s, "program");
    std::set<std::string> programSymbols(program.provides.begin(), program.provides.end());
    if (layout == Layout::PlainZ80) {  // the start of the BSS and the first free byte after everything
        programSymbols.insert("prg_bss");
        programSymbols.insert("prg_end");
    }
    for (std::size_t k = 0; k < work.size(); ++k) {
        const auto [sym, by] = work[k];
        if (programSymbols.count(sym) != 0) continue;
        const auto it = lib.bySymbol.find(sym);
        if (it == lib.bySymbol.end()) {
            diags.push_back({0, 0, "runtime symbol '" + sym + "' (needed by " + by + ") is not provided by any module"});
            continue;
        }
        if (!taken.insert(it->second).second) continue;
        order.push_back(it->second);
        const Module& m = lib.modules[it->second];
        r.map += m.name + " <- " + sym + " (" + by + ")\n";
        for (const std::string& req : m.requires_) work.emplace_back(req, m.name);
    }
    for (std::size_t i : order) r.modules.push_back(lib.modules[i].name);

    auto partsOf = [&](Section sec, std::size_t from, std::size_t to) {
        std::string out;
        for (std::size_t k = from; k < to && k < order.size(); ++k) {
            const std::size_t i = order[k];
            const Module& m = lib.modules[i];
            std::string text;
            for (const Module::Part& p : m.parts)
                if (p.section == sec) text += p.text;
            if (trim(text).empty()) continue;
            out += "; ==== " + m.name + " (" + m.file + ")\n" + text;
        }
        return out;
    };
    // the entry module (first root) comes first, so that the program starts at its first byte
    const std::size_t entry = roots.empty() ? 0 : 1;
    const std::string code = partsOf(Section::Code, 0, entry) + "; ==== program\n" + program.code +
                             partsOf(Section::Code, entry, order.size());
    const std::string data = program.data + partsOf(Section::Data, 0, order.size());
    const std::string bss = program.bss + partsOf(Section::Bss, 0, order.size());
    std::string& s = r.source;
    s = header;
    if (layout == Layout::Sections) s += "        SECTION code,CODE\n";
    s += code;
    if (!trim(data).empty()) {
        s += layout == Layout::Sections ? "        SECTION data,DATA\n" : layout == Layout::PlainZ80 ? "" : "        EVEN\n";
        s += data;
    }
    if (layout == Layout::PlainZ80) s += "prg_bss:\n";
    if (!trim(bss).empty()) {
        s += layout == Layout::Sections ? "        SECTION bss,BSS\n" : layout == Layout::PlainZ80 ? "" : "        EVEN\n";
        s += bss;
    }
    if (layout == Layout::PlainZ80) s += "prg_end:\n";
    return r;
}

}  // namespace basic::link

#include "link/embedded.hpp"

namespace basic::link {

std::vector<Module> embeddedModulesOf(const std::vector<std::string>& dirs, Diagnostics& diags) {
    std::vector<Module> out;
    for (const std::string& dir : dirs)
        for (const EmbeddedModule& m : embeddedModules())
            if (dir == m.dir) out.push_back(parseModule(m.text, std::string(m.dir) + "/" + m.name, diags));
    return out;
}

}  // namespace basic::link
