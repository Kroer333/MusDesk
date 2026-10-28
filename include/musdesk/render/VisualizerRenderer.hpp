#pragma once

#include "musdesk/core/Composition.hpp"
#include "musdesk/media/ImageLoader.hpp"

#include <memory>

namespace musdesk {
// Owns GPU resources. Construct and destroy while its OpenGL context is current.
class VisualizerRenderer {
public:
    static constexpr int width = 1080;
    static constexpr int height = 1920;

    explicit VisualizerRenderer(const std::string& fontPath);
    ~VisualizerRenderer();
    VisualizerRenderer(const VisualizerRenderer&) = delete;
    VisualizerRenderer& operator=(const VisualizerRenderer&) = delete;

    void setArtwork(const ImageData& image);
    void clearArtwork();
    void renderFrame(double timeSeconds, const CompositionSettings& settings, float audioEnergy = 0);
    void renderFrame(double timeSeconds, const CompositionSettings& settings, const AudioReaction& audio);
    std::uint32_t textureId() const;
    int outputWidth() const;
    int outputHeight() const;
    // Bottom-up RGBA readback, used for frame reproducibility checks.
    std::vector<std::uint8_t> readPixels() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
