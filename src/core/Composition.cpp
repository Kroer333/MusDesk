#include "musdesk/core/Composition.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace musdesk {
namespace {
struct OverlayBounds { float left, right, top, bottom; };
std::optional<OverlayBounds> visibleLabels(const CompositionLayout& layout, const CompositionDesign& design) {
    std::optional<OverlayBounds> bounds;
    const auto include = [&](float left, float right, float top, float bottom) {
        if (!bounds) bounds = OverlayBounds{left,right,top,bottom};
        else {
            bounds->left = std::min(bounds->left,left);
            bounds->right = std::max(bounds->right,right);
            bounds->top = std::min(bounds->top,top);
            bounds->bottom = std::max(bounds->bottom,bottom);
        }
    };
    const float scale = layout.textScale;
    if (design.showTitle) include(layout.textX-layout.textWidth/2,layout.textX+layout.textWidth/2,
        layout.titleY-80*scale,layout.titleY+20*scale);
    if (design.showArtist) include(layout.textX-layout.textWidth/2,layout.textX+layout.textWidth/2,
        layout.artistY-60*scale,layout.artistY+20*scale);
    if (design.showProgress) include(layout.timelineLeft-13,layout.timelineRight+13,
        layout.timelineY-13,layout.timelineY+13);
    if (design.showTimestamps) include(layout.timelineLeft,layout.timelineRight,
        layout.timelineY+20*scale,layout.timelineY+85*scale);
    return bounds;
}
}
bool needsArtwork(const CompositionDesign& design) {
    return design.background == BackgroundKind::Artwork || design.scene == SceneKind::Vinyl || design.scene == SceneKind::Cover;
}
void validateDesign(const CompositionDesign& design) {
    const auto range = [](double value,double low,double high) {
        if (!std::isfinite(value) || value < low || value > high)
            throw std::runtime_error("Composition design value is out of range.");
    };
    range(static_cast<int>(design.scene),0,3);
    range(static_cast<int>(design.background),0,2);
    for (float value : design.backgroundTop) range(value,0,1);
    for (float value : design.backgroundBottom) range(value,0,1);
    range(design.foregroundScale,0.5,1.5);
    range(design.labelsScale,0.5,1.5);
    for (float value : {design.foregroundX,design.foregroundY,design.labelsX,design.labelsY}) range(value,-0.3f,0.3f);
    range(design.barsHeight,0.25,1.5);
}
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

CompositionLayout layoutFor(const CompositionSettings& settings) {
    validateDesign(settings.design);
    auto layout = layoutFor(settings.format);
    const auto& design = settings.design;
    if (design.scene == SceneKind::Titles) {
        layout.discX = layout.textX = layout.canvas.width*0.5f;
        layout.discY = layout.canvas.height*0.38f;
        layout.textWidth = layout.canvas.width*0.8f;
        layout.titleY = layout.canvas.height*0.45f;
        layout.artistY = layout.titleY+90;
        layout.timelineLeft = layout.canvas.width*0.15f;
        layout.timelineRight = layout.canvas.width*0.85f;
        layout.timelineY = layout.canvas.height*0.70f;
    }
    const bool foreground = design.scene != SceneKind::Titles;
    layout.discRadius = std::min(layout.discRadius*(foreground ? design.foregroundScale : 1),
        std::min(layout.canvas.width,layout.canvas.height)*0.45f);
    layout.discX = std::clamp(layout.discX+(foreground ? design.foregroundX*layout.canvas.width : 0),
        layout.discRadius+12,layout.canvas.width-layout.discRadius-12);
    layout.discY = std::clamp(layout.discY+(foreground ? design.foregroundY*layout.canvas.height : 0),
        layout.discRadius+12,layout.canvas.height-layout.discRadius-12);
    const float width = layout.canvas.width, height = layout.canvas.height;
    layout.textWidth = std::min(layout.textWidth*design.labelsScale,width-24);
    layout.textX = std::clamp(layout.textX+design.labelsX*width,layout.textWidth/2+12,width-layout.textWidth/2-12);
    const float title = layout.titleY;
    layout.artistY = title+(layout.artistY-title)*design.labelsScale;
    layout.timelineY = title+(layout.timelineY-title)*design.labelsScale;
    layout.textScale = design.labelsScale;
    const auto labels = visibleLabels(layout,design);
    const float shift = labels ? std::clamp(design.labelsY*height,12-labels->top,height-12-labels->bottom)
        : design.labelsY*height;
    layout.titleY += shift;
    layout.artistY += shift;
    layout.timelineY += shift;
    const float margin = design.showProgress ? 25.0f : 12.0f;
    const float halfBar = std::min((layout.timelineRight-layout.timelineLeft)*design.labelsScale/2,
        std::min(layout.textX-margin,width-layout.textX-margin));
    layout.timelineLeft = layout.textX-halfBar;
    layout.timelineRight = layout.textX+halfBar;
    return layout;
}

OutputSize previewSize(CompositionFormat format, int displayedShortEdge, int outputShortEdge) {
    outputSize(format,outputShortEdge);
    int edge = 1080;
    for (int candidate : {360,540,720,1080}) {
        if (displayedShortEdge <= candidate) { edge = candidate; break; }
    }
    edge = std::min(edge,outputShortEdge);
    const auto canvas = layoutFor(format).canvas;
    return {canvas.width*edge/1080,canvas.height*edge/1080};
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
    frame.subjectScale = 1 + (preset.subjectEnabled
        ? std::min(0.18, safe(preset.subjectPulse, 0.12) * beat * safe(preset.pulseStrength, 2)) : 0);
    const auto layout = layoutFor(settings);
    const bool foreground = settings.design.scene != SceneKind::Titles;
    const auto labels = visibleLabels(layout,settings.design);
    const auto fit = [&](double value,double origin,double size) {
        if (value > origin) return (size-12-origin)/(value-origin);
        if (value < origin) return (origin-12)/(origin-value);
        return 2.0;
    };
    if (labels) frame.subjectScale = std::min({frame.subjectScale,fit(labels->left,layout.discX,layout.canvas.width),
        fit(labels->right,layout.discX,layout.canvas.width),fit(labels->top,layout.discY,layout.canvas.height),
        fit(labels->bottom,layout.discY,layout.canvas.height)});
    frame.subjectScale = std::min({frame.subjectScale,foreground ? fit(layout.discX-layout.discRadius,layout.discX,layout.canvas.width) : 2.0,
        foreground ? fit(layout.discX+layout.discRadius,layout.discX,layout.canvas.width) : 2.0,
        foreground ? fit(layout.discY-layout.discRadius,layout.discY,layout.canvas.height) : 2.0,
        foreground ? fit(layout.discY+layout.discRadius,layout.discY,layout.canvas.height) : 2.0});
    frame.ringMotion = preset.wavesEnabled ? drive * safe(preset.ringReactivity, 1) : 0;
    if (preset.cameraEnabled) {
        const double kick = std::isfinite(audio.kick) ? std::clamp<double>(audio.kick, -1, 1) : 0;
        frame.cameraOffsetX = kick * safe(preset.cameraStrength, 0.05) * safe(preset.pulseStrength, 2);
        frame.cameraZoom = 1 + 2.2 * std::abs(frame.cameraOffsetX);
    }
    if (speed > 0 && preset.backgroundEnabled) {
        const double motion = drive * safe(preset.backgroundReactivity, 1);
        frame.backgroundZoom += 0.06 * motion;
        frame.backgroundAngleRadians += 0.012 * motion * std::sin(backgroundTime * 1.1);
        frame.backgroundOffsetX += 0.008 * motion * std::sin(backgroundTime * 1.7);
        frame.backgroundOffsetY += 0.006 * motion * std::cos(backgroundTime * 1.3);
    }
    if (!settings.design.animateBackground) {
        frame.backgroundAngleRadians = frame.backgroundOffsetX = frame.backgroundOffsetY = 0;
        frame.backgroundZoom = 1;
    }
    return frame;
}
}
