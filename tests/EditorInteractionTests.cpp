#include "EditorUi.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct PreviewRect { ImVec2 min, max; };
constexpr ImTextureID previewTexture = 42;

PreviewRect frame(musdesk::EditorUiState& state) {
    state.previewKeyboardEditing = state.previewArrowKeysCaptured = false;
    ImGui::NewFrame();
    musdesk::drawEditorUi(state,previewTexture);
    ImGui::Render();
    for (const auto* list : ImGui::GetDrawData()->CmdLists) {
        for (const auto& command : list->CmdBuffer) {
            if (command.GetTexID() != previewTexture || command.ElemCount < 6) continue;
            PreviewRect rect{{100000,100000},{-100000,-100000}};
            for (unsigned i=0; i<6; ++i) {
                const auto p = list->VtxBuffer[command.VtxOffset+list->IdxBuffer[command.IdxOffset+i]].pos;
                rect.min.x = std::min(rect.min.x,p.x); rect.min.y = std::min(rect.min.y,p.y);
                rect.max.x = std::max(rect.max.x,p.x); rect.max.y = std::max(rect.max.y,p.y);
            }
            return rect;
        }
    }
    throw std::runtime_error("Preview image not drawn");
}
ImVec2 previewPoint(const PreviewRect& rect, musdesk::CanvasPoint point) {
    return {rect.min.x+point.x*(rect.max.x-rect.min.x),rect.min.y+point.y*(rect.max.y-rect.min.y)};
}
void mouse(musdesk::EditorUiState& state, ImVec2 point, bool down) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(point.x,point.y);
    io.AddMouseButtonEvent(0,down);
    frame(state);
}
void key(musdesk::EditorUiState& state, ImGuiKey key, bool down) {
    ImGui::GetIO().AddKeyEvent(key,down);
    frame(state);
}
}

int main() {
    ImGui::CreateContext();
    try {
        using namespace musdesk;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1280,800};
        io.DeltaTime = 1.0f/60;
        unsigned char* pixels; int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        io.Fonts->SetTexID(1);
        configureEditorStyle(1);
        EditorUiState state;
        state.format = CompositionFormat::Landscape;
        state.shortEdge = 720;
        state.audioPlaying = state.audioReady = state.animatePreview = true;
        state.audioPath = "generated.wav";
        state.timeSeconds = 4;
        state.previewSnapping = false;
        CompositionSettings settings;
        settings.format = state.format;
        settings.shortEdge = state.shortEdge;
        const auto layout = layoutFor(settings);
        auto rect = frame(state);
        rect = frame(state);
        const auto center = previewPoint(rect,canvasToPreview({layout.discX,layout.discY},layout,state.previewFrame));
        const auto original = state.design;

        state.foregroundPlacementLocked = true;
        mouse(state,center,false);
        mouse(state,center,true);
        require(state.previewSelection == CompositionElement::Foreground && !state.previewDragging,
            "Locked foreground must remain selectable without starting a drag");
        mouse(state,{center.x+35,center.y+20},true);
        mouse(state,{center.x+35,center.y+20},false);
        key(state,ImGuiKey_RightArrow,true);
        require(state.previewArrowKeysCaptured && state.design == original,
            "Locked arrows must be consumed without moving or seeking audio");
        key(state,ImGuiKey_RightArrow,false);
        require(state.audioPlaying && state.animatePreview && state.timeSeconds == 4,
            "Locked interactions must not change playback");

        state.foregroundPlacementLocked = false;
        mouse(state,center,false);
        mouse(state,center,true);
        mouse(state,{center.x+35,center.y+20},true);
        require(state.previewDragging && state.design.foregroundX != original.foregroundX,
            "Unlocked foreground must move while playback is running");
        require(state.design.labelsX == original.labelsX && state.design.labelsY == original.labelsY,
            "Foreground dragging must preserve label placement");
        applyVisualPreset(state,beatPulsePreset());
        state.design.background = BackgroundKind::Solid;
        state.design.labelsX = 0.025f;
        state.timeSeconds = 4.1;
        frame(state);
        require(state.audioPlaying && state.animatePreview && state.timeSeconds == 4.1
            && state.preset == beatPulsePreset(),"Preset editing during a drag must preserve playback and the new effects");
        key(state,ImGuiKey_Escape,true);
        auto expected = original;
        expected.background = BackgroundKind::Solid;
        expected.labelsX = 0.025f;
        require(state.design == expected && state.preset == beatPulsePreset(),
            "Escape must cancel placement without reverting independent effect edits");
        key(state,ImGuiKey_Escape,false);
        mouse(state,center,false);
        mouse(state,center,true);
        mouse(state,center,false);
        key(state,ImGuiKey_RightArrow,true);
        require(state.previewKeyboardEditing && state.previewArrowKeysCaptured
            && std::abs(state.design.foregroundX-original.foregroundX-1.0f/1280) < 0.00001f,
            "Arrow must move one output pixel during playback");
        key(state,ImGuiKey_RightArrow,false);
        key(state,ImGuiMod_Shift,true);
        key(state,ImGuiKey_UpArrow,true);
        require(std::abs(state.design.foregroundY-original.foregroundY+10.0f/720) < 0.00001f,
            "Shift arrow must move ten output pixels during playback");
        key(state,ImGuiKey_UpArrow,false);
        key(state,ImGuiMod_Shift,false);

        state.labelsPlacementLocked = true;
        rect = frame(state);
        const auto labels = previewPoint(rect,canvasToPreview({layout.textX,layout.titleY},layout,state.previewFrame));
        mouse(state,labels,false);
        mouse(state,labels,true);
        require(state.previewSelection == CompositionElement::Labels && !state.previewDragging,
            "Label lock must work independently of the foreground lock");
        mouse(state,labels,false);
        key(state,ImGuiKey_LeftArrow,true);
        require(state.design.labelsX == expected.labelsX && state.design.labelsY == expected.labelsY,
            "Locked labels and timeline must ignore keyboard positioning");
        key(state,ImGuiKey_LeftArrow,false);
        require(state.audioPlaying && state.animatePreview && !state.requestRestart && !state.seekAudio
            && !state.requestPlayPause && state.timeSeconds == 4.1,
            "Placement and effects must never request transport changes");
        std::cout << "Editor interaction checks passed\n";
        ImGui::DestroyContext();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ImGui::DestroyContext();
        return 1;
    }
}
