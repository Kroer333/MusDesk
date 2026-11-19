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
std::array<CanvasPoint,4> boundsCorners(const CanvasBounds& bounds) {
    return {{{bounds.left,bounds.top},{bounds.right,bounds.top},
        {bounds.right,bounds.bottom},{bounds.left,bounds.bottom}}};
}
CompositionDesign resizeCompositionElement(const CompositionSettings& initial, CompositionElement element,
    ResizeCorner corner, CanvasPoint delta) {
    const auto bounds = elementBounds(initial,element);
    const int index = static_cast<int>(corner);
    if (!bounds || index < 0 || index > 3 || !std::isfinite(delta.x) || !std::isfinite(delta.y)
        || (delta.x == 0 && delta.y == 0)) return initial.design;
    const CanvasPoint center{(bounds->left+bounds->right)/2,(bounds->top+bounds->bottom)/2};
    const auto point = boundsCorners(*bounds)[index];
    const float vx = point.x-center.x, vy = point.y-center.y;
    const double factor = 1+(double(vx)*delta.x+double(vy)*delta.y)/(double(vx)*vx+double(vy)*vy);
    if (std::abs(factor-1) < 0.000001) return initial.design;
    auto resized = initial;
    if (element == CompositionElement::Foreground) {
        const auto base = layoutFor(initial.format);
        const auto layout = layoutFor(initial);
        const float maximum = std::min(1.5f,std::min(base.canvas.width,base.canvas.height)*0.45f/base.discRadius);
        resized.design.foregroundScale = static_cast<float>(std::clamp(layout.discRadius/base.discRadius*factor,0.5,double(maximum)));
    } else resized.design.labelsScale = static_cast<float>(std::clamp(initial.design.labelsScale*factor,0.5,1.5));
    const auto after = elementBounds(resized,element);
    if (after) resized.design = moveCompositionElement(resized,element,
        {center.x-(after->left+after->right)/2,center.y-(after->top+after->bottom)/2});
    const auto finalBounds = elementBounds(resized,element);
    if (finalBounds && std::abs(finalBounds->left-bounds->left) < 0.0001f
        && std::abs(finalBounds->right-bounds->right) < 0.0001f
        && std::abs(finalBounds->top-bounds->top) < 0.0001f
        && std::abs(finalBounds->bottom-bounds->bottom) < 0.0001f) return initial.design;
    return resized.design;
}
CompositionAlignment snapCompositionElement(const CompositionSettings& settings, CompositionElement element,
    CanvasPoint tolerance) {
    CompositionAlignment result{settings.design};
    if (!std::isfinite(tolerance.x) || !std::isfinite(tolerance.y) || tolerance.x < 0 || tolerance.y < 0) return result;
    const auto layout = layoutFor(settings);
    auto current = settings;
    for (int axis=0; axis<2; ++axis) {
        const auto bounds = elementBounds(current,element);
        if (!bounds) break;
        const float size = axis == 0 ? layout.canvas.width : layout.canvas.height;
        const float low = axis == 0 ? bounds->left : bounds->top;
        const float high = axis == 0 ? bounds->right : bounds->bottom;
        const std::array<float,3> positions{(low+high)/2,low,high};
        const std::array<float,3> targets{size*0.5f,size*0.1f,size*0.9f};
        float nearest = (axis == 0 ? tolerance.x : tolerance.y)+0.0001f;
        auto best = current.design;
        std::optional<float> guide;
        for (int candidate=0; candidate<3; ++candidate) {
            const float distance = targets[candidate]-positions[candidate];
            if (std::abs(distance) > nearest) continue;
            auto proposed = current;
            proposed.design = moveCompositionElement(current,element,axis == 0 ? CanvasPoint{distance,0} : CanvasPoint{0,distance});
            const auto placed = elementBounds(proposed,element);
            if (!placed) continue;
            const float a = axis == 0 ? placed->left : placed->top;
            const float b = axis == 0 ? placed->right : placed->bottom;
            const float position = candidate == 0 ? (a+b)/2 : (candidate == 1 ? a : b);
            if (std::abs(position-targets[candidate]) > 0.01f) continue;
            nearest = std::abs(distance);
            best = proposed.design;
            guide = targets[candidate];
        }
        current.design = best;
        if (axis == 0) result.guideX = guide;
        else result.guideY = guide;
    }
    result.design = current.design;
    return result;
}
CompositionDesign nudgeCompositionElement(const CompositionSettings& settings, CompositionElement element, CanvasPoint outputPixels) {
    const auto canvas = layoutFor(settings.format).canvas;
    const auto output = outputSize(settings.format,settings.shortEdge);
    return moveCompositionElement(settings,element,
        {outputPixels.x*canvas.width/output.width,outputPixels.y*canvas.height/output.height});
}
CompositionDesign editCompositionElement(const CompositionSettings& settings, CompositionElement element, CompositionEdit action) {
    const auto bounds = elementBounds(settings,element);
    if (!bounds) return settings.design;
    const CanvasPoint center{(bounds->left+bounds->right)/2,(bounds->top+bounds->bottom)/2};
    const auto canvas = layoutFor(settings.format).canvas;
    if (action == CompositionEdit::CenterHorizontal)
        return moveCompositionElement(settings,element,{canvas.width*0.5f-center.x,0});
    if (action == CompositionEdit::CenterVertical)
        return moveCompositionElement(settings,element,{0,canvas.height*0.5f-center.y});
    auto edited = settings;
    if (action == CompositionEdit::ResetPosition) {
        if (element == CompositionElement::Foreground) edited.design.foregroundX = edited.design.foregroundY = 0;
        else edited.design.labelsX = edited.design.labelsY = 0;
    } else if (action == CompositionEdit::ResetSize) {
        if (element == CompositionElement::Foreground) edited.design.foregroundScale = 1;
        else edited.design.labelsScale = 1;
        const auto after = elementBounds(edited,element);
        edited.design = moveCompositionElement(edited,element,
            {center.x-(after->left+after->right)/2,center.y-(after->top+after->bottom)/2});
    }
    return edited.design;
}
}
