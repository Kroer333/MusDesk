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
        const auto right = musdesk::imageCrop(2, 1, {1,1,0.5f});
        require(right.scaleX == 0.5 && right.centerX == 0.75 && right.scaleY == 1,
            "Right focus must select the right half of a wide image without exposing empty pixels");
        for (double aspect : {0.5,1.0,2.0}) for (double target : {0.5625,0.75,1.0,4.0/3,16.0/9}) {
            for (float zoom : {1.0f,3.0f}) for (float focus : {0.0f,1.0f}) {
                const auto crop = musdesk::imageCrop(aspect, target, {zoom,focus,focus});
                require(crop.centerX - crop.scaleX / 2 >= 0 && crop.centerX + crop.scaleX / 2 <= 1
                    && crop.centerY - crop.scaleY / 2 >= 0 && crop.centerY + crop.scaleY / 2 <= 1,
                    "Every aspect/zoom/focus combination must remain inside the artwork");
            }
        }
        const auto invalidCrop = musdesk::imageCrop(0, std::numeric_limits<double>::infinity(),
            {std::numeric_limits<float>::quiet_NaN(),-1,2});
        require(std::isfinite(invalidCrop.centerX) && std::isfinite(invalidCrop.scaleY),
            "Unsafe render inputs must not produce invalid UV coordinates");
        for (auto format : {musdesk::CompositionFormat::Portrait, musdesk::CompositionFormat::Landscape,
            musdesk::CompositionFormat::Square, musdesk::CompositionFormat::PortraitThreeFour, musdesk::CompositionFormat::LandscapeFourThree}) {
            require(musdesk::parseCompositionFormat(musdesk::formatName(format)) == format, "Format identifiers must round trip");
            const auto layout = musdesk::layoutFor(format);
            musdesk::CompositionSettings maximumPulse;
            maximumPulse.format = format;
            maximumPulse.preset.subjectPulse = 0.12f;
            maximumPulse.preset.pulseStrength = 2;
            const auto pose = musdesk::compositionAt(1, maximumPulse, {1,1,1});
            require(pose.subjectScale >= 1 && pose.subjectScale <= 1.18
                && layout.discY + (layout.timelineY + 85 - layout.discY)*pose.subjectScale <= layout.canvas.height - 11
                && layout.discX + (layout.timelineRight - layout.discX)*pose.subjectScale <= layout.canvas.width - 11,
                "Maximum overlay pulse must keep the timeline and timestamps within each layout");
            const double maximumRing = layout.discRadius * 1.18 * 1.21;
            require(layout.discX > maximumRing && layout.discY > maximumRing
                && layout.discX + maximumRing < layout.canvas.width && layout.discY + maximumRing < layout.canvas.height,
                "Maximum vinyl/ring pulse must remain inside every layout");
            require(layout.timelineY + 60 < layout.canvas.height
                && layout.textX - layout.textWidth / 2 >= 0 && layout.textX + layout.textWidth / 2 <= layout.canvas.width,
                "Text/timeline must fit every layout");
            for (int edge : {720,1080}) {
                const auto size = musdesk::outputSize(format, edge);
                require(std::min(size.width,size.height) == edge && size.width % 2 == 0 && size.height % 2 == 0,
                    "All output sizes must have the selected short edge and be H.264 compatible");
            }
        }
        musdesk::CompositionSettings settings;
        settings.preset.discRpm = 6;
        const auto atZero = musdesk::compositionAt(0, settings);
        const auto atFive = musdesk::compositionAt(5, settings);
        const auto atTen = musdesk::compositionAt(10, settings);
        require(atZero.discAngleRadians == 0, "Initial angle must be zero");
        const musdesk::AudioReaction hit{0.7f, 0.8f, 1};
        const auto reacting = musdesk::compositionAt(5, settings, hit);
        require(reacting.subjectScale > 1 && reacting.ringMotion > 0
            && reacting.backgroundZoom > atFive.backgroundZoom, "Bass and beats must drive subject, rim and background");
        (void)musdesk::compositionAt(99, settings, {1, 1, 1});
        require(musdesk::compositionAt(5, settings, hit) == reacting, "Reactive frame must be independent of playback history");
        const auto bassOnly = musdesk::compositionAt(5, settings, {0, 1, 0});
        auto kicked = settings;
        kicked.preset = musdesk::sideKickPreset();
        auto kickAudio = hit;
        kickAudio.kick = 1;
        const auto rightKick = musdesk::compositionAt(5, kicked, kickAudio);
        kickAudio.kick = -1;
        const auto leftKick = musdesk::compositionAt(5, kicked, kickAudio);
        require(rightKick.subjectScale == 1 && rightKick.cameraOffsetX > 0
            && leftKick.cameraOffsetX == -rightKick.cameraOffsetX && leftKick.cameraZoom > 1,
            "Side Kick must move the whole screen in either direction without subject scaling");
        kicked.preset.cameraEnabled = false;
        require(musdesk::compositionAt(5, kicked, kickAudio).cameraOffsetX == 0,
            "Disabling screen kick must restore a neutral camera");
        require(bassOnly.subjectScale == 1 && bassOnly.ringMotion > 0,
            "Sustained bass must move rim/background without triggering subject scaling");
        auto disabled = settings;
        disabled.preset.pulseStrength = 0;
        require(musdesk::compositionAt(5, disabled, hit) == musdesk::compositionAt(5, disabled),
            "Zero audio response must disable all new modulation");
        auto legacy = settings;
        legacy.preset.subjectPulse = legacy.preset.backgroundReactivity = legacy.preset.ringReactivity = 0;
        require(musdesk::compositionAt(5, legacy, hit) == musdesk::compositionAt(5, legacy),
            "Legacy preset values must preserve the old frame pose");
        const auto invalidAudio = musdesk::compositionAt(5, settings,
            {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1});
        require(invalidAudio == atFive, "Invalid audio values must not corrupt the composition");
        require(std::abs(atFive.discAngleRadians - std::numbers::pi) < 1e-10, "6 RPM: half turn at 5 seconds");
        require(std::abs(atTen.discAngleRadians) < 1e-10, "6 RPM: full turn at 10 seconds");
        (void)musdesk::compositionAt(100, settings);
        const auto seekBack = musdesk::compositionAt(5, settings);
        require(atFive.discAngleRadians == seekBack.discAngleRadians
            && atFive.backgroundZoom == seekBack.backgroundZoom
            && atFive.backgroundOffsetX == seekBack.backgroundOffsetX, "Seeking must reproduce the same frame");
        require(musdesk::compositionAt(-5, settings).progress == 0, "Negative time must clamp to start");
        require(musdesk::compositionAt(1000, settings).progress == 1, "Time beyond duration must clamp to end");
        settings.preset.discRpm = 0;
        require(musdesk::compositionAt(20, settings).discAngleRadians == 0, "Stopped disc must not rotate");
        settings.preset.discRpm = -6;
        require(std::abs(musdesk::compositionAt(5, settings).discAngleRadians + std::numbers::pi) < 1e-10,
            "Negative RPM must reverse rotation");
        settings.preset.backgroundSpeed = 0;
        const auto staticStart = musdesk::compositionAt(0, settings);
        const auto staticLater = musdesk::compositionAt(100, settings);
        require(staticStart.backgroundAngleRadians == staticLater.backgroundAngleRadians
            && staticStart.backgroundZoom == staticLater.backgroundZoom
            && staticStart.backgroundOffsetY == staticLater.backgroundOffsetY, "Zero background speed must freeze its pose");
        settings.durationSeconds = 0;
        settings.preset.discRpm = std::numeric_limits<double>::infinity();
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
