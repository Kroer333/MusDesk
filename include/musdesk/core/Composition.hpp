#pragma once

#include <string>
#include "musdesk/core/VisualPreset.hpp"
#include "musdesk/core/AudioAnalysis.hpp"

namespace musdesk {
enum class CompositionFormat { Portrait, Landscape, Square, PortraitThreeFour, LandscapeFourThree };
enum class SceneKind { Vinyl, Cover, Bars, Titles };
enum class BackgroundKind { Artwork, Gradient, Solid };
struct CompositionDesign {
    SceneKind scene = SceneKind::Vinyl;
    BackgroundKind background = BackgroundKind::Artwork;
    std::array<float,3> backgroundTop{0.085f,0.13f,0.20f};
    std::array<float,3> backgroundBottom{0.025f,0.04f,0.075f};
    bool animateBackground = true;
    bool showTitle = true, showArtist = true, showProgress = true, showTimestamps = true;
    float foregroundScale = 1, foregroundX = 0, foregroundY = 0;
    float labelsScale = 1, labelsX = 0, labelsY = 0;
    float barsHeight = 1;
    bool operator==(const CompositionDesign&) const = default;
};
void validateDesign(const CompositionDesign& design);
bool needsArtwork(const CompositionDesign& design);
struct OutputSize {
    int width;
    int height;
    bool operator==(const OutputSize&) const = default;
};
struct CompositionLayout {
    OutputSize canvas;
    float discX, discY, discRadius;
    float textX, textWidth, titleY, artistY;
    float timelineLeft, timelineRight, timelineY;
    float textScale = 1;
};
struct ImageFraming {
    float zoom = 1;
    float focusX = 0.5f;
    float focusY = 0.5f;
    bool operator==(const ImageFraming&) const = default;
};
struct ImageCrop {
    double scaleX, scaleY, centerX, centerY;
};
void validateImageFraming(const ImageFraming& framing);
ImageCrop imageCrop(double imageAspect, double targetAspect, const ImageFraming& framing);
const char* formatName(CompositionFormat format);
CompositionFormat parseCompositionFormat(const std::string& name);
OutputSize outputSize(CompositionFormat format, int shortEdge = 1080);
OutputSize previewSize(CompositionFormat format, int displayedShortEdge, int outputShortEdge = 1080);
CompositionLayout layoutFor(CompositionFormat format);

struct CompositionSettings {
    std::string title = "Track title";
    std::string artist = "Artist name";
    double durationSeconds = 252.0;
    VisualPreset preset;
    CompositionFormat format = CompositionFormat::Portrait;
    int shortEdge = 1080;
    ImageFraming backgroundFraming;
    ImageFraming labelFraming;
    float backgroundSoftness = 1;
    CompositionDesign design;
    bool operator==(const CompositionSettings&) const = default;
};
CompositionLayout layoutFor(const CompositionSettings& settings);

struct CompositionFrame {
    double timeSeconds;
    double discAngleRadians;
    double backgroundAngleRadians;
    double backgroundZoom;
    double backgroundOffsetX;
    double backgroundOffsetY;
    double progress;
    double subjectScale = 1;
    double labelsScale = 1;
    double ringMotion = 0;
    double cameraOffsetX = 0;
    double cameraZoom = 1;
    bool operator==(const CompositionFrame&) const = default;
};

// Pure frame evaluation: no playback clock or accumulated rotation state.
CompositionFrame compositionAt(double timeSeconds, const CompositionSettings& settings, const AudioReaction& audio = {});
}
