#pragma once

#include <string>
#include "musdesk/core/VisualPreset.hpp"

namespace musdesk {
struct CompositionSettings {
    std::string title = "Track title";
    std::string artist = "Artist name";
    double durationSeconds = 252.0;
    VisualPreset preset;
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
};

// Pure frame evaluation: no playback clock or accumulated rotation state.
CompositionFrame compositionAt(double timeSeconds, const CompositionSettings& settings);
}
