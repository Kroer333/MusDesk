#include "musdesk/core/Composition.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        musdesk::CompositionSettings settings;
        settings.discRpm = 6;
        const auto atZero = musdesk::compositionAt(0, settings);
        const auto atFive = musdesk::compositionAt(5, settings);
        const auto atTen = musdesk::compositionAt(10, settings);
        require(atZero.discAngleRadians == 0, "Initial angle must be zero");
        require(std::abs(atFive.discAngleRadians - std::numbers::pi) < 1e-10, "6 RPM: half turn at 5 seconds");
        require(std::abs(atTen.discAngleRadians) < 1e-10, "6 RPM: full turn at 10 seconds");
        (void)musdesk::compositionAt(100, settings);
        const auto seekBack = musdesk::compositionAt(5, settings);
        require(atFive.discAngleRadians == seekBack.discAngleRadians
            && atFive.backgroundZoom == seekBack.backgroundZoom
            && atFive.backgroundOffsetX == seekBack.backgroundOffsetX, "Seeking must reproduce the same frame");
        require(musdesk::compositionAt(-5, settings).progress == 0, "Negative time must clamp to start");
        require(musdesk::compositionAt(1000, settings).progress == 1, "Time beyond duration must clamp to end");
        settings.discRpm = 0;
        require(musdesk::compositionAt(20, settings).discAngleRadians == 0, "Stopped disc must not rotate");
        settings.discRpm = -6;
        require(std::abs(musdesk::compositionAt(5, settings).discAngleRadians + std::numbers::pi) < 1e-10,
            "Negative RPM must reverse rotation");
        settings.durationSeconds = 0;
        settings.discRpm = std::numeric_limits<double>::infinity();
        const auto invalid = musdesk::compositionAt(std::numeric_limits<double>::quiet_NaN(), settings);
        require(std::isfinite(invalid.progress) && invalid.timeSeconds == 0 && invalid.discAngleRadians == 0,
            "Invalid numeric input must not produce nonfinite frame data");
        std::cout << "Composition timing and seek checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
