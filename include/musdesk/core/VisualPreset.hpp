#pragma once

#include <array>
#include <string>
#include "musdesk/core/AudioAnalysis.hpp"

namespace musdesk {
struct VisualPreset {
    std::string name = "Soft Glow";
    std::array<float, 3> glowColor{0.24f, 0.60f, 1.0f};
    float glowIntensity = 0.8f;
    float glowWidth = 1;
    float glowSoftness = 1;
    float glowReactivity = 1;
    float glowBase = 1;
    float glowAttackMs = 0;
    float glowReleaseMs = 0;
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
    std::array<float,3> spectrumColor{0.24f,0.60f,1.0f};
    int spectrumPoints = 128;
    float spectrumSharpness = 1;
    float spectrumRandomness = 1;
    float spectrumSpeed = 1;
    float spectrumLineWidth = 1;
    float spectrumGap = 0.055f;
    float spectrumIntensity = 1;
    float spectrumAttackMs = 0;
    float spectrumReleaseMs = 0;
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
void resetGlow(VisualPreset& preset);
void resetSpectrum(VisualPreset& preset);
}
