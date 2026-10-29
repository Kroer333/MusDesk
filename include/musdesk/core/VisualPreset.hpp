#pragma once

#include <array>
#include <string>
#include "musdesk/core/AudioAnalysis.hpp"

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
    float spectrumStrength = 0.65f;
    float spectrumGainDb = 0;
    float spectrumBalance = 0;
    bool glowEnabled = true;
    bool spectrumEnabled = false;
    bool wavesEnabled = false;
    bool subjectEnabled = true;
    bool backgroundEnabled = true;
    bool cameraEnabled = false;
    float cameraStrength = 0.025f;
    BeatBand beatBand = BeatBand::Bass;

    bool operator==(const VisualPreset&) const = default;
};

VisualPreset softGlowPreset();
VisualPreset beatPulsePreset();
VisualPreset sideKickPreset();
VisualPreset spectrumPulsePreset();
void validatePreset(const VisualPreset& preset);
}
