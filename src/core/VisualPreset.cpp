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
    preset.spectrumColor = preset.glowColor;
    preset.glowIntensity = 1.2f;
    preset.pulseStrength = 1.7f;
    preset.discRpm = 12;
    preset.backgroundSpeed = 1.1;
    preset.backgroundZoom = 1.18f;
    preset.subjectPulse = 0.07f;
    preset.labelsPulse = 0.07f;
    preset.backgroundReactivity = 0.9f;
    preset.ringReactivity = 0.9f;
    preset.spectrumStrength = 0.9f;
    return preset;
}

VisualPreset sideKickPreset() {
    auto preset = beatPulsePreset();
    preset.name = "Side Kick";
    preset.subjectEnabled = preset.labelsEnabled = false;
    preset.cameraEnabled = true;
    preset.glowIntensity = 0.6f;
    return preset;
}

VisualPreset spectrumPulsePreset() {
    auto preset = beatPulsePreset();
    preset.name = "Spectrum Pulse";
    preset.glowEnabled = false;
    preset.spectrumEnabled = true;
    preset.subjectEnabled = preset.labelsEnabled = false;
    preset.spectrumStrength = 1.5f;
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
    for (auto component : preset.spectrumColor) range(component, 0, 1, "spectrum_style.color");
    range(preset.glowWidth, 0.2f, 3, "glow.width");
    range(preset.glowSoftness, 0.25f, 3, "glow.softness");
    range(preset.glowReactivity, 0, 3, "glow.reactivity");
    range(preset.glowBase, 0, 1, "glow.base");
    range(preset.glowAttackMs, 0, 250, "glow.attack_ms");
    range(preset.glowReleaseMs, 0, 1000, "glow.release_ms");
    range(preset.spectrumPoints, 32, 256, "spectrum_style.points");
    range(preset.spectrumSharpness, 0, 1, "spectrum_style.sharpness");
    range(preset.spectrumRandomness, 0, 1, "spectrum_style.randomness");
    range(preset.spectrumSpeed, 0, 4, "spectrum_style.speed");
    range(preset.spectrumLineWidth, 0.25f, 3, "spectrum_style.line_width");
    range(preset.spectrumGap, 0.015f, 0.25f, "spectrum_style.gap");
    range(preset.spectrumIntensity, 0, 3, "spectrum_style.intensity");
    range(preset.spectrumAttackMs, 0, 250, "spectrum_style.attack_ms");
    range(preset.spectrumReleaseMs, 0, 1000, "spectrum_style.release_ms");
    range(preset.glowIntensity, 0, 3, "glow.intensity (0..3)");
    range(preset.pulseStrength, 0, 2, "glow.pulse_strength (0..2)");
    range(preset.discRpm, -45, 45, "disc_rpm (-45..45)");
    range(preset.backgroundSpeed, 0, 3, "background.speed (0..3)");
    range(preset.backgroundZoom, 1.1, 1.5, "background.zoom (1.1..1.5)");
    range(preset.backgroundBrightness, 0, 1.5, "background.brightness (0..1.5)");
    range(preset.beatThreshold, 0, 1, "reactivity.beat_threshold (0..1)");
    range(preset.subjectPulse, 0, 0.12f, "reactivity.subject_pulse (0..0.12)");
    range(preset.labelsPulse, 0, 0.12f, "reactivity.labels_pulse (0..0.12)");
    range(preset.backgroundReactivity, 0, 1, "reactivity.background (0..1)");
    range(preset.ringReactivity, 0, 1, "reactivity.ring (0..1)");
    range(preset.spectrumStrength, 0, 4, "reactivity.spectrum (0..4)");
    range(preset.spectrumGainDb, -24, 24, "reactivity.spectrum_gain_db (-24..24)");
    range(preset.spectrumBalance, -1, 1, "reactivity.spectrum_balance (-1..1)");
    range(preset.cameraStrength, 0, 0.05f, "effects.camera_strength (0..0.05)");
    range(static_cast<int>(preset.beatBand), 0, 2, "reactivity.beat_band");
}
void resetGlow(VisualPreset& preset) {
    const VisualPreset defaults;
    preset.glowColor = defaults.glowColor;
    preset.glowIntensity = defaults.glowIntensity;
    preset.glowWidth = preset.glowSoftness = preset.glowReactivity = preset.glowBase = 1;
    preset.glowAttackMs = preset.glowReleaseMs = 0;
}
void resetSpectrum(VisualPreset& preset) {
    const VisualPreset defaults;
    preset.spectrumColor = defaults.spectrumColor;
    preset.spectrumStrength = defaults.spectrumStrength;
    preset.spectrumGainDb = preset.spectrumBalance = 0;
    preset.spectrumPoints = 128;
    preset.spectrumSharpness = preset.spectrumRandomness = preset.spectrumSpeed = 1;
    preset.spectrumLineWidth = preset.spectrumIntensity = 1;
    preset.spectrumGap = defaults.spectrumGap;
    preset.spectrumAttackMs = preset.spectrumReleaseMs = 0;
}
}
