#include "musdesk/core/VisualPreset.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace musdesk {
VisualPreset softGlowPreset() { return {}; }

VisualPreset beatPulsePreset() {
    VisualPreset preset;
    preset.name = "Beat Pulse";
    preset.glowColor = {0.60f, 0.35f, 1.0f};
    preset.glowIntensity = 1.2f;
    preset.pulseStrength = 1.7f;
    preset.discRpm = 12;
    preset.backgroundSpeed = 1.1;
    preset.backgroundZoom = 1.18f;
    preset.subjectPulse = 0.07f;
    preset.backgroundReactivity = 0.9f;
    preset.ringReactivity = 0.9f;
    preset.spectrumStrength = 0.9f;
    return preset;
}

VisualPreset sideKickPreset() {
    auto preset = beatPulsePreset();
    preset.name = "Side Kick";
    preset.subjectEnabled = false;
    preset.cameraEnabled = true;
    preset.glowIntensity = 0.6f;
    return preset;
}

void validatePreset(const VisualPreset& preset) {
    if (preset.name.empty() || preset.name.size() > 96
        || std::all_of(preset.name.begin(), preset.name.end(), [](char c) { return c == ' '; })
        || std::any_of(preset.name.begin(), preset.name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error("Preset name must contain 1-96 UTF-8 bytes without control characters.");
    auto range = [](double value, double minimum, double maximum, const char* field) {
        if (!std::isfinite(value) || value < minimum || value > maximum)
            throw std::runtime_error(std::string("Preset value out of range: ") + field);
    };
    for (auto component : preset.glowColor) range(component, 0, 1, "glow.color");
    range(preset.glowIntensity, 0, 3, "glow.intensity (0..3)");
    range(preset.pulseStrength, 0, 2, "glow.pulse_strength (0..2)");
    range(preset.discRpm, -45, 45, "disc_rpm (-45..45)");
    range(preset.backgroundSpeed, 0, 3, "background.speed (0..3)");
    range(preset.backgroundZoom, 1.1, 1.5, "background.zoom (1.1..1.5)");
    range(preset.backgroundBrightness, 0, 1.5, "background.brightness (0..1.5)");
    range(preset.beatThreshold, 0, 1, "reactivity.beat_threshold (0..1)");
    range(preset.subjectPulse, 0, 0.12f, "reactivity.subject_pulse (0..0.12)");
    range(preset.backgroundReactivity, 0, 1, "reactivity.background (0..1)");
    range(preset.ringReactivity, 0, 1, "reactivity.ring (0..1)");
    range(preset.spectrumStrength, 0, 1, "reactivity.spectrum (0..1)");
    range(preset.spectrumGainDb, -24, 24, "reactivity.spectrum_gain_db (-24..24)");
    range(preset.spectrumBalance, -1, 1, "reactivity.spectrum_balance (-1..1)");
    range(preset.cameraStrength, 0, 0.05f, "effects.camera_strength (0..0.05)");
    range(preset.cameraThreshold, 0, 1, "effects.camera_threshold (0..1)");
}
}
