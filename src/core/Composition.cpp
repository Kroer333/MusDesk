#include "musdesk/core/Composition.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace musdesk {
CompositionFrame compositionAt(double timeSeconds, const CompositionSettings& settings) {
    const double duration = std::isfinite(settings.durationSeconds) && settings.durationSeconds > 0
        ? settings.durationSeconds : 252.0;
    const double t = std::isfinite(timeSeconds) ? std::clamp(timeSeconds, 0.0, duration) : 0.0;
    const double rpm = std::isfinite(settings.discRpm) ? std::clamp(settings.discRpm, -120.0, 120.0) : 0.0;
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
    return {t, angle, 0.018 * std::sin(t * 0.045),
        1.16 + 0.02 * std::sin(t * 0.08),
        0.015 * std::sin(t * 0.035), 0.012 * std::cos(t * 0.03), t / duration};
}
}
