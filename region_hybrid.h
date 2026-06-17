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

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <array>
#include <cstddef>

namespace region_hybrid {

/// Floating point RGB color (values in [0,1])
struct RGBf {
    float r, g, b;
    RGBf(float rr = 0.0f, float gg = 0.0f, float bb = 0.0f) : r(rr), g(gg), b(bb) {}
};

/// HSL color representation
struct HSL {
    float h;  // Hue in degrees [0, 360)
    float s;  // Saturation [0, 1]
    float l;  // Lightness [0, 1]
    HSL(float hh = 0.0f, float ss = 0.0f, float ll = 0.0f) : h(hh), s(ss), l(ll) {}
};

/// Rich per-region metadata
struct RegionStats {
    int id = -1;
    size_t pixel_count = 0;
    std::vector<HSL> representative_colors;     // 1 to 3 dominant colors
    std::array<float, 2> pca_direction{1.0f, 0.0f}; // Principal direction in chroma plane
    std::array<float, 2> pca_mean{0.0f, 0.0f};
    float u_min = 0.0f;
    float u_max = 1.0f;

    float mean_luma = 0.5f;
    float mean_saturation = 0.0f;
    // Future: bounding box, contour points, etc.
};

/// Complete processing result
struct Result {
    std::vector<uint8_t> tone_final;        // Final tone indices after NIM blending (0..tones-1)
    std::vector<uint8_t> tone_color_only;   // Pure region-based tone indices
    std::vector<int>     region_id;         // Per-pixel region label
    std::vector<float>   neutral_map;       // Neutral Influence Map [0,1]

    std::vector<RegionStats> regions;       // Metadata for each region
};

/// Configuration parameters
struct Params {
    // Region detection
    int deadzone_levels = 12;             // RGB quantization levels per channel (8-16 good)
    bool use_lab_like_weight = true;      // Perceptual channel weighting
    int min_region_pixels = 8;            // Filter small noise regions

    // Color analysis & tone mapping
    int max_region_colors = 3;            // Max colors to pick per region (1-3)
    float s_min = 0.08f;                  // Minimum saturation for colored samples
    float hue_sep_deg = 25.0f;            // Minimum hue separation between colors
    float s_sep = 0.15f;                  // Saturation separation
    float l_sep = 0.15f;                  // Lightness separation

    int tones = 8;                        // Number of output tone levels
    bool dark_on_top = true;              // Whether darker tones get higher indices

    // Neutral Influence Map (NIM)
    bool use_nim = true;
    int blur_radius_Y = 2;
    int blur_radius_neutral = 3;

    float Y_hi0 = 0.85f, Y_hi1 = 0.95f;  // Bright neutral thresholds
    float Y_lo0 = 0.15f, Y_lo1 = 0.25f;  // Dark neutral thresholds
    float G0 = 0.05f, G1 = 0.20f;        // Gradient suppression thresholds

    // Extensibility hook
    std::string segmentation_mode = "deadzone"; // "deadzone", "future_slic", etc.
};

/// Main processing function
Result Run(const std::vector<RGBf>& image, int width, int height,
           const Params& params = Params());

/// Public utilities
HSL RGBToHSL(const RGBf& c);
float Luma01(const RGBf& c);

/// Optional: Temporal region ID propagation for video sequences (basic stub)
void PropagateRegionIDs(const Result& previous, Result& current);

} // namespace region_hybrid
