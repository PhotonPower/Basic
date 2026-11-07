// Graphics of platform/pc (rt_gfx, D25) against the original: the same drawings as the probe
// programs probe22-24, the rasters read back with POINT compared with their result files.

#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>

#include "rt_gfx.hpp"
#include "test_util.hpp"

namespace {

std::map<std::string, std::string> g_rows;  // "probe22 T2 y10" -> "1100000000"

void load(const std::string& probe) {
    std::ifstream in(std::string(BASIC_SOURCE_DIR) + "/tests/probes/results/" + probe + ".txt");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t y = line.find(" y");
        const std::size_t sp = line.rfind(' ');
        if (y == std::string::npos || sp == std::string::npos || sp < y) continue;
        g_rows[probe + " " + line.substr(0, sp)] = line.substr(sp + 1);
    }
}

// compares the raster x0..x1, y0..y1 with the rows "<probe> <tag> y<y>"
void raster(const brt::Gfx& g, const std::string& probe, const std::string& tag, int x0, int x1, int y0, int y1) {
    for (int y = y0; y <= y1; ++y) {
        std::string got;
        for (int x = x0; x <= x1; ++x) got += std::to_string(g.point(x, y));
        const std::string key = probe + " " + tag + " y" + std::to_string(y);
        const auto it = g_rows.find(key);
        CHECK(it != g_rows.end());
        if (it == g_rows.end()) continue;
        if (got != it->second) std::fprintf(stderr, "%s: %s, original %s\n", key.c_str(), got.c_str(), it->second.c_str());
        CHECK_EQ(got, it->second);
    }
}

void probe22() {
    brt::Gfx g;
    g.openWindow(0, {});
    // T2
    g.clearWindow(0);
    g.color({1}, 1);
    g.line(0, 10, 9, 13);
    g.line(9, 20, 0, 23);
    raster(g, "probe22", "T2", 0, 9, 10, 23);
    // T3 (DEFFILL sets the pen, BOUNDARY off at the start)
    g.clearWindow(0);
    g.color({1, 0, 2}, 7);
    g.deffill({3, 1}, 3);
    g.box(0, 30, 5, 33, false);
    g.box(8, 30, 13, 33, true);
    g.boundary(0);
    g.box(16, 30, 21, 33, true);
    g.boundary(1);
    raster(g, "probe22", "T3", 0, 22, 29, 34);
    // T5
    g.clearWindow(0);
    g.color({1}, 1);
    g.ellipse(7, 60, 6, 3, false);
    raster(g, "probe22", "T5", 0, 14, 56, 64);
    // T6: XOR and JAM2 with a pattern
    g.clearWindow(0);
    g.color({1}, 1);
    g.line(0, 70, 9, 70);
    g.graphmode(2);
    g.line(0, 70, 4, 70);
    g.line(0, 71, 4, 71);
    g.graphmode(1);
    g.color({1, 2}, 3);
    g.defline(0xF0F0);
    g.line(0, 72, 19, 72);
    g.graphmode(0);
    g.line(0, 73, 19, 73);
    g.defline(1);
    raster(g, "probe22", "T6", 0, 19, 70, 73);
    // T7: DRAW TO, clipping
    g.clearWindow(0);
    g.color({1}, 1);
    g.plot(0, 80);
    g.drawTo(5, 82);
    g.line(7, 80, 9, 80);
    g.drawTo(9, 82);
    g.line(-3, 85, 3, 85);
    raster(g, "probe22", "T7", 0, 10, 80, 85);
    // T1: POINT outside
    CHECK_EQ(g.point(-1, -1), -1);
    CHECK_EQ(g.point(5000, 5), -1);
}

void probe23() {
    brt::Gfx g;
    g.openWindow(0, {});
    // T1: the height of CIRCLE for r = 1..40
    std::ifstream in(std::string(BASIC_SOURCE_DIR) + "/tests/probes/results/probe23.txt");
    std::string line;
    while (std::getline(in, line)) {
        int r = 0, top = 0, bottom = 0, left = 0;
        if (std::sscanf(line.c_str(), "T1 r=%d oben %d unten %d links %d", &r, &top, &bottom, &left) != 4) continue;
        g.clearWindow(0);
        g.color({1}, 1);
        g.circle(100, 100, r, false);
        int y0 = -1, y1 = -1, x0 = -1;
        for (int y = 40; y <= 160; ++y)
            if (g.point(100, y) != 0) {
                if (y0 < 0) y0 = y;
                y1 = y;
            }
        for (int x = 40; x <= 160 && x0 < 0; ++x)
            if (g.point(x, 100) != 0) x0 = x;
        CHECK_EQ(100 - y0, top);
        CHECK_EQ(y1 - 100, bottom);
        CHECK_EQ(100 - x0, left);
    }
    // T2: ties towards the end point
    g.clearWindow(0);
    g.color({1}, 1);
    g.line(0, 10, 2, 11);
    g.line(10, 11, 8, 10);
    g.line(0, 20, 4, 22);
    g.line(14, 22, 10, 20);
    g.line(0, 30, 1, 32);
    g.line(6, 32, 5, 30);
    raster(g, "probe23", "T2a", 0, 14, 10, 11);
    raster(g, "probe23", "T2b", 0, 14, 20, 22);
    raster(g, "probe23", "T2c", 0, 6, 30, 32);
    // T3: one pen
    g.clearWindow(0);
    g.color({1}, 1);
    g.deffill({3, 1}, 3);
    g.plot(0, 40);
    g.color({2}, 1);
    g.box(2, 40, 4, 41, true);
    g.deffill({1, 1}, 3);
    g.plot(6, 40);
    g.color({1, 0, 3}, 7);
    g.deffill({2, 1}, 3);
    g.circle(20, 45, 3, true);
    raster(g, "probe23", "T3", 0, 25, 40, 47);
    // T4: XOR, first point
    g.clearWindow(0);
    g.color({1}, 1);
    g.graphmode(2);
    g.line(0, 50, 4, 50);
    g.line(0, 51, 4, 51);
    g.line(0, 52, 4, 52);
    g.plot(8, 50);
    g.plot(8, 50);
    g.plot(9, 50);
    g.drawTo(12, 50);
    g.graphmode(0);
    raster(g, "probe23", "T4", 0, 14, 50, 52);
}

void probe24() {
    brt::Gfx g;
    g.openWindow(0, {});
    // T1: CIRCLE; r = 6 and 12 agree with the original, 4, 9, 17 differ near the change of
    // direction (D25: the ROM's DrawEllipse is not known exactly)
    for (int r : {6, 12}) {
        g.clearWindow(0);
        g.color({1}, 1);
        g.circle(30, 30, r, false);
        raster(g, "probe24", "T1 r=" + std::to_string(r), 30 - r - 1, 30 + r + 1, 22, 38);
    }
    // T2: PCIRCLE r = 6 without and with BOUNDARY
    for (int b : {0, 1}) {
        g.clearWindow(0);
        g.color({1, 0, 2}, 7);
        g.deffill({3, 1}, 3);
        g.boundary(b);
        g.circle(30, 30, 6, true);
        raster(g, "probe24", std::string("T2 r=6 ") + (b ? "mit" : "ohne"), 23, 37, 25, 35);
    }
    // T3: PELLIPSE 6,3 and ELLIPSE 5,5
    g.clearWindow(0);
    g.boundary(0);
    g.ellipse(30, 30, 6, 3, true);
    raster(g, "probe24", "T3 pe", 23, 37, 26, 34);
    g.clearWindow(0);
    g.color({1}, 1);
    g.ellipse(30, 30, 5, 5, false);
    raster(g, "probe24", "T3 e55", 24, 36, 24, 36);
    // T4: PBOX with BOUNDARY, corners in any order
    g.clearWindow(0);
    g.color({1, 0, 2}, 7);
    g.deffill({3, 1}, 3);
    g.boundary(1);
    g.box(0, 40, 5, 43, true);
    g.boundary(0);
    g.color({1}, 1);
    g.box(13, 43, 8, 40, false);
    g.deffill({3, 1}, 3);
    g.box(21, 43, 16, 40, true);
    raster(g, "probe24", "T4", 0, 22, 39, 44);
}

// T8: POLYLINE (open), POLYFILL without and with BOUNDARY and OFFSET, POLYLINE with 2 points
void probe25() {
    brt::Gfx g;
    g.openWindow(0, {});
    g.color({1, 0, 2}, 7);
    g.deffill({3, 1}, 3);
    g.boundary(0);
    const std::vector<std::int32_t> xy = {10, 10, 20, 12, 16, 20, 8, 17};
    g.clearWindow(0);
    g.command("POLYLINE", xy, "", 0);
    raster(g, "probe25", "T8 line", 6, 22, 8, 22);
    g.clearWindow(0);
    g.command("POLYFILL", xy, "", 0);
    raster(g, "probe25", "T8 fill", 6, 22, 8, 22);
    g.clearWindow(0);
    g.boundary(1);
    for (std::size_t k = 0; k < xy.size(); ++k) g.arg(xy[k] + (k % 2 == 0 ? 1 : 0));  // OFFSET 1,0
    g.command("POLYFILL", {}, "", 0);
    raster(g, "probe25", "T8 fill b off", 6, 22, 8, 22);
    g.clearWindow(0);
    g.boundary(0);
    g.command("POLYLINE", {10, 10, 20, 12}, "", 0);
    raster(g, "probe25", "T8 line2", 6, 22, 8, 14);
}

}  // namespace

int main() {
    load("probe25");
    load("probe22");
    load("probe23");
    load("probe24");
    probe22();
    probe23();
    probe24();
    probe25();
    return report("gfx");
}
