#include "musdesk/core/CompositionEditing.hpp"
#include "musdesk/core/CompositionHistory.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a-b) < 0.002f; }
}
int main() {
    try {
        using namespace musdesk;
        for (auto format : {CompositionFormat::Portrait,CompositionFormat::Landscape,CompositionFormat::Square,
            CompositionFormat::PortraitThreeFour,CompositionFormat::LandscapeFourThree}) {
            for (auto scene : {SceneKind::Vinyl,SceneKind::Cover,SceneKind::Bars,SceneKind::Titles}) {
                CompositionSettings settings;
                settings.format = format;
                settings.design.scene = scene;
                const auto layout = layoutFor(settings);
                for (double kick : {-0.035,0.0,0.035}) {
                    CompositionFrame frame{};
                    frame.cameraOffsetX = kick;
                    frame.cameraZoom = 1.1;
                    frame.subjectScale = 1.08;
                    frame.labelsScale = 1.14;
                    const auto label = canvasToPreview({layout.textX,layout.titleY},layout,frame,CompositionElement::Labels);
                    require(hitComposition(settings,frame,label) == CompositionElement::Labels,
                        "Hit testing must follow pulse, camera and every scene/aspect ratio");
                    if (scene != SceneKind::Titles) {
                        const auto center = canvasToPreview({layout.discX,layout.discY},layout,frame);
                        require(hitComposition(settings,frame,center) == CompositionElement::Foreground,
                            "Foreground center must remain selectable with camera motion");
                    } else require(!elementBounds(settings,CompositionElement::Foreground),"Titles must have no foreground selection");
                    const auto a = previewToCanvas({0.2f,0.3f},layout,frame);
                    const auto b = previewToCanvas({0.3f,0.4f},layout,frame);
                    require(near(b.x-a.x,layout.canvas.width*0.1f/1.1f)
                        && near(b.y-a.y,layout.canvas.height*0.1f/1.1f),
                        "Drag displacement must remove camera zoom independently of preview pixels");
                }
                for (auto element : {CompositionElement::Foreground,CompositionElement::Labels}) {
                    auto moved = settings;
                    moved.design = moveCompositionElement(settings,element,{20,18});
                    const auto after = layoutFor(moved);
                    if (element == CompositionElement::Foreground && scene != SceneKind::Titles)
                        require(near(after.discX-layout.discX,20) && near(after.discY-layout.discY,18),
                            "Foreground must follow the pointer in logical canvas units");
                    if (element == CompositionElement::Labels)
                        require(near(after.textX-layout.textX,20) && near(after.titleY-layout.titleY,18),
                            "The whole label group must follow the pointer");
                    const auto extreme = moveCompositionElement(settings,element,{100000,-100000});
                    validateDesign(extreme);
                    moved.design = extreme;
                    const auto bounds = elementBounds(moved,element);
                    if (bounds) require(bounds->left >= 11.99f && bounds->top >= 11.99f
                        && bounds->right <= layout.canvas.width-11.99f && bounds->bottom <= layout.canvas.height-11.99f,
                        "Dragging outside preview must keep elements inside canvas and supported ranges");
                    const auto originalBounds = elementBounds(settings,element);
                    if (!originalBounds) continue;
                    for (int edge : {720,1080}) {
                        auto numeric = settings;
                        numeric.shortEdge = edge;
                        const auto output = outputSize(format,edge);
                        const CanvasPoint center{(originalBounds->left+originalBounds->right)*0.5f*output.width/layout.canvas.width+12,
                            (originalBounds->top+originalBounds->bottom)*0.5f*output.height/layout.canvas.height-8};
                        numeric.design = setCompositionElementTransform(numeric,element,center,0.8f);
                        const auto actual = elementBounds(numeric,element);
                        require(near((actual->left+actual->right)*0.5f*output.width/layout.canvas.width,center.x)
                            && near((actual->top+actual->bottom)*0.5f*output.height/layout.canvas.height,center.y),
                            "Numeric center and size must use output pixels in every layout");
                        if (element == CompositionElement::Foreground)
                            require(numeric.design.foregroundScale == 0.8f && numeric.design.labelsX == settings.design.labelsX
                                && numeric.design.labelsScale == settings.design.labelsScale,"Numeric foreground edit must preserve labels");
                        else require(numeric.design.labelsScale == 0.8f && numeric.design.foregroundX == settings.design.foregroundX
                            && numeric.design.foregroundScale == settings.design.foregroundScale,"Numeric label edit must preserve foreground");
                        numeric.design = setCompositionElementTransform(numeric,element,{-1e30f,1e30f},99);
                        validateDesign(numeric.design);
                        require(setCompositionElementTransform(numeric,element,{0,0},std::numeric_limits<float>::quiet_NaN()) == numeric.design,
                            "Nonfinite numeric placement input must be ignored");
                    }
                    const CanvasPoint center{(originalBounds->left+originalBounds->right)/2,
                        (originalBounds->top+originalBounds->bottom)/2};
                    for (int i=0; i<4; ++i) {
                        const auto corner = boundsCorners(*originalBounds)[i];
                        moved.design = resizeCompositionElement(settings,element,static_cast<ResizeCorner>(i),
                            {(corner.x-center.x)*0.1f,(corner.y-center.y)*0.1f});
                        const auto resized = elementBounds(moved,element);
                        require(resized && resized->right-resized->left > originalBounds->right-originalBounds->left,
                            "Every corner must enlarge either element proportionally");
                        require(near((resized->left+resized->right)/2,center.x)
                            && near((resized->top+resized->bottom)/2,center.y),
                            "Resize must preserve selection center when frame limits allow it");
                        for (CanvasPoint delta : {CanvasPoint{100000,100000},{-100000,-100000}}) {
                            moved.design = resizeCompositionElement(settings,element,static_cast<ResizeCorner>(i),delta);
                            validateDesign(moved.design);
                            const auto bounded = elementBounds(moved,element);
                            require(bounded && bounded->left >= 11.99f && bounded->top >= 11.99f
                                && bounded->right <= layout.canvas.width-11.99f && bounded->bottom <= layout.canvas.height-11.99f,
                                "Extreme resize gestures must stay within the frame without inversion");
                        }
                    }
                    moved.design = resizeCompositionElement(settings,element,ResizeCorner::BottomRight,{0,0});
                    require(moved == settings,"A handle click without movement must not change settings");
                    for (int edge : {720,1080}) {
                        auto positioned = settings;
                        positioned.shortEdge = edge;
                        const auto output = outputSize(format,edge);
                        positioned.design = nudgeCompositionElement(positioned,element,{1,-10});
                        const auto nudged = elementBounds(positioned,element);
                        require(near(((nudged->left+nudged->right)/2-center.x)*output.width/layout.canvas.width,1)
                            && near(((nudged->top+nudged->bottom)/2-center.y)*output.height/layout.canvas.height,-10),
                            "Nudges must use output pixels in every format and resolution");
                    }
                    moved.design = editCompositionElement(settings,element,CompositionEdit::CenterHorizontal);
                    auto centered = elementBounds(moved,element);
                    require(near((centered->left+centered->right)/2,layout.canvas.width*0.5f)
                        && near((centered->top+centered->bottom)/2,center.y),
                        "Center X must align the selection without moving it vertically");
                    moved.design = editCompositionElement(settings,element,CompositionEdit::CenterVertical);
                    centered = elementBounds(moved,element);
                    require(near((centered->top+centered->bottom)/2,layout.canvas.height*0.5f)
                        && near((centered->left+centered->right)/2,center.x),
                        "Center Y must align the selection without moving it horizontally");
                    auto custom = settings;
                    if (element == CompositionElement::Foreground) {
                        custom.design.foregroundScale = 0.8f;
                        custom.design.foregroundX = custom.design.foregroundY = 0.03f;
                    } else {
                        custom.design.labelsScale = 0.8f;
                        custom.design.labelsX = custom.design.labelsY = 0.03f;
                    }
                    const auto resetPosition = editCompositionElement(custom,element,CompositionEdit::ResetPosition);
                    require((element == CompositionElement::Foreground
                        ? resetPosition.foregroundX == 0 && resetPosition.foregroundY == 0 && resetPosition.foregroundScale == 0.8f
                        : resetPosition.labelsX == 0 && resetPosition.labelsY == 0 && resetPosition.labelsScale == 0.8f),
                        "Reset position must restore offsets while retaining size");
                    moved = custom;
                    moved.design = editCompositionElement(custom,element,CompositionEdit::ResetSize);
                    const auto resized = elementBounds(moved,element);
                    const auto original = elementBounds(custom,element);
                    require((element == CompositionElement::Foreground ? moved.design.foregroundScale : moved.design.labelsScale) == 1
                        && near((resized->left+resized->right)/2,(original->left+original->right)/2)
                        && near((resized->top+resized->bottom)/2,(original->top+original->bottom)/2),
                        "Reset size must restore exactly 1x and preserve center when possible");
                    if (element == CompositionElement::Foreground)
                        require(moved.design.labelsScale == custom.design.labelsScale && moved.design.labelsX == custom.design.labelsX
                            && moved.design.labelsY == custom.design.labelsY,"Foreground actions must preserve labels");
                    else require(moved.design.foregroundScale == custom.design.foregroundScale && moved.design.foregroundX == custom.design.foregroundX
                        && moved.design.foregroundY == custom.design.foregroundY,"Label actions must preserve foreground");
                }
            }
        }
        CompositionSettings settings;
        settings.format = CompositionFormat::Portrait;
        settings.design.foregroundX = 0.3f;
        const auto clamped = layoutFor(settings);
        auto moved = settings;
        moved.design = moveCompositionElement(settings,CompositionElement::Foreground,{-10,0});
        require(near(layoutFor(moved).discX,clamped.discX-10),
            "Moving inward from an already-clamped offset must have no dead zone");
        settings = {};
        settings.design.showTitle = settings.design.showArtist = false;
        settings.design.showProgress = settings.design.showTimestamps = false;
        require(!elementBounds(settings,CompositionElement::Labels),"Hidden labels must not leave a selectable box");
        const auto layout = layoutFor(settings);
        const auto corner = canvasToPreview({layout.discX+layout.discRadius*0.9f,layout.discY+layout.discRadius*0.9f},layout,{});
        require(hitComposition(settings,{},corner) == CompositionElement::None,"Empty corners outside vinyl must not select it");
        require(hitComposition(settings,{}, {-0.1f,0.5f}) == CompositionElement::None,"Letterbox clicks must not select elements");
        require(moveCompositionElement(settings,CompositionElement::Foreground,{std::numeric_limits<float>::quiet_NaN(),0}) == settings.design,
            "Invalid pointer coordinates must leave the design unchanged");
        require(moveCompositionElement(settings,CompositionElement::Foreground,{0,0}) == settings.design,"Clicking without moving must be a no-op");
        CompositionHistory history;
        history.reset(settings);
        for (int i=1; i<=25; ++i) {
            moved = settings;
            moved.design = moveCompositionElement(settings,CompositionElement::Foreground,{float(i),float(i)});
            history.observe(moved,true);
        }
        history.observe(moved,false);
        require(history.undo() == settings && !history.canUndo(),"One drag must create exactly one undo step");
        require(history.redo() == moved,"Redo must restore the complete drag");
        history.reset(settings);
        history.observe(moved,true);
        history.observe(settings,false);
        require(!history.canUndo(),"Cancelling a drag must not add an undo step");
        settings = {};
        settings.format = CompositionFormat::Landscape;
        moved = settings;
        moved.design = moveCompositionElement(settings,CompositionElement::Foreground,{429,0});
        const auto centered = snapCompositionElement(moved,CompositionElement::Foreground,{12,12});
        moved.design = centered.design;
        require(centered.guideX && near(*centered.guideX,960) && near(layoutFor(moved).discX,960),
            "Movement near the canvas center must snap exactly and identify its guide");
        moved = settings;
        const auto margin = snapCompositionElement(moved,CompositionElement::Foreground,{12,12});
        moved.design = margin.design;
        const auto marginBounds = elementBounds(moved,CompositionElement::Foreground);
        require(margin.guideX && near(*margin.guideX,192) && near(marginBounds->left,192),
            "The element edge must snap to the ten-percent safe margin");
        moved.design = moveCompositionElement(settings,CompositionElement::Foreground,{-80,70});
        const auto unsnapped = snapCompositionElement(moved,CompositionElement::Foreground,{2,2});
        require(unsnapped.design == moved.design && !unsnapped.guideX && !unsnapped.guideY,
            "Movement far from guides must remain free");
        require(snapCompositionElement(settings,CompositionElement::Foreground,{-1,12}).design == settings.design,
            "Invalid snap tolerances must leave settings unchanged");
        history.reset(settings);
        for (int i=1; i<=20; ++i) {
            moved = settings;
            moved.design = resizeCompositionElement(settings,CompositionElement::Foreground,ResizeCorner::BottomRight,
                {float(i),float(i)});
            history.observe(moved,true);
        }
        history.observe(moved,false);
        require(history.undo() == settings && !history.canUndo() && history.redo() == moved,
            "A resize gesture must be one complete Undo/Redo operation");
        history.reset(settings);
        history.observe(moved,true);
        history.observe(settings,false);
        require(!history.canUndo(),"Cancelled resize must restore settings without adding history");
        history.reset(settings);
        moved = settings;
        for (int i=0; i<20; ++i) {
            moved.design = nudgeCompositionElement(moved,CompositionElement::Foreground,{1,0});
            history.observe(moved,true);
        }
        history.observe(moved,false);
        require(history.undo() == settings && !history.canUndo() && history.redo() == moved,
            "Repeated key nudges must be grouped into one undoable gesture");
        std::cout << "Preview hit testing, coordinate transforms, movement limits and drag history passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
