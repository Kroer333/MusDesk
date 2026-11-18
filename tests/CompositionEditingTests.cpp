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
                    const auto label = canvasToPreview({layout.textX,layout.titleY},layout,frame);
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
        std::cout << "Preview hit testing, coordinate transforms, movement limits and drag history passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
