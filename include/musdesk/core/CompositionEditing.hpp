#pragma once

#include "musdesk/core/Composition.hpp"
#include <array>
#include <optional>
#include <vector>

namespace musdesk {
enum class CompositionElement { None, Foreground, Labels };
enum class ResizeCorner { None = -1, TopLeft, TopRight, BottomRight, BottomLeft };
struct CanvasPoint { float x, y; };
struct CanvasBounds { float left, right, top, bottom; };
struct CompositionAlignment {
    CompositionDesign design;
    std::optional<float> guideX, guideY;
};

std::vector<CanvasBounds> visibleLabelRegions(const CompositionLayout& layout, const CompositionDesign& design);
std::optional<CanvasBounds> visibleLabelBounds(const CompositionLayout& layout, const CompositionDesign& design);
std::optional<CanvasBounds> elementBounds(const CompositionSettings& settings, CompositionElement element);
CanvasPoint canvasToPreview(CanvasPoint point, const CompositionLayout& layout, const CompositionFrame& frame);
CanvasPoint previewToCanvas(CanvasPoint normalized, const CompositionLayout& layout, const CompositionFrame& frame);
CompositionElement hitComposition(const CompositionSettings& settings, const CompositionFrame& frame,
    CanvasPoint normalized, float tolerance = 0);
CompositionDesign moveCompositionElement(const CompositionSettings& initial, CompositionElement element,
    CanvasPoint delta);
std::array<CanvasPoint,4> boundsCorners(const CanvasBounds& bounds);
CompositionDesign resizeCompositionElement(const CompositionSettings& initial, CompositionElement element,
    ResizeCorner corner, CanvasPoint delta);
CompositionAlignment snapCompositionElement(const CompositionSettings& settings, CompositionElement element,
    CanvasPoint tolerance);
}
