#pragma once

// Graphics of GFA-BASIC 3.5 (chapters 8 and 9, D25) for the interpreter and the C++ target: screens
// and windows as pixel fields of colour numbers, one drawing pen (COLOR and DEFFILL both set it,
// probe23 T3), the raster rules measured on the original (probe22-24). At the end of the program
// the windows can be written as a picture (PPM) and as a digest for the tests.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "rt_ref.h"

namespace brt {

class Gfx {
public:
    Gfx();

    // screens and windows (D25: window 0 is open from the start, 640 x 256 HiRes, 4 colours)
    void openScreen(int n, const std::vector<std::int32_t>& args);  // OPENS nr[,x,y,w,h,bitp[,mod]]
    void closeScreen(int n);
    void openWindow(int n, const std::vector<std::int32_t>& args);  // OPENW nr[,x,y,w,h,idcmp,flags[,scr]]
    void closeWindow(int n);
    void clearWindow(int n);    // CLEARW
    void cls();                 // CLS of the current window: text and graphics (probe22 T9)
    void titleWindow(int n, const std::string& t);
    void titleScreen(int n, const std::string& t);

    // pens and modes
    void color(const std::vector<std::int32_t>& v, unsigned present);  // COLOR front,back,outline
    void deffill(const std::vector<std::int32_t>& v, unsigned present);  // DEFFILL colour,style,pattern
    void setcolor(const std::vector<std::int32_t>& v);  // SETCOLOR reg,r,g,b / reg,rgb
    void graphmode(std::int32_t m);
    void boundary(std::int32_t on) { boundary_ = on != 0; }
    void defline(std::int32_t d);

    // drawing (coordinates in the inner area of the current window, clipped there)
    void plot(std::int32_t x, std::int32_t y);
    void line(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2);
    void drawTo(std::int32_t x, std::int32_t y);  // DRAW TO: from the last point
    void box(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2, bool filled);
    void ellipse(std::int32_t x, std::int32_t y, std::int32_t rx, std::int32_t ry, bool filled);
    void circle(std::int32_t x, std::int32_t y, std::int32_t r, bool filled);
    void text(std::int32_t x, std::int32_t y, const std::string& s);
    std::int32_t point(std::int32_t x, std::int32_t y) const;  // -1 outside (probe22 T1)

    // A graphics command by name (HIR cmd): v = its numeric arguments from left to right, str = the
    // string argument (TEXT, TITLEW, TITLES), mode = the HIR mode (arguments given; DRAW: 1 "DRAW TO").
    // false: no graphics command.
    static bool isCommand(const std::string& name);
    void command(const std::string& name, const std::vector<std::int32_t>& v, const std::string& str, int mode);

    // results: a digest (one line per window, pixels as FNV-1a hash, texts) and a PPM picture
    std::string digest() const;
    bool writePpm(const std::string& path) const;
    bool used() const { return used_; }

private:
    struct Screen {
        bool open = false;
        int width = 640, height = 256, depth = 2;
        std::array<std::uint16_t, 64> palette{};
        std::string title;
    };
    struct Text {
        std::int32_t x, y;
        int color;
        std::string s;
    };
    struct Window {
        bool open = false;
        int screen = 0;
        int x = 0, y = 0, width = 0, height = 0;  // outer, on the screen
        int left = 4, top = 11, right = 4, bottom = 2;  // borders (probe22 T0: 4, 11)
        std::vector<std::uint8_t> pix;  // inner area, row by row
        std::vector<Text> texts;
        std::string title;
        int innerW() const { return width - left - right; }
        int innerH() const { return height - top - bottom; }
    };
    std::array<Screen, 8> screens_{};
    std::array<Window, 16> windows_{};
    int current_ = 0;
    int apen_ = 1, bpen_ = 0, open_ = 1;
    int mode_ = 0;  // GRAPHMODE: 0 JAM1, 1 JAM2, 2 COMPLEMENT (bits), 4 INVERSVID
    std::uint16_t pattern_ = 0xFFFF;
    int fillStyle_ = 1;
    bool boundary_ = false;  // off at the start (probe23 T3)
    bool firstDot_ = true;   // FRST_DOT of graphics.library (probe23 T4)
    std::int32_t penX_ = 0, penY_ = 0;
    bool used_ = false;

    Window& win();
    const Window& win() const;
    int depthMask() const;
    void pixel(std::int32_t x, std::int32_t y, int color);          // JAM1/JAM2: set
    void patternPixel(std::int32_t x, std::int32_t y, bool bit);    // line pixel by mode and pattern bit
    void segment(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2);
    void span(std::int32_t x1, std::int32_t x2, std::int32_t y, int color);
    static std::vector<std::pair<std::int32_t, std::int32_t>> ellipsePoints(std::int32_t rx, std::int32_t ry);
};

}  // namespace brt
