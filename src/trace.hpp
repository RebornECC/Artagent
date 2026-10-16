#pragma once
// Чистый C++ без зависимостей от Geode: картинка -> набор цветных прямоугольников.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace trace {

struct Rect { int x, y, w, h, color; };      // в пикселях исходной картинки
struct Result {
    std::vector<Rect> rects;
    std::vector<std::array<uint8_t, 3>> palette;
    int cell = 0;
};

// rgba: w*h*4 байт. Пиксели с alpha < 128 считаются пустыми.
inline Result run(const uint8_t* rgba, int w, int h, int maxObjects, int colors) {
    Result best;
    int cell = std::max(1, std::max(w, h) / 160);
    for (int iter = 0; iter < 40; ++iter) {
        int gw = (w + cell - 1) / cell, gh = (h + cell - 1) / cell;
        std::vector<std::array<float, 3>> col(gw * gh);
        std::vector<char> on(gw * gh, 0);
        for (int gy = 0; gy < gh; ++gy) for (int gx = 0; gx < gw; ++gx) {
            float r = 0, g = 0, b = 0; int n = 0, tot = 0;
            for (int y = gy * cell; y < std::min(h, (gy + 1) * cell); ++y)
                for (int x = gx * cell; x < std::min(w, (gx + 1) * cell); ++x) {
                    const uint8_t* p = rgba + (y * w + x) * 4; ++tot;
                    if (p[3] < 128) continue;
                    r += p[0]; g += p[1]; b += p[2]; ++n;
                }
            if (n * 2 >= tot && n > 0) { on[gy * gw + gx] = 1; col[gy * gw + gx] = {r / n, g / n, b / n}; }
        }
        // k-means: начальные центры равномерно по яркости
        std::vector<int> idx;
        for (int i = 0; i < gw * gh; ++i) if (on[i]) idx.push_back(i);
        Result res; res.cell = cell;
        if (idx.empty()) return res;
        auto lum = [&](int i) { return col[i][0] * .3f + col[i][1] * .59f + col[i][2] * .11f; };
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return lum(a) < lum(b); });
        int k = std::max(1, std::min<int>(colors, idx.size()));
        std::vector<std::array<float, 3>> c(k);
        for (int i = 0; i < k; ++i) c[i] = col[idx[(size_t)(i + 0.5f) * idx.size() / k]];
        std::vector<int> lab(gw * gh, -1);
        for (int it = 0; it < 8; ++it) {
            std::vector<std::array<float, 4>> acc(k, {0, 0, 0, 0});
            for (int i : idx) {
                int bi = 0; float bd = 1e18f;
                for (int j = 0; j < k; ++j) {
                    float d = 0; for (int q = 0; q < 3; ++q) { float t = col[i][q] - c[j][q]; d += t * t; }
                    if (d < bd) { bd = d; bi = j; }
                }
                lab[i] = bi;
                for (int q = 0; q < 3; ++q) acc[bi][q] += col[i][q];
                acc[bi][3] += 1;
            }
            for (int j = 0; j < k; ++j) if (acc[j][3] > 0) for (int q = 0; q < 3; ++q) c[j][q] = acc[j][q] / acc[j][3];
        }
        for (auto& v : c) res.palette.push_back({(uint8_t)std::lround(v[0]), (uint8_t)std::lround(v[1]), (uint8_t)std::lround(v[2])});
        // жадное слияние клеток одного цвета в прямоугольники
        std::vector<char> used(gw * gh, 0);
        for (int gy = 0; gy < gh; ++gy) for (int gx = 0; gx < gw; ++gx) {
            int i = gy * gw + gx;
            if (!on[i] || used[i]) continue;
            int L = lab[i], rw = 1, rh = 1;
            while (gx + rw < gw && on[i + rw] && !used[i + rw] && lab[i + rw] == L) ++rw;
            for (;;) {
                if (gy + rh >= gh) break;
                bool ok = true;
                for (int x = 0; x < rw && ok; ++x) { int j = (gy + rh) * gw + gx + x; ok = on[j] && !used[j] && lab[j] == L; }
                if (!ok) break; ++rh;
            }
            for (int y = 0; y < rh; ++y) for (int x = 0; x < rw; ++x) used[(gy + y) * gw + gx + x] = 1;
            res.rects.push_back({gx * cell, gy * cell, std::min(rw * cell, w - gx * cell), std::min(rh * cell, h - gy * cell), L});
        }
        best = std::move(res);
        if ((int)best.rects.size() <= maxObjects) break;
        cell = std::max(cell + 1, (int)(cell * 1.2f)); // слишком много объектов: грубее сетка
    }
    return best;
}
} // namespace trace
