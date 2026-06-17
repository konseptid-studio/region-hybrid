// Region Hybrid - algorithm
// Copyright (c) 2025-2026 Ivan Kolev / KonseptID Studio
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "region_hybrid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <queue>

namespace region_hybrid {

static inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline int idx(int x, int y, int w) { return y * w + x; }

static inline float hue_wrap(float h) {
  while (h < 0.0f) h += 360.0f;
  while (h >= 360.0f) h -= 360.0f;
  return h;
}

static inline float hue_dist(float a, float b) {
  float d = std::fabs(hue_wrap(a) - hue_wrap(b));
  return std::min(d, 360.0f - d);
}

static inline float hue_delta(float a, float b) {
  float d = hue_wrap(b) - hue_wrap(a);
  if (d > 180.0f) d -= 360.0f;
  if (d < -180.0f) d += 360.0f;
  return d;
}

static inline float smoothstep(float e0, float e1, float x) {
  if (e0 == e1) return (x >= e1) ? 1.0f : 0.0f;
  float t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}

// ==================== Color Conversions ====================

HSL RGBToHSL(const RGBf& c) {
  float r = c.r, g = c.g, b = c.b;
  float mx = std::max({r, g, b});
  float mn = std::min({r, g, b});
  float d = mx - mn;

  HSL o{};
  o.l = 0.5f * (mx + mn);

  if (d < 1e-8f) {
    o.h = 0.0f;
    o.s = 0.0f;
    return o;
  }

  o.s = d / (1.0f - std::fabs(2.0f * o.l - 1.0f));

  float h;
  if (mx == r)      h = 60.0f * std::fmod(((g - b) / d), 6.0f);
  else if (mx == g) h = 60.0f * (((b - r) / d) + 2.0f);
  else              h = 60.0f * (((r - g) / d) + 4.0f);

  o.h = hue_wrap(h);
  return o;
}

float Luma01(const RGBf& c) {
  return clamp01(0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b);
}

// ==================== Region Detection ====================

static inline uint8_t qchan(float v01, int levels) {
  v01 = clamp01(v01);
  int q = (int)std::lround(v01 * (levels - 1));
  return (uint8_t)std::max(0, std::min(levels - 1, q));
}

static uint32_t PackKey(uint8_t r, uint8_t g, uint8_t b) {
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static std::vector<uint32_t> DeadzoneKeys(const std::vector<RGBf>& img, int w, int h, const Params& p) {
  std::vector<uint32_t> keys(w * h);
  for (int i = 0; i < w * h; ++i) {
    float r = img[i].r, g = img[i].g, b = img[i].b;
    if (p.use_lab_like_weight) {
      r = clamp01(r * 0.90f + g * 0.10f);
      b = clamp01(b * 0.90f + g * 0.10f);
    }
    uint8_t qr = qchan(r, p.deadzone_levels);
    uint8_t qg = qchan(g, p.deadzone_levels);
    uint8_t qb = qchan(b, p.deadzone_levels);
    keys[i] = PackKey(qr, qg, qb);
  }
  return keys;
}

static void ConnectedComponents(
    const std::vector<uint32_t>& key, int w, int h,
    std::vector<int>& region_id, std::vector<int>& region_sizes) {

  region_id.assign(w * h, -1);
  region_sizes.clear();
  int next_id = 0;

  std::queue<int> q;
  auto push = [&](int i) { region_id[i] = next_id; q.push(i); };

  for (int i = 0; i < w * h; ++i) {
    if (region_id[i] != -1) continue;
    uint32_t k = key[i];
    int count = 0;
    push(i);

    while (!q.empty()) {
      int v = q.front(); q.pop();
      ++count;
      int x = v % w, y = v / w;

      auto try_n = [&](int nx, int ny) {
        if ((unsigned)nx >= (unsigned)w || (unsigned)ny >= (unsigned)h) return;
        int ni = idx(nx, ny, w);
        if (region_id[ni] != -1) return;
        if (key[ni] != k) return;
        push(ni);
      };

      try_n(x - 1, y); try_n(x + 1, y);
      try_n(x, y - 1); try_n(x, y + 1);
    }

    region_sizes.push_back(count);
    ++next_id;
  }
}

// ==================== Per-Region Color Picking ====================

static std::vector<HSL> PickRegionColors(
    const std::vector<HSL>& samples, int wantK, const Params& p) {

  std::vector<HSL> pool;
  pool.reserve(samples.size());
  for (auto& s : samples) {
    if (s.s >= p.s_min) pool.push_back(s);
  }

  if (pool.empty()) {
    float L = 0.0f;
    for (auto& s : samples) L += s.l;
    L = (samples.empty() ? 0.5f : L / (float)samples.size());
    return { HSL{0.0f, 0.0f, clamp01(L)} };
  }

  // Seed from most populated coarse hue bin
  const int bins = 36;
  std::array<int, bins> hist{};
  for (auto& s : pool) {
    int b = (int)std::floor((s.h / 360.0f) * bins);
    b = std::max(0, std::min(bins - 1, b));
    hist[b]++;
  }
  int bestBin = (int)(std::max_element(hist.begin(), hist.end()) - hist.begin());
  float binH0 = (bestBin + 0.5f) * (360.0f / bins);

  auto score_seed = [&](const HSL& s) {
    float dH = hue_dist(s.h, binH0);
    return dH + (1.0f - s.s) * 30.0f;
  };

  HSL seed = pool[0];
  float bestSeed = score_seed(seed);
  for (auto& s : pool) {
    float sc = score_seed(s);
    if (sc < bestSeed) {
      bestSeed = sc;
      seed = s;
    }
  }

  std::vector<HSL> out{seed};

  auto dist_hsl = [&](const HSL& a, const HSL& b) {
    float dH = hue_dist(a.h, b.h) / 180.0f;
    float dS = std::fabs(a.s - b.s);
    float dL = std::fabs(a.l - b.l);
    return dH + dS + dL;
  };

  while ((int)out.size() < wantK) {
    HSL best = out.back();
    float bestMinD = -1.0f;

    for (auto& c : pool) {
      bool ok = true;
      for (auto& s : out) {
        float dh = hue_dist(c.h, s.h);
        float ds = std::fabs(c.s - s.s);
        float dl = std::fabs(c.l - s.l);
        if (dh < p.hue_sep_deg && ds < p.s_sep && dl < p.l_sep) {
          ok = false;
          break;
        }
      }
      if (!ok) continue;

      float minD = std::numeric_limits<float>::infinity();
      for (auto& s : out) minD = std::min(minD, dist_hsl(c, s));
      if (minD > bestMinD) {
        bestMinD = minD;
        best = c;
      }
    }

    if (bestMinD < 0.0f) break;
    out.push_back(best);
  }

  return out;
}

// ==================== Chroma PCA ====================

static void RegionPCA_Direction(
    const std::vector<HSL>& samples, const Params& p,
    float& out_vx, float& out_vy, float& out_mx, float& out_my) {

  std::vector<std::array<float, 2>> pts;
  pts.reserve(samples.size());
  for (auto& s : samples) {
    if (s.s < p.s_min) continue;
    float rad = s.h * 3.1415926535f / 180.0f;
    pts.push_back({s.s * std::cos(rad), s.s * std::sin(rad)});
  }

  if (pts.size() < 8) {
    out_vx = 1.0f; out_vy = 0.0f;
    out_mx = 0.0f; out_my = 0.0f;
    return;
  }

  float mx = 0, my = 0;
  for (auto& q : pts) { mx += q[0]; my += q[1]; }
  mx /= (float)pts.size(); my /= (float)pts.size();
  out_mx = mx; out_my = my;

  float c00 = 0, c01 = 0, c11 = 0;
  for (auto& q : pts) {
    float x = q[0] - mx, y = q[1] - my;
    c00 += x * x; c01 += x * y; c11 += y * y;
  }
  c00 /= (float)pts.size(); c01 /= (float)pts.size(); c11 /= (float)pts.size();

  // Power iteration for principal eigenvector
  float vx = 1.0f, vy = 0.0f;
  for (int it = 0; it < 8; ++it) {
    float nx = c00 * vx + c01 * vy;
    float ny = c01 * vx + c11 * vy;
    float n = std::sqrt(nx * nx + ny * ny) + 1e-12f;
    vx = nx / n; vy = ny / n;
  }
  out_vx = vx; out_vy = vy;
}

static float ProjectU(const HSL& s, float vx, float vy, float mx, float my) {
  if (s.s <= 1e-8f) return 0.5f;
  float rad = s.h * 3.1415926535f / 180.0f;
  float x = s.s * std::cos(rad) - mx;
  float y = s.s * std::sin(rad) - my;
  return x * vx + y * vy;
}

// ==================== Neutral Influence Map (NIM) ====================

static std::vector<float> BoxBlur(const std::vector<float>& in, int w, int h, int rad) {
  if (rad <= 0) return in;
  std::vector<float> tmp(w * h), out(w * h);

  // Horizontal pass
  for (int y = 0; y < h; ++y) {
    float sum = 0.0f;
    int count = 0;
    int yoff = y * w;
    for (int x = -rad; x <= rad; ++x) {
      int xx = std::max(0, std::min(w - 1, x));
      sum += in[yoff + xx]; ++count;
    }
    tmp[yoff + 0] = sum / count;
    for (int x = 1; x < w; ++x) {
      int add = std::min(w - 1, x + rad);
      int sub = std::max(0, x - rad - 1);
      sum += in[yoff + add] - in[yoff + sub];
      tmp[yoff + x] = sum / count;
    }
  }

  // Vertical pass
  for (int x = 0; x < w; ++x) {
    float sum = 0.0f;
    int count = 0;
    for (int y = -rad; y <= rad; ++y) {
      int yy = std::max(0, std::min(h - 1, y));
      sum += tmp[yy * w + x]; ++count;
    }
    out[0 * w + x] = sum / count;
    for (int y = 1; y < h; ++y) {
      int add = std::min(h - 1, y + rad);
      int sub = std::max(0, y - rad - 1);
      sum += tmp[add * w + x] - tmp[sub * w + x];
      out[y * w + x] = sum / count;
    }
  }
  return out;
}

static void BuildNIM(const std::vector<RGBf>& img, int w, int h, const Params& p,
                     std::vector<float>& Yb, std::vector<float>& neutral) {
  std::vector<float> Y(w * h);
  for (int i = 0; i < w * h; ++i) Y[i] = Luma01(img[i]);

  Yb = BoxBlur(Y, w, h, p.blur_radius_Y);

  // Simple gradient magnitude
  std::vector<float> G(w * h, 0.0f);
  for (int y = 0; y < h; ++y) {
    int y0 = std::max(0, y - 1), y1 = std::min(h - 1, y + 1);
    for (int x = 0; x < w; ++x) {
      int x0 = std::max(0, x - 1), x1 = std::min(w - 1, x + 1);
      float dYdx = Yb[idx(x1, y, w)] - Yb[idx(x0, y, w)];
      float dYdy = Yb[idx(x, y1, w)] - Yb[idx(x, y0, w)];
      G[idx(x, y, w)] = std::fabs(dYdx) + std::fabs(dYdy);
    }
  }

  neutral.assign(w * h, 0.0f);
  for (int i = 0; i < w * h; ++i) {
    float Hm = smoothstep(p.Y_hi0, p.Y_hi1, Yb[i]);
    float Sm = 1.0f - smoothstep(p.Y_lo0, p.Y_lo1, Yb[i]);
    float edge_sup = 1.0f - smoothstep(p.G0, p.G1, G[i]);
    neutral[i] = clamp01((Hm + Sm) * edge_sup);
  }
  neutral = BoxBlur(neutral, w, h, p.blur_radius_neutral);
}

// ==================== Tone Mapping ====================

static int ToneFromU(float u01, int T) {
  int t = (int)std::lround(clamp01(u01) * (T - 1));
  return std::max(0, std::min(T - 1, t));
}

static uint8_t RegionToneColorOnly(
    const HSL& px, float u, float u_min, float u_max,
    const std::vector<HSL>& colors, int T) {

  float un = (u_max <= u_min + 1e-12f) ? 0.5f : (u - u_min) / (u_max - u_min);
  un = clamp01(un);

  if (colors.size() <= 1) {
    return (uint8_t)ToneFromU(1.0f - px.l, T);  // Use lightness for detail
  }
  if (colors.size() == 2) {
    return (uint8_t)ToneFromU(un, T);
  }

  // 3 colors: simple piecewise
  float mid = 0.5f;
  if (un <= mid) {
    return (uint8_t)ToneFromU(0.5f * (un / mid), T);
  } else {
    return (uint8_t)ToneFromU(0.5f + 0.5f * ((un - mid) / (1.0f - mid)), T);
  }
}

// ==================== Main Entry Point ====================

Result Run(const std::vector<RGBf>& img, int w, int h, const Params& p) {
  Result out;
  out.tone_final.assign(w * h, 0);
  out.tone_color_only.assign(w * h, 0);
  out.region_id.assign(w * h, -1);
  out.neutral_map.assign(w * h, 0.0f);

  // 1. Region detection
  auto keys = DeadzoneKeys(img, w, h, p);
  std::vector<int> region_sizes;
  ConnectedComponents(keys, w, h, out.region_id, region_sizes);
  const int R = (int)region_sizes.size();

  // 2. NIM (Neutral Influence Map)
  std::vector<float> Yb;
  if (p.use_nim) {
    BuildNIM(img, w, h, p, Yb, out.neutral_map);
  } else {
    Yb.assign(w * h, 0.0f);
    for (int i = 0; i < w * h; ++i) Yb[i] = Luma01(img[i]);
  }

  // 3. Build per-region pixel lists
  std::vector<std::vector<int>> region_pixels(R);
  for (int i = 0; i < w * h; ++i) {
    int rid = out.region_id[i];
    if (rid >= 0 && rid < R) region_pixels[rid].push_back(i);
  }

  // 4. Per-region analysis and tone assignment
  out.regions.resize(R);
  for (int rid = 0; rid < R; ++rid) {
    const auto& pixels = region_pixels[rid];
    if (pixels.empty() || (int)pixels.size() < p.min_region_pixels) {
      // Tiny region fallback
      for (int pi : pixels) {
        float y = Yb[pi];
        int tn = p.dark_on_top ? (int)std::lround((1.0f - y) * (p.tones - 1))
                               : (int)std::lround(y * (p.tones - 1));
        out.tone_color_only[pi] = (uint8_t)std::max(0, std::min(p.tones - 1, tn));
      }
      continue;
    }

    std::vector<HSL> samples;
    samples.reserve(pixels.size());
    float sum_l = 0.0f, sum_s = 0.0f;
    for (int pi : pixels) {
      HSL hsl = RGBToHSL(img[pi]);
      samples.push_back(hsl);
      sum_l += hsl.l;
      sum_s += hsl.s;
    }

    // Fill region stats
    auto& stats = out.regions[rid];
    stats.id = rid;
    stats.pixel_count = pixels.size();
    stats.mean_luma = sum_l / (float)pixels.size();
    stats.mean_saturation = sum_s / (float)pixels.size();

    int wantK = std::max(1, std::min(p.max_region_colors, 3));
    stats.representative_colors = PickRegionColors(samples, wantK, p);

    // PCA
    float vx, vy, mx, my;
    RegionPCA_Direction(samples, p, vx, vy, mx, my);
    stats.pca_direction = {vx, vy};
    stats.pca_mean = {mx, my};

    // u range
    float umin = +std::numeric_limits<float>::infinity();
    float umax = -std::numeric_limits<float>::infinity();
    for (auto& s : samples) {
      float u = ProjectU(s, vx, vy, mx, my);
      umin = std::min(umin, u);
      umax = std::max(umax, u);
    }
    if (!std::isfinite(umin) || !std::isfinite(umax)) {
      umin = 0.0f; umax = 1.0f;
    }
    stats.u_min = umin;
    stats.u_max = umax;

    // Assign tones
    for (int pi : pixels) {
      HSL px = RGBToHSL(img[pi]);
      float u = ProjectU(px, vx, vy, mx, my);
      out.tone_color_only[pi] = RegionToneColorOnly(px, u, umin, umax, stats.representative_colors, p.tones);
    }
  }

  // 5. Blend with NIM
  for (int i = 0; i < w * h; ++i) {
    int t_color = (int)out.tone_color_only[i];

    float y = Yb[i];
    int t_neutral = p.dark_on_top
        ? (int)std::lround((1.0f - y) * (p.tones - 1))
        : (int)std::lround(y * (p.tones - 1));
    t_neutral = std::max(0, std::min(p.tones - 1, t_neutral));

    float wN = p.use_nim ? clamp01(out.neutral_map[i]) : 0.0f;
    int tf = (int)std::lround(lerp((float)t_color, (float)t_neutral, wN));
    out.tone_final[i] = (uint8_t)std::max(0, std::min(p.tones - 1, tf));
  }

  return out;
}

// Stub for future video tracking
void PropagateRegionIDs(const Result& previous, Result& current) {
  // TODO: Implement overlap-based ID propagation + temporal smoothing
  (void)previous;
  (void)current;
}

} // namespace region_hybrid
