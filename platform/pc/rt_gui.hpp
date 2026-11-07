#pragma once

// Requesters and menus of GFA-BASIC 3.5 (chapter 9; D26) for the interpreter and the C++ target,
// which have no windows: they become text dialogs on the keyboard stream of INPUT, so that programs
// can be tested with a prepared input (tests/programs/<name>.in).
//
//   ALERT    the text lines and the buttons, then "? "; the input is the number of a button; an
//            empty line, an invalid number or the end of the input choose the default button
//            (default 0: button 1).
//   FILESELECT  "titel [ok] pfad? "; the input is the file name ("" = cancel, also at the end).
//   FORM INPUT n AS a$  reads a line (at most n characters); an empty line keeps a$.
//   MENU m$()  lists the menu once ("menu: 1 Laden, 2 Speichern | ..."). ON MENU reads a line: the
//            number of an enabled entry selects it (MENU(0)); an empty line is a tick; the end of
//            the input ends the program (the usual DO ... ON MENU ... LOOP would never end).

#include <cstdint>
#include <functional>
#include <istream>
#include <string>
#include <vector>

#include "rt_pc.hpp"

namespace brt {

class Gui {
public:
    std::istream* in = nullptr;    // the keyboard stream (shared with INPUT)
    std::function<void()> flush;   // writes the collected output before waiting

    std::int32_t alert(Output& out, std::int32_t symbol, const std::string& text, std::int32_t def, const std::string& buttons);
    std::string fileselect(Output& out, const std::string& title, const std::string& ok, const std::string& path);
    std::string formInput(Output& out, std::int32_t n, const std::string& old);

    void menu(Output& out, const std::vector<std::string>& entries);  // MENU m$()
    void menuFlags(std::int32_t item, std::int32_t flags);              // MENU x,y
    void menuText(std::int32_t item, const std::string& s);             // MENU x,s$
    void menuKill();
    // ON MENU: true if an entry was chosen (MENU(0) set); the end of the input: EndOfInput
    bool onMenu(Output& out);
    std::int32_t value(std::int32_t x) const;  // MENU(x)

    struct EndOfInput {};

private:
    struct Entry {
        std::string text;
        bool title = false, selectable = false, enabled = true;
        std::int32_t flags = 16;  // ITEMENABLED
    };
    std::vector<Entry> entries_;
    std::int32_t chosen_ = -1, cls_ = 0;
    bool readLine(Output& out, const std::string& prompt, std::string& line);
};

}  // namespace brt
