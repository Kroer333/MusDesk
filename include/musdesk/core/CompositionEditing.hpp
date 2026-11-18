#pragma once

#include "musdesk/core/Composition.hpp"
#include <optional>
#include <vector>

namespace musdesk {
enum class CompositionElement { None, Foreground, Labels };
struct CanvasPoint { float x, y; };
struct CanvasBounds { float left, right, top, bottom; };

std::vector<CanvasBounds> visibleLabelRegions(const CompositionLayout& layout, const CompositionDesign& design);
std::optional<CanvasBounds> visibleLabelBounds(const CompositionLayout& layout, const CompositionDesign& design);
std::optional<CanvasBounds> elementBounds(const CompositionSettings& settings, CompositionElement element);
CanvasPoint canvasToPreview(CanvasPoint point, const CompositionLayout& layout, const CompositionFrame& frame);
CanvasPoint previewToCanvas(CanvasPoint normalized, const CompositionLayout& layout, const CompositionFrame& frame);
CompositionElement hitComposition(const CompositionSettings& settings, const CompositionFrame& frame,
    CanvasPoint normalized, float tolerance = 0);
CompositionDesign moveCompositionElement(const CompositionSettings& initial, CompositionElement element,
    CanvasPoint delta);
}
