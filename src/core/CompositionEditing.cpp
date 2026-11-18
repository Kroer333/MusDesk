#include "musdesk/core/CompositionEditing.hpp"
#include <algorithm>
#include <cmath>

namespace musdesk {
std::vector<CanvasBounds> visibleLabelRegions(const CompositionLayout& layout, const CompositionDesign& design) {
    std::vector<CanvasBounds> regions;
    const float scale = layout.textScale;
    if (design.showTitle) regions.push_back({layout.textX-layout.textWidth/2,layout.textX+layout.textWidth/2,
        layout.titleY-80*scale,layout.titleY+20*scale});
    if (design.showArtist) regions.push_back({layout.textX-layout.textWidth/2,layout.textX+layout.textWidth/2,
        layout.artistY-60*scale,layout.artistY+20*scale});
    if (design.showProgress) regions.push_back({layout.timelineLeft-13,layout.timelineRight+13,
        layout.timelineY-13,layout.timelineY+13});
    if (design.showTimestamps) regions.push_back({layout.timelineLeft,layout.timelineRight,
        layout.timelineY+20*scale,layout.timelineY+85*scale});
    return regions;
}
std::optional<CanvasBounds> visibleLabelBounds(const CompositionLayout& layout, const CompositionDesign& design) {
    std::optional<CanvasBounds> bounds;
    for (const auto& region : visibleLabelRegions(layout,design)) {
        if (!bounds) bounds = region;
        else {
            bounds->left = std::min(bounds->left,region.left);
            bounds->right = std::max(bounds->right,region.right);
            bounds->top = std::min(bounds->top,region.top);
            bounds->bottom = std::max(bounds->bottom,region.bottom);
        }
    }
    return bounds;
}
std::optional<CanvasBounds> elementBounds(const CompositionSettings& settings, CompositionElement element) {
    const auto layout = layoutFor(settings);
    if (element == CompositionElement::Labels) return visibleLabelBounds(layout,settings.design);
    if (element == CompositionElement::Foreground && settings.design.scene != SceneKind::Titles)
        return CanvasBounds{layout.discX-layout.discRadius,layout.discX+layout.discRadius,
            layout.discY-layout.discRadius,layout.discY+layout.discRadius};
    return {};
}
CanvasPoint previewToCanvas(CanvasPoint point, const CompositionLayout& layout, const CompositionFrame& frame) {
    return {static_cast<float>(((point.x-0.5-frame.cameraOffsetX)/frame.cameraZoom+0.5)*layout.canvas.width),
        static_cast<float>(((point.y-0.5)/frame.cameraZoom+0.5)*layout.canvas.height)};
}
CanvasPoint canvasToPreview(CanvasPoint point, const CompositionLayout& layout, const CompositionFrame& frame) {
    point.x = layout.discX+(point.x-layout.discX)*static_cast<float>(frame.subjectScale);
    point.y = layout.discY+(point.y-layout.discY)*static_cast<float>(frame.subjectScale);
    return {static_cast<float>((point.x/layout.canvas.width-0.5)*frame.cameraZoom+0.5+frame.cameraOffsetX),
        static_cast<float>((point.y/layout.canvas.height-0.5)*frame.cameraZoom+0.5)};
}
CompositionElement hitComposition(const CompositionSettings& settings, const CompositionFrame& frame,
    CanvasPoint normalized, float tolerance) {
    if (!std::isfinite(normalized.x) || !std::isfinite(normalized.y)
        || normalized.x < 0 || normalized.x > 1 || normalized.y < 0 || normalized.y > 1)
        return CompositionElement::None;
    const auto layout = layoutFor(settings);
    auto point = previewToCanvas(normalized,layout,frame);
    point.x = layout.discX+(point.x-layout.discX)/static_cast<float>(frame.subjectScale);
    point.y = layout.discY+(point.y-layout.discY)/static_cast<float>(frame.subjectScale);
    const auto contains = [&](const CanvasBounds& bounds) {
        return point.x >= bounds.left-tolerance && point.x <= bounds.right+tolerance
            && point.y >= bounds.top-tolerance && point.y <= bounds.bottom+tolerance;
    };
    for (const auto& region : visibleLabelRegions(layout,settings.design))
        if (contains(region)) return CompositionElement::Labels;
    if (const auto bounds = elementBounds(settings,CompositionElement::Foreground); bounds && contains(*bounds)) {
        if (settings.design.scene != SceneKind::Vinyl
            || std::hypot(point.x-layout.discX,point.y-layout.discY) <= layout.discRadius+tolerance)
            return CompositionElement::Foreground;
    }
    return CompositionElement::None;
}
CompositionDesign moveCompositionElement(const CompositionSettings& initial, CompositionElement element, CanvasPoint delta) {
    auto design = initial.design;
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || (delta.x == 0 && delta.y == 0)) return design;
    const auto layout = layoutFor(initial);
    const auto base = layoutFor(initial.format);
    const float width = layout.canvas.width, height = layout.canvas.height;
    if (element == CompositionElement::Foreground && design.scene != SceneKind::Titles) {
        const float x = std::clamp(layout.discX+delta.x,layout.discRadius+12,width-layout.discRadius-12);
        const float y = std::clamp(layout.discY+delta.y,layout.discRadius+12,height-layout.discRadius-12);
        design.foregroundX = std::clamp((x-base.discX)/width,-0.3f,0.3f);
        design.foregroundY = std::clamp((y-base.discY)/height,-0.3f,0.3f);
    } else if (element == CompositionElement::Labels) {
        const auto bounds = visibleLabelBounds(layout,design);
        if (!bounds) return design;
        const float x = std::clamp(layout.textX+delta.x,layout.textWidth/2+12,width-layout.textWidth/2-12);
        const float y = layout.titleY+std::clamp(delta.y,12-bounds->top,height-12-bounds->bottom);
        const float baseX = design.scene == SceneKind::Titles ? width*0.5f : base.textX;
        const float baseY = design.scene == SceneKind::Titles ? height*0.45f : base.titleY;
        design.labelsX = std::clamp((x-baseX)/width,-0.3f,0.3f);
        design.labelsY = std::clamp((y-baseY)/height,-0.3f,0.3f);
    } else return design;
    auto moved = initial;
    moved.design = design;
    const auto after = layoutFor(moved);
    if (element == CompositionElement::Foreground && after.discX == layout.discX && after.discY == layout.discY)
        return initial.design;
    if (element == CompositionElement::Labels && after.textX == layout.textX && after.titleY == layout.titleY)
        return initial.design;
    return design;
}
}
