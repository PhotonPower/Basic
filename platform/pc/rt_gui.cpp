#include "rt_gui.hpp"

#include <cstdlib>

namespace brt {

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> r(1);
    for (char c : s) {
        if (c == '|') r.emplace_back();
        else r.back() += c;
    }
    return r;
}

std::string trimmed(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

}  // namespace

bool Gui::readLine(Output& out, const std::string& prompt, std::string& line) {
    out.put(prompt);
    if (flush) flush();
    line.clear();
    if (in == nullptr || !std::getline(*in, line)) {
        out.newline();
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    out.col = 0;  // the Return of the user moved the cursor to the next line
    return true;
}

std::int32_t Gui::alert(Output& out, std::int32_t, const std::string& text, std::int32_t def, const std::string& buttons) {
    for (const std::string& l : split(text)) {
        out.put(l);
        out.newline();
    }
    const std::vector<std::string> b = split(buttons);
    const auto n = static_cast<std::int32_t>(b.size());
    std::string prompt = "<";
    for (std::int32_t k = 0; k < n; ++k) prompt += (k ? "|" : "") + std::to_string(k + 1) + ":" + b[static_cast<std::size_t>(k)];
    prompt += ">? ";
    const std::int32_t fallback = def >= 1 && def <= n ? def : 1;
    std::string line;
    if (!readLine(out, prompt, line)) return fallback;
    const std::string t = trimmed(line);
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (t.empty() || *end != '\0' || v < 1 || v > n) return fallback;
    return static_cast<std::int32_t>(v);
}

std::string Gui::fileselect(Output& out, const std::string& title, const std::string& ok, const std::string& path) {
    std::string line;
    if (!readLine(out, title + " [" + ok + "] " + path + "? ", line)) return "";
    return trimmed(line);
}

std::string Gui::formInput(Output& out, std::int32_t n, const std::string& old) {
    std::string line;
    if (!readLine(out, "", line) || line.empty()) return old;
    if (n >= 1 && static_cast<std::int32_t>(line.size()) > n) line.resize(static_cast<std::size_t>(n));
    return line;
}

// The array: per menu a title, its entries, an empty string; after the last menu one more empty
// string. "-" first: a disabled entry; the index of the array is the number of the entry.
void Gui::menu(Output& out, const std::vector<std::string>& m) {
    entries_.clear();
    bool title = true;
    for (std::size_t k = 0; k < m.size(); ++k) {
        Entry e;
        e.text = m[k];
        if (m[k].empty()) {
            entries_.push_back(e);
            if (title) break;  // two empty strings: the end of the whole menu
            title = true;
            continue;
        }
        e.title = title;
        e.selectable = !title && m[k][0] != '-';
        e.enabled = e.selectable;
        title = false;
        entries_.push_back(e);
    }
    std::string list = "menu:";
    bool first = true;
    for (std::size_t k = 0; k < entries_.size(); ++k) {
        const Entry& e = entries_[k];
        if (e.text.empty()) continue;
        if (e.title) list += first ? " " + e.text + ":" : " | " + e.text + ":";
        else list += " " + std::to_string(k) + " " + e.text;
        first = false;
    }
    out.put(list);
    out.newline();
}

void Gui::menuFlags(std::int32_t item, std::int32_t flags) {
    if (item < 0 || item >= static_cast<std::int32_t>(entries_.size())) return;
    Entry& e = entries_[static_cast<std::size_t>(item)];
    e.flags = flags;
    e.enabled = e.selectable && (flags & 16) != 0;  // ITEMENABLED
}

void Gui::menuText(std::int32_t item, const std::string& s) {
    if (item < 0 || item >= static_cast<std::int32_t>(entries_.size())) return;
    std::string& t = entries_[static_cast<std::size_t>(item)].text;
    t = s.substr(0, t.size());  // not longer than the old text (reference)
}

void Gui::menuKill() {
    entries_.clear();
    chosen_ = -1;
}

bool Gui::onMenu(Output& out) {
    std::string line;
    if (!readLine(out, "", line)) throw EndOfInput{};
    const std::string t = trimmed(line);
    if (t.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (*end != '\0' || v < 0 || v >= static_cast<long>(entries_.size())) return false;
    if (!entries_[static_cast<std::size_t>(v)].enabled) return false;
    chosen_ = static_cast<std::int32_t>(v);
    cls_ = 256;  // MENUPICK
    return true;
}

std::int32_t Gui::value(std::int32_t x) const {
    if (x == 0) return chosen_;
    if (x == 1) return cls_;
    return 0;
}

}  // namespace brt
