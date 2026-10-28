#include "musdesk/core/Composition.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace musdesk {
void validateImageFraming(const ImageFraming& framing) {
    if (!std::isfinite(framing.zoom) || framing.zoom < 1 || framing.zoom > 3
        || !std::isfinite(framing.focusX) || framing.focusX < 0 || framing.focusX > 1
        || !std::isfinite(framing.focusY) || framing.focusY < 0 || framing.focusY > 1)
        throw std::runtime_error("Image framing requires zoom 1..3 and focus coordinates 0..1.");
}
ImageCrop imageCrop(double imageAspect, double targetAspect, const ImageFraming& framing) {
    if (!std::isfinite(imageAspect) || imageAspect <= 0) imageAspect = 1;
    if (!std::isfinite(targetAspect) || targetAspect <= 0) targetAspect = 1;
    const auto safe = [](double value, double low, double high, double fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    const double zoom = safe(framing.zoom, 1, 3, 1);
    const double scaleX = std::min(1.0, targetAspect / imageAspect) / zoom;
    const double scaleY = std::min(1.0, imageAspect / targetAspect) / zoom;
    return {scaleX, scaleY,
        std::clamp(safe(framing.focusX, 0, 1, 0.5), scaleX / 2, 1 - scaleX / 2),
        std::clamp(safe(framing.focusY, 0, 1, 0.5), scaleY / 2, 1 - scaleY / 2)};
}

const char* formatName(CompositionFormat format) {
    switch (format) {
    case CompositionFormat::Portrait: return "9:16";
    case CompositionFormat::Landscape: return "16:9";
    case CompositionFormat::Square: return "1:1";
    case CompositionFormat::PortraitThreeFour: return "3:4";
    case CompositionFormat::LandscapeFourThree: return "4:3";
    }
    throw std::runtime_error("Unsupported composition format.");
}
CompositionFormat parseCompositionFormat(const std::string& name) {
    for (auto format : {CompositionFormat::Portrait, CompositionFormat::Landscape, CompositionFormat::Square,
        CompositionFormat::PortraitThreeFour, CompositionFormat::LandscapeFourThree})
        if (name == formatName(format)) return format;
    throw std::runtime_error("Unsupported composition format.");
}
CompositionLayout layoutFor(CompositionFormat format) {
    switch (format) {
    case CompositionFormat::Portrait: return {{1080,1920},540,760,370,540,900,1250,1340,160,920,1480};
    case CompositionFormat::Landscape: return {{1920,1080},520,540,320,1390,800,470,575,1060,1720,740};
    case CompositionFormat::Square: return {{1080,1080},540,355,235,540,900,720,795,160,920,930};
    case CompositionFormat::PortraitThreeFour: return {{1080,1440},540,480,280,540,900,930,1020,160,920,1180};
    case CompositionFormat::LandscapeFourThree: return {{1440,1080},400,540,250,1080,600,470,575,800,1360,740};
    }
    throw std::runtime_error("Unsupported composition format.");
}
OutputSize outputSize(CompositionFormat format, int shortEdge) {
    if (shortEdge != 720 && shortEdge != 1080) throw std::runtime_error("Choose a 720 or 1080 pixel short edge.");
    const auto canvas = layoutFor(format).canvas;
    return {canvas.width * shortEdge / 1080, canvas.height * shortEdge / 1080};
}

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
