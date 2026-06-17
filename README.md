# RegionHybrid

**Lightweight, fast, region-aware image processor** for stylized rendering, machine vision, and video object tracing.

A general-purpose C++ library that performs stable region segmentation + perceptual tone mapping using deadzone quantization, HSL analysis, and a smart Neutral Influence Map.

## Features

- **Fast region detection**: RGB deadzone quantization + connected components (very stable on noisy images)
- **Per-region analysis**: Automatic 1–3 representative colors + chroma-plane PCA for natural tone gradients
- **Neutral Influence Map (NIM)**: Gracefully falls back to luminance in flat, bright, or dark areas
- **Rich output**: Tone maps + full per-region metadata (colors, PCA direction, etc.)
- **Video-friendly**: Designed for frame-by-frame processing with optional temporal propagation
- **Header-only friendly core**, minimal dependencies
- **Apache 2.0** licensed — free for commercial and open-source use

## Use Cases

- **Image quantization & compression**
- **Machine vision** — feature extraction, object segmentation
- **Video object tracing / tracking** — with temporal coherence
- **Artistic image tracing / vectorization prep**
- **Real-time effects** in games or creative tools
- **Pre-processing** for neural networks or traditional CV pipelines

## Performance

From our tests:
- **~40–60 FPS** at 1080p on a single modern CPU core (no SIMD/OpenMP yet)
- Excellent for real-time video applications with light tuning
- Scales well to 4K with optimizations

**Strengths**:
- Extremely practical middle-ground between simple thresholding and heavy ML segmentation.
- Very low computational cost → perfect for embedded, mobile, or real-time apps.
- The PCA-based tone coordinate is surprisingly effective at preserving artistic intent across regions.
- NIM makes output clean and artifact-free in a way many toon shaders struggle with.

**Limitations**:
 Simplified for general purposes 
- Current segmentation is purely color-based; it won't magically separate touching objects.
- No built-in edge-aware or semantic understanding yet.
- Temporal stability in video is basic — will need Kalman/filtering for production tracking.

**Big Implications**:
- This could become a strong building block for **real-time artistic video filters** (think Instagram/TikTok style effects but higher quality and controllable).
- Excellent foundation for **hybrid traditional + ML pipelines** — use RegionHybrid for coarse regions, then run lightweight models only inside regions.
- In open-source creative tools (Krita, Blender, Godot), this style of lightweight region processor is currently underserved.
- Potential research angle: combining with modern differentiable rendering or neural fields.


## Roadmap

- [ ] CMake + example CLI
- [ ] Python bindings (pybind11)
- [ ] OpenCV / stb_image helpers
- [ ] Better temporal coherence for video
- [ ] Polygon / SVG export per region
- [ ] SIMD & parallel optimizations

## Contributing

Pull requests welcome! Especially:
- Performance improvements
- Additional segmentation backends
- More examples/applications

## License

Apache License 2.0 — see LICENSE file.

---
