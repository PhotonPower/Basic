#include "lexer.hpp"

#include <cctype>

namespace basic {

namespace {

bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.';
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isHexDigit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

std::string upper(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

}  // namespace

std::vector<Token> lexLine(std::string_view s, int lineNumber, Diagnostics& diags) {
    std::vector<Token> out;
    std::size_t i = 0;
    bool space = false;
    auto add = [&](Tok kind, std::string text, std::size_t start) {
        Token t;
        t.kind = kind;
        t.text = std::move(text);
        t.col = static_cast<int>(start) + 1;
        t.spaceBefore = space;
        out.push_back(std::move(t));
        space = false;
    };
    while (i < s.size()) {
        const char c = s[i];
        const std::size_t start = i;
        if (c == ' ' || c == '\t') {
            space = true;
            ++i;
            continue;
        }
        // comment at the start of the statement
        if (out.empty() && c == '\'') break;
        if (isIdentStart(c)) {
            while (i < s.size() && isIdentChar(s[i])) ++i;
            std::string name = upper(s.substr(start, i - start));
            if (out.empty() && name == "REM") break;
            if (out.empty() && name == "DATA") {  // raw contents, no '!' comment
                add(Tok::Ident, name, start);
                break;
            }
            // VAL? and DIM? are names with '?'
            if (i < s.size() && s[i] == '?' && (name == "VAL" || name == "DIM")) {
                name += '?';
                ++i;
                add(Tok::Ident, name, start);
                continue;
            }
            // type postfix directly after the name
            if (i < s.size()) {
                const char p = s[i];
                const bool radix = p == '&' && i + 1 < s.size() &&
                                   (std::toupper(static_cast<unsigned char>(s[i + 1])) == 'H' ||
                                    std::toupper(static_cast<unsigned char>(s[i + 1])) == 'O' ||
                                    std::toupper(static_cast<unsigned char>(s[i + 1])) == 'X') &&
                                   i + 2 < s.size() && isHexDigit(s[i + 2]);
                if ((p == '!' || p == '|' || p == '%' || p == '#' || p == '$' || (p == '&' && !radix))) {
                    name += p;
                    ++i;
                }
            }
            add(Tok::Ident, name, start);
            continue;
        }
        if (isDigit(c) || (c == '.' && i + 1 < s.size() && isDigit(s[i + 1]))) {
            while (i < s.size() && isDigit(s[i])) ++i;
            if (i < s.size() && s[i] == '.') {
                ++i;
                while (i < s.size() && isDigit(s[i])) ++i;
            }
            if (i < s.size() && (s[i] == 'E' || s[i] == 'e')) {
                std::size_t j = i + 1;
                if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
                if (j < s.size() && isDigit(s[j])) {
                    i = j;
                    while (i < s.size() && isDigit(s[i])) ++i;
                }
            }
            add(Tok::Number, std::string(s.substr(start, i - start)), start);
            continue;
        }
        // radix literals: &H.. &O.. &X.. $.. %..
        if (c == '&' && i + 2 <= s.size() && i + 1 < s.size()) {
            const char r = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i + 1])));
            int base = r == 'H' ? 16 : r == 'O' ? 8 : r == 'X' ? 2 : 0;
            if (base != 0) {
                std::size_t j = i + 2;
                while (j < s.size() && isHexDigit(s[j]) &&
                       (base == 16 || (s[j] - '0' >= 0 && s[j] - '0' < base)))
                    ++j;
                if (j > i + 2) {
                    i = j;
                    add(Tok::Number, std::string(s.substr(start, i - start)), start);
                    continue;
                }
            }
        }
        if (c == '$' && i + 1 < s.size() && isHexDigit(s[i + 1])) {
            ++i;
            while (i < s.size() && isHexDigit(s[i])) ++i;
            add(Tok::Number, std::string(s.substr(start, i - start)), start);
            continue;
        }
        if (c == '%' && i + 1 < s.size() && (s[i + 1] == '0' || s[i + 1] == '1')) {
            ++i;
            while (i < s.size() && (s[i] == '0' || s[i] == '1')) ++i;
            add(Tok::Number, std::string(s.substr(start, i - start)), start);
            continue;
        }
        if (c == '"') {
            ++i;
            std::string text;
            while (i < s.size() && s[i] != '"') text += s[i++];
            if (i >= s.size()) {
                diags.push_back({lineNumber, static_cast<int>(start) + 1, "unterminated string"});
            } else {
                ++i;
            }
            add(Tok::String, std::move(text), start);
            continue;
        }
        if (c == '!') break;  // comment to the end of the line
        // two-character operators (the aliases =< => >< are normalised here)
        if (i + 1 < s.size()) {
            const std::string two(s.substr(i, 2));
            std::string op;
            if (two == "<=" || two == "=<") op = "<=";
            else if (two == ">=" || two == "=>") op = ">=";
            else if (two == "<>" || two == "><") op = "<>";
            else if (two == "==") op = "==";
            if (!op.empty()) {
                i += 2;
                add(Tok::Punct, op, start);
                continue;
            }
        }
        static const std::string_view singles = "+-*/^\\=<>(),;':@#~.";
        if (singles.find(c) != std::string_view::npos) {
            ++i;
            add(Tok::Punct, std::string(1, c), start);
            continue;
        }
        diags.push_back({lineNumber, static_cast<int>(start) + 1, std::string("unexpected character '") + c + "'"});
        ++i;
    }
    Token end;
    end.kind = Tok::End;
    end.col = static_cast<int>(s.size()) + 1;
    out.push_back(end);
    return out;
}

std::vector<SourceLine> lexSource(std::string_view source, Diagnostics& diags) {
    std::vector<SourceLine> lines;
    std::size_t pos = 0;
    int number = 0;
    while (pos <= source.size()) {
        std::size_t eol = source.find_first_of("\r\n", pos);
        if (eol == std::string_view::npos) eol = source.size();
        SourceLine line;
        line.number = ++number;
        line.text = std::string(source.substr(pos, eol - pos));
        line.tokens = lexLine(line.text, line.number, diags);
        if (line.tokens.size() == 1) line.tokens.clear();  // blank or comment
        lines.push_back(std::move(line));
        if (eol >= source.size()) break;
        pos = eol + 1;
        if (source[eol] == '\r' && pos < source.size() && source[pos] == '\n') ++pos;
        if (pos >= source.size()) break;
    }
    return lines;
}

std::string formatDiagnostic(const std::string& file, const Diagnostic& d) {
    std::string r = file;
    if (d.line > 0) r += ":" + std::to_string(d.line);
    if (d.col > 0) r += ":" + std::to_string(d.col);
    return r + ": " + d.message;
}

}  // namespace basic
