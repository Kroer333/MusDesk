#pragma once

#include <array>
#include <string>

namespace musdesk {
struct VisualPreset {
    std::string name = "Soft Glow";
    std::array<float, 3> glowColor{0.24f, 0.60f, 1.0f};
    float glowIntensity = 0.8f;
    float pulseStrength = 0.6f;
    double discRpm = 8;
    double backgroundSpeed = 0.6;
    float backgroundZoom = 1.16f;
    float backgroundBrightness = 0.88f;
    float beatThreshold = 0.55f;
    float subjectPulse = 0.05f;
    float backgroundReactivity = 0.65f;
    float ringReactivity = 0.55f;

    bool operator==(const VisualPreset&) const = default;
};

VisualPreset softGlowPreset();
VisualPreset beatPulsePreset();
void validatePreset(const VisualPreset& preset);
}
