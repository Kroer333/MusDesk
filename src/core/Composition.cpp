#include "musdesk/core/Composition.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace musdesk {
CompositionFrame compositionAt(double timeSeconds, const CompositionSettings& settings, const AudioReaction& audio) {
    const double duration = std::isfinite(settings.durationSeconds) && settings.durationSeconds > 0
        ? settings.durationSeconds : 252.0;
    const double t = std::isfinite(timeSeconds) ? std::clamp(timeSeconds, 0.0, duration) : 0.0;
    const double rpm = std::isfinite(settings.preset.discRpm) ? std::clamp(settings.preset.discRpm, -45.0, 45.0) : 0.0;
    double angle = 0;
    if (rpm != 0) {
        const double period = 60.0 / std::abs(rpm);
        if (std::isfinite(period)) {
            angle = std::fmod(t, period) / period * 2.0 * std::numbers::pi;
            if (rpm < 0) {
                angle = -angle;
            }
        }
    }
    const auto& preset = settings.preset;
    const double speed = std::isfinite(preset.backgroundSpeed) ? std::clamp(preset.backgroundSpeed, 0.0, 3.0) : 0;
    const double zoom = std::isfinite(preset.backgroundZoom) ? std::clamp<double>(preset.backgroundZoom, 1.1, 1.5) : 1.16;
    const double backgroundTime = t * speed;
    CompositionFrame frame{t, angle, 0.018 * std::sin(backgroundTime * 0.045),
        zoom + 0.02 * std::sin(backgroundTime * 0.08),
        0.015 * std::sin(backgroundTime * 0.035), 0.012 * std::cos(backgroundTime * 0.03), t / duration};
    const auto safe = [](double value, double maximum) {
        return std::isfinite(value) ? std::clamp(value, 0.0, maximum) : 0.0;
    };
    const double beat = safe(audio.beat, 1);
    const double drive = (0.65 * safe(audio.bass, 1) + 0.35 * beat) * safe(preset.pulseStrength, 2);
    frame.subjectScale = 1 + std::min(0.18, safe(preset.subjectPulse, 0.12) * beat * safe(preset.pulseStrength, 2));
    frame.ringMotion = drive * safe(preset.ringReactivity, 1);
    if (speed > 0) {
        const double motion = drive * safe(preset.backgroundReactivity, 1);
        frame.backgroundZoom += 0.06 * motion;
        frame.backgroundAngleRadians += 0.012 * motion * std::sin(backgroundTime * 1.1);
        frame.backgroundOffsetX += 0.008 * motion * std::sin(backgroundTime * 1.7);
        frame.backgroundOffsetY += 0.006 * motion * std::cos(backgroundTime * 1.3);
    }
    return frame;
}
}
