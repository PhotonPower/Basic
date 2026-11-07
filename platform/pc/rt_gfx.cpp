#include "rt_gfx.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

namespace brt {

namespace {

// Kickstart 1.x default colours (Workbench): blue, white, black, orange
constexpr std::array<std::uint16_t, 4> kDefaultPalette = {0x05A, 0xFFF, 0x002, 0xF80};

}  // namespace

Gfx::Gfx() {
    Screen& s = screens_[0];
    s.open = true;
    for (std::size_t k = 0; k < kDefaultPalette.size(); ++k) s.palette[k] = kDefaultPalette[k];
    // window 0: the whole screen without the screen title (probe22 T0: 640 x 245)
    openWindow(0, {});
    used_ = false;
}

Gfx::Window& Gfx::win() { return windows_[static_cast<std::size_t>(current_)]; }
const Gfx::Window& Gfx::win() const { return windows_[static_cast<std::size_t>(current_)]; }
int Gfx::depthMask() const { return (1 << screens_[static_cast<std::size_t>(win().screen)].depth) - 1; }

void Gfx::openScreen(int n, const std::vector<std::int32_t>& a) {
    if (n < 0 || n >= static_cast<int>(screens_.size())) return;
    Screen& s = screens_[static_cast<std::size_t>(n)];
    if (!s.open) {
        s = Screen{};
        s.open = true;
        if (a.size() >= 4) {
            s.width = a[2];
            s.height = a[3];
        }
        if (a.size() >= 5) s.depth = std::clamp<std::int32_t>(a[4], 1, 6);
        for (std::size_t k = 0; k < kDefaultPalette.size(); ++k) s.palette[k] = kDefaultPalette[k];
    }
    used_ = true;
}

void Gfx::closeScreen(int n) {
    if (n < 0 || n >= static_cast<int>(screens_.size())) return;
    for (Window& w : windows_)
        if (w.screen == n) w.open = false;
    screens_[static_cast<std::size_t>(n)].open = false;
}

void Gfx::openWindow(int n, const std::vector<std::int32_t>& a) {
    if (n < 0 || n >= static_cast<int>(windows_.size())) return;
    Window& w = windows_[static_cast<std::size_t>(n)];
    w = Window{};
    w.open = true;
    w.screen = a.size() >= 7 && a[6] >= 0 ? a[6] : 0;
    const Screen& s = screens_[static_cast<std::size_t>(w.screen)];
    if (a.size() >= 4) {
        w.x = a[0];
        w.y = a[1];
        w.width = a[2];
        w.height = a[3];
    } else if (n == 0) {
        w.x = 0, w.y = 11, w.width = s.width, w.height = s.height - 11;
    } else {  // 1..4 quarters, 5..15 centred (reference chapter 9)
        const int hw = s.width / 2, hh = (s.height - 11) / 2;
        w.width = hw, w.height = hh;
        w.x = (n == 2 || n == 4) ? hw : n >= 5 ? hw / 2 : 0;
        w.y = 11 + ((n == 3 || n == 4) ? hh : n >= 5 ? hh / 2 : 0);
    }
    w.pix.assign(static_cast<std::size_t>(std::max(0, w.innerW()) * std::max(0, w.innerH())), 0);
    current_ = n;
    used_ = true;
}

void Gfx::closeWindow(int n) {
    if (n >= 0 && n < static_cast<int>(windows_.size())) windows_[static_cast<std::size_t>(n)].open = false;
}

void Gfx::clearWindow(int n) {
    if (n < 0 || n >= static_cast<int>(windows_.size())) return;
    Window& w = windows_[static_cast<std::size_t>(n)];
    std::fill(w.pix.begin(), w.pix.end(), 0);
    w.texts.clear();
    current_ = n;
}

void Gfx::cls() { clearWindow(current_); }
void Gfx::titleWindow(int n, const std::string& t) {
    if (n >= 0 && n < static_cast<int>(windows_.size())) windows_[static_cast<std::size_t>(n)].title = t;
}
void Gfx::titleScreen(int n, const std::string& t) {
    if (n >= 0 && n < static_cast<int>(screens_.size())) screens_[static_cast<std::size_t>(n)].title = t;
}

void Gfx::color(const std::vector<std::int32_t>& v, unsigned present) {
    if ((present & 1) != 0 && !v.empty()) apen_ = v[0] & 63;
    if ((present & 2) != 0 && v.size() > 1) bpen_ = v[1] & 63;
    if ((present & 4) != 0 && v.size() > 2) open_ = v[2] & 63;
}

void Gfx::deffill(const std::vector<std::int32_t>& v, unsigned present) {
    if ((present & 1) != 0 && !v.empty()) apen_ = v[0] & 63;  // the same pen as COLOR (probe23 T3)
    if ((present & 2) != 0 && v.size() > 1) fillStyle_ = v[1];
}

void Gfx::setcolor(const std::vector<std::int32_t>& v) {
    if (v.empty()) return;
    Screen& s = screens_[static_cast<std::size_t>(win().screen)];
    const std::size_t reg = static_cast<std::size_t>(v[0] & 63);
    if (v.size() >= 4) s.palette[reg] = static_cast<std::uint16_t>(((v[1] & 15) << 8) | ((v[2] & 15) << 4) | (v[3] & 15));
    else if (v.size() == 2) s.palette[reg] = static_cast<std::uint16_t>(v[1] & 0xFFF);
}

void Gfx::graphmode(std::int32_t m) {
    mode_ = m;
    firstDot_ = true;  // SetDrMd: the next line draws its first point (probe23 T4)
}

void Gfx::defline(std::int32_t d) {
    // 0..6 predefined (reference); 1 solid is checked, the others are assumptions (D25)
    static const std::uint16_t kPredef[7] = {0x0000, 0xFFFF, 0xCCCC, 0xAAAA, 0xFF18, 0xFF00, 0xFE10};
    pattern_ = d >= 0 && d <= 6 ? kPredef[d] : static_cast<std::uint16_t>(d);
}

void Gfx::pixel(std::int32_t x, std::int32_t y, int color) {
    Window& w = win();
    if (x < 0 || y < 0 || x >= w.innerW() || y >= w.innerH()) return;
    std::uint8_t& p = w.pix[static_cast<std::size_t>(y) * static_cast<std::size_t>(w.innerW()) + static_cast<std::size_t>(x)];
    if ((mode_ & 2) != 0) p = static_cast<std::uint8_t>(p ^ depthMask());
    else p = static_cast<std::uint8_t>(color & depthMask());
}

void Gfx::patternPixel(std::int32_t x, std::int32_t y, bool bit) {
    if (bit) pixel(x, y, apen_);
    else if ((mode_ & 3) == 1) pixel(x, y, bpen_);  // JAM2: the 0 bits in the background pen (probe22 T6)
}

void Gfx::plot(std::int32_t x, std::int32_t y) {
    used_ = true;
    pixel(x, y, apen_);
    penX_ = x, penY_ = y;
}

// A line as on the original (probe22 T2, probe23 T2): along the major axis, the minor offset is
// i * d / n rounded to nearest, a tie towards the end point. The pattern starts with bit 15. In
// COMPLEMENT mode the first point is left out unless the line follows GRAPHMODE (FRST_DOT).
std::vector<std::pair<std::int32_t, std::int32_t>> Gfx::linePoints(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2) {
    std::vector<std::pair<std::int32_t, std::int32_t>> r;
    const std::int32_t dx = x2 - x1, dy = y2 - y1;
    const std::int32_t adx = std::abs(dx), ady = std::abs(dy);
    const std::int32_t n = std::max(adx, ady);
    for (std::int32_t i = 0; i <= n; ++i) {
        std::int32_t x = x1, y = y1;
        if (n > 0) {
            const std::int32_t minor = adx >= ady ? ady : adx;
            const std::int32_t off = static_cast<std::int32_t>((2LL * i * minor + n) / (2LL * n));
            if (adx >= ady) {
                x = x1 + (dx < 0 ? -i : i);
                y = y1 + (dy < 0 ? -off : off);
            } else {
                y = y1 + (dy < 0 ? -i : i);
                x = x1 + (dx < 0 ? -off : off);
            }
        }
        r.emplace_back(x, y);
    }
    return r;
}

void Gfx::segment(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2) {
    const bool skipFirst = (mode_ & 2) != 0 && !firstDot_;
    const auto pts = linePoints(x1, y1, x2, y2);
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (i == 0 && skipFirst) continue;
        patternPixel(pts[i].first, pts[i].second, ((pattern_ >> (15 - (i & 15))) & 1) != 0);
    }
    firstDot_ = false;
    penX_ = x2, penY_ = y2;
}

void Gfx::line(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2) {
    used_ = true;
    segment(x1, y1, x2, y2);
}

void Gfx::drawTo(std::int32_t x, std::int32_t y) {
    used_ = true;
    segment(penX_, penY_, x, y);
}

void Gfx::span(std::int32_t x1, std::int32_t x2, std::int32_t y, int color) {
    if (x1 > x2) std::swap(x1, x2);
    for (std::int32_t x = x1; x <= x2; ++x) pixel(x, y, color);
}

void Gfx::box(std::int32_t x1, std::int32_t y1, std::int32_t x2, std::int32_t y2, bool filled) {
    used_ = true;
    if (x1 > x2) std::swap(x1, x2);  // the corners in any order (probe24 T4)
    if (y1 > y2) std::swap(y1, y2);
    if (filled) {
        for (std::int32_t y = y1; y <= y2; ++y) span(x1, x2, y, apen_);
        if (boundary_) {  // the outline in the outline pen (probe24 T4)
            span(x1, x2, y1, open_);
            span(x1, x2, y2, open_);
            for (std::int32_t y = y1; y <= y2; ++y) pixel(x1, y, open_), pixel(x2, y, open_);
        }
        return;
    }
    segment(x1, y1, x2, y1);
    segment(x2, y1, x2, y2);
    segment(x2, y2, x1, y2);
    segment(x1, y2, x1, y1);
}

// The points of one quadrant of an ellipse by the midpoint method (probe22 T5, probe24 T3 match it
// exactly); the other quadrants by symmetry.
std::vector<std::pair<std::int32_t, std::int32_t>> Gfx::ellipsePoints(std::int32_t rx, std::int32_t ry) {
    std::vector<std::pair<std::int32_t, std::int32_t>> q;
    rx = std::abs(rx), ry = std::abs(ry);
    if (ry == 0) {
        for (std::int32_t x = 0; x <= rx; ++x) q.emplace_back(x, 0);
        return q;
    }
    if (rx == 0) {
        for (std::int32_t y = 0; y <= ry; ++y) q.emplace_back(0, y);
        return q;
    }
    // in quarters to stay integer: 4*d1 = 4b2 - 4a2*ry + a2
    const long long a2 = 1LL * rx * rx, b2 = 1LL * ry * ry;
    long long x = 0, y = ry;
    long long d1 = 4 * b2 - 4 * a2 * ry + a2;
    long long dx = 0, dy = 2 * a2 * y;
    while (dx < dy) {
        q.emplace_back(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
        if (d1 < 0) {
            ++x;
            dx += 2 * b2;
            d1 += 4 * (dx + b2);
        } else {
            ++x, --y;
            dx += 2 * b2;
            dy -= 2 * a2;
            d1 += 4 * (dx - dy + b2);
        }
    }
    // 4*d2 = b2*(2x+1)^2 + 4a2*(y-1)^2 - 4a2*b2
    long long d2 = b2 * (2 * x + 1) * (2 * x + 1) + 4 * a2 * (y - 1) * (y - 1) - 4 * a2 * b2;
    while (y >= 0) {
        q.emplace_back(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
        if (d2 > 0) {
            --y;
            dy -= 2 * a2;
            d2 += 4 * (a2 - dy);
        } else {
            --y, ++x;
            dx += 2 * b2;
            dy -= 2 * a2;
            d2 += 4 * (dx - dy + a2);
        }
    }
    return q;
}

// spans between the outermost outline points of every row (probe24 T2, T3; probe25 T8), with
// BOUNDARY the outline in the outline pen
void Gfx::fillOutline(const std::vector<std::pair<std::int32_t, std::int32_t>>& outline) {
    std::map<std::int32_t, std::pair<std::int32_t, std::int32_t>> rows;
    for (const auto& [x, y] : outline) {
        auto it = rows.find(y);
        if (it == rows.end()) rows[y] = {x, x};
        else it->second = {std::min(it->second.first, x), std::max(it->second.second, x)};
    }
    for (const auto& [y, r] : rows) span(r.first, r.second, y, apen_);
    if (!boundary_) return;
    for (const auto& [x, y] : outline) pixel(x, y, open_);
}

void Gfx::ellipse(std::int32_t cx, std::int32_t cy, std::int32_t rx, std::int32_t ry, bool filled) {
    used_ = true;
    const auto q = ellipsePoints(rx, ry);
    std::set<std::pair<std::int32_t, std::int32_t>> outline;
    for (const auto& [x, y] : q)
        for (int sx : {1, -1})
            for (int sy : {1, -1}) outline.insert({cx + sx * x, cy + sy * y});
    if (filled) {
        fillOutline({outline.begin(), outline.end()});
        return;
    }
    for (const auto& [x, y] : outline) pixel(x, y, apen_);
}

void Gfx::polyline(const std::vector<std::int32_t>& xy) {
    used_ = true;
    const std::size_t n = xy.size() / 2;
    if (n == 0) return;
    if (n == 1) {
        plot(xy[0], xy[1]);
        return;
    }
    for (std::size_t k = 0; k + 1 < n; ++k) segment(xy[2 * k], xy[2 * k + 1], xy[2 * k + 2], xy[2 * k + 3]);
}

void Gfx::polyfill(const std::vector<std::int32_t>& xy) {
    used_ = true;
    const std::size_t n = xy.size() / 2;
    if (n == 0) return;
    std::vector<std::pair<std::int32_t, std::int32_t>> outline;
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t j = (k + 1) % n;  // the last point back to the first
        const auto pts = linePoints(xy[2 * k], xy[2 * k + 1], xy[2 * j], xy[2 * j + 1]);
        outline.insert(outline.end(), pts.begin(), pts.end());
    }
    fillOutline(outline);
}

// CIRCLE: an ellipse with ry = 4r/9 (HiRes, probe23 T1: the height is exact for r = 1..40; some
// points near the change of direction differ from the original, D25)
void Gfx::circle(std::int32_t x, std::int32_t y, std::int32_t r, bool filled) { ellipse(x, y, r, 4 * r / 9, filled); }

void Gfx::text(std::int32_t x, std::int32_t y, const std::string& s) {
    used_ = true;
    win().texts.push_back(Text{x, y, apen_, s});
}

std::int32_t Gfx::point(std::int32_t x, std::int32_t y) const {
    const Window& w = win();
    if (x < 0 || y < 0 || x >= w.innerW() || y >= w.innerH()) return -1;
    return w.pix[static_cast<std::size_t>(y) * static_cast<std::size_t>(w.innerW()) + static_cast<std::size_t>(x)];
}

bool Gfx::isCommand(const std::string& n) { return n != "CLS" && gfxCommandCode(n) >= 0; }

void Gfx::command(const std::string& n, const std::vector<std::int32_t>& given, const std::string& str, int mode) {
    const auto present = static_cast<unsigned>(mode);
    std::vector<std::int32_t> v;
    v.swap(pending_);
    v.insert(v.end(), given.begin(), given.end());
    if (n == "OPENS") openScreen(v[0], std::vector<std::int32_t>(v.begin() + 1, v.end()));
    else if (n == "CLOSES") closeScreen(v[0]);
    else if (n == "OPENW") openWindow(v[0], std::vector<std::int32_t>(v.begin() + 1, v.end()));
    else if (n == "CLOSEW") closeWindow(v[0]);
    else if (n == "CLEARW") clearWindow(v[0]);
    else if (n == "TITLEW") titleWindow(v[0], str);
    else if (n == "TITLES") titleScreen(v[0], str);
    else if (n == "COLOR") color(v, present);
    else if (n == "DEFFILL") deffill(v, present);
    else if (n == "SETCOLOR") setcolor(v);
    else if (n == "GRAPHMODE") graphmode(v[0]);
    else if (n == "BOUNDARY") boundary(v[0]);
    else if (n == "DEFLINE") defline(v[0]);
    else if (n == "PLOT") plot(v[0], v[1]);
    else if (n == "LINE") line(v[0], v[1], v[2], v[3]);
    else if (n == "BOX" || n == "PBOX") box(v[0], v[1], v[2], v[3], n == "PBOX");
    else if (n == "CIRCLE" || n == "PCIRCLE") circle(v[0], v[1], v[2], n == "PCIRCLE");
    else if (n == "ELLIPSE" || n == "PELLIPSE") ellipse(v[0], v[1], v[2], v[3], n == "PELLIPSE");
    else if (n == "TEXT") text(v[0], v[1], str);
    else if (n == "CLS") cls();
    else if (n == "POLYLINE") polyline(v);
    else if (n == "POLYFILL") polyfill(v);
    else if (n == "DRAW") {  // DRAW x,y = PLOT; DRAW TO x,y from the last point; x1,y1 TO x2,y2 ... lines
        std::size_t k = 0;
        if (mode != 1) {
            plot(v[0], v[1]);
            k = 2;
        }
        for (; k + 1 < v.size(); k += 2) drawTo(v[k], v[k + 1]);
    }
    // FRONTS, BACKS, FRONTW, BACKW: only the order on the screen, nothing to draw
}

std::string Gfx::digest() const {
    std::string r;
    char buf[160];
    for (std::size_t n = 0; n < windows_.size(); ++n) {
        const Window& w = windows_[n];
        if (!w.open) continue;
        std::uint64_t h = 1469598103934665603ULL;  // FNV-1a over the pixels
        std::size_t set = 0;
        for (std::uint8_t p : w.pix) {
            h = (h ^ p) * 1099511628211ULL;
            set += p != 0;
        }
        std::snprintf(buf, sizeof buf, "window %zu screen %d %dx%d at %d,%d pixels %zu fnv %016llx\n", n, w.screen, w.innerW(),
                      w.innerH(), w.x, w.y, set, static_cast<unsigned long long>(h));
        r += buf;
        for (const Text& t : w.texts) {
            std::snprintf(buf, sizeof buf, "  text %d,%d colour %d: ", t.x, t.y, t.color);
            r += buf + t.s + "\n";
        }
    }
    for (std::size_t n = 0; n < screens_.size(); ++n) {
        const Screen& s = screens_[n];
        if (!s.open) continue;
        std::snprintf(buf, sizeof buf, "screen %zu %dx%d depth %d palette", n, s.width, s.height, s.depth);
        r += buf;
        for (int k = 0; k < (1 << s.depth); ++k) {
            std::snprintf(buf, sizeof buf, " %03X", s.palette[static_cast<std::size_t>(k)]);
            r += buf;
        }
        r += "\n";
    }
    return r;
}

bool Gfx::writePpm(const std::string& path) const {
    const Screen& s = screens_[0];
    std::vector<std::uint8_t> img(static_cast<std::size_t>(s.width * s.height), 0);
    for (const Window& w : windows_) {
        if (!w.open || w.screen != 0) continue;
        for (int y = 0; y < w.innerH(); ++y)
            for (int x = 0; x < w.innerW(); ++x) {
                const int sx = w.x + w.left + x, sy = w.y + w.top + y;
                if (sx >= 0 && sy >= 0 && sx < s.width && sy < s.height)
                    img[static_cast<std::size_t>(sy * s.width + sx)] = w.pix[static_cast<std::size_t>(y * w.innerW() + x)];
            }
        for (const Text& t : w.texts)  // no font of our own: a block per character (D25)
            for (std::size_t c = 0; c < t.s.size(); ++c)
                for (int dy = -6; dy <= 0; ++dy)
                    for (int dx = 1; dx < 7; ++dx) {
                        const int sx = w.x + w.left + t.x + static_cast<int>(8 * c) + dx, sy = w.y + w.top + t.y + dy;
                        if (sx >= 0 && sy >= 0 && sx < s.width && sy < s.height)
                            img[static_cast<std::size_t>(sy * s.width + sx)] = static_cast<std::uint8_t>(t.color);
                    }
    }
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", s.width, s.height);
    for (std::uint8_t p : img) {
        const std::uint16_t c = s.palette[p & 63];
        const unsigned char rgb[3] = {static_cast<unsigned char>(((c >> 8) & 15) * 17), static_cast<unsigned char>(((c >> 4) & 15) * 17),
                                      static_cast<unsigned char>((c & 15) * 17)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

}  // namespace brt
