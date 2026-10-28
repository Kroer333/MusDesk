#pragma once

#include <string>
#include "musdesk/core/VisualPreset.hpp"
#include "musdesk/core/AudioAnalysis.hpp"

namespace musdesk {
enum class CompositionFormat { Portrait, Landscape, Square, PortraitThreeFour, LandscapeFourThree };
struct OutputSize {
    int width;
    int height;
    bool operator==(const OutputSize&) const = default;
};
struct CompositionLayout {
    OutputSize canvas;
    float discX, discY, discRadius;
    float textX, textWidth, titleY, artistY;
    float timelineLeft, timelineRight, timelineY;
};
const char* formatName(CompositionFormat format);
CompositionFormat parseCompositionFormat(const std::string& name);
OutputSize outputSize(CompositionFormat format, int shortEdge = 1080);
CompositionLayout layoutFor(CompositionFormat format);

struct CompositionSettings {
    std::string title = "Track title";
    std::string artist = "Artist name";
    double durationSeconds = 252.0;
    VisualPreset preset;
    CompositionFormat format = CompositionFormat::Portrait;
    int shortEdge = 1080;
    bool operator==(const CompositionSettings&) const = default;
};

struct CompositionFrame {
    double timeSeconds;
    double discAngleRadians;
    double backgroundAngleRadians;
    double backgroundZoom;
    double backgroundOffsetX;
    double backgroundOffsetY;
    double progress;
    double subjectScale = 1;
    double ringMotion = 0;
    bool operator==(const CompositionFrame&) const = default;
};

// Pure frame evaluation: no playback clock or accumulated rotation state.
CompositionFrame compositionAt(double timeSeconds, const CompositionSettings& settings, const AudioReaction& audio = {});
}
