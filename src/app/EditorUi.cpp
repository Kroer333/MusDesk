#include "EditorUi.hpp"

#include <imgui.h>

#include <algorithm>

namespace musdesk {
namespace {
void drawControls(EditorUiState& state) {
    ImGui::SeparatorText("Composition");
    ImGui::TextUnformatted("Vertical music visualizer");
    ImGui::TextDisabled("1080 x 1920  /  9:16");
    ImGui::TextDisabled("Target: 30 FPS");
    ImGui::Spacing();

    ImGui::SeparatorText("Media");
    ImGui::TextDisabled("No audio or artwork loaded");
    ImGui::BeginDisabled();
    ImGui::Button("Import audio", ImVec2(-1, 0));
    ImGui::Button("Import artwork", ImVec2(-1, 0));
    ImGui::EndDisabled();
    ImGui::TextWrapped("Audio and artwork import is not available yet.");
    ImGui::Spacing();

    ImGui::SeparatorText("Preview settings");
    ImGui::ColorEdit3("Canvas color", state.canvasColor.data(), ImGuiColorEditFlags_NoInputs);
    ImGui::TextUnformatted("Quick colors");
    if (ImGui::Button("Dark")) {
        state.canvasColor = {0.09f, 0.12f, 0.18f};
    }
    ImGui::SameLine();
    if (ImGui::Button("Indigo")) {
        state.canvasColor = {0.19f, 0.13f, 0.32f};
    }
    ImGui::SameLine();
    if (ImGui::Button("Warm")) {
        state.canvasColor = {0.30f, 0.16f, 0.12f};
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##PreviewScale", &state.previewScalePercent, 50.0f, 100.0f,
        "Preview scale: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Show safe area", &state.showSafeArea);
    if (ImGui::Button("Reset preview", ImVec2(-1, 0))) {
        state = EditorUiState{};
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Preview settings are kept for this session only.");

    ImGui::SeparatorText("Export");
    ImGui::BeginDisabled();
    ImGui::Button("Export MP4", ImVec2(-1, 0));
    ImGui::EndDisabled();
    ImGui::TextWrapped("Export is not available yet.");
}

void drawPreview(const EditorUiState& state) {
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::TextDisabled("9:16");
    ImGui::Separator();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float padding = ImGui::GetFontSize();
    const float usableWidth = available.x - 2.0f * padding;
    const float usableHeight = available.y - 2.0f * padding;
    if (usableWidth <= 0.0f || usableHeight <= 0.0f) {
        return;
    }

    constexpr float aspectRatio = 9.0f / 16.0f;
    const float width = std::min(usableWidth, usableHeight * aspectRatio)
        * (state.previewScalePercent / 100.0f);
    const float height = width / aspectRatio;
    const ImVec2 topLeft(origin.x + (available.x - width) * 0.5f,
        origin.y + (available.y - height) * 0.5f);
    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 canvasColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(state.canvasColor[0], state.canvasColor[1], state.canvasColor[2], 1.0f));
    drawList->AddRectFilled(topLeft, bottomRight, canvasColor);
    drawList->AddRect(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Border));

    if (state.showSafeArea) {
        drawList->AddRect(ImVec2(topLeft.x + width * 0.1f, topLeft.y + height * 0.1f),
            ImVec2(bottomRight.x - width * 0.1f, bottomRight.y - height * 0.1f),
            IM_COL32(100, 210, 190, 150));
    }

    const char* label = "Your visualizer\nwill appear here";
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    // Hide the placeholder when the canvas is too small to contain its text.
    if (labelSize.x < width - padding && labelSize.y < height - padding) {
        drawList->AddText(ImVec2(topLeft.x + (width - labelSize.x) * 0.5f,
            topLeft.y + (height - labelSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
    }
    ImGui::Dummy(available);
}
}

void configureEditorStyle(float displayScale) {
    if (displayScale <= 0.0f) {
        displayScale = 1.0f;
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};
    ImGui::StyleColorsDark();
    style.WindowPadding = ImVec2(18, 16);
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
    style.WindowRounding = 0;
    style.ChildRounding = 8;
    style.FrameRounding = 4;
    style.GrabRounding = 4;
    style.FontSizeBase = 16;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.07f, 0.09f, 1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.09f, 0.115f, 1);
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.24f, 0.29f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.14f, 0.22f, 0.24f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.35f, 0.36f, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.43f, 0.40f, 1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.39f, 0.82f, 0.75f, 1);
    style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_CheckMark];
    style.ScaleAllSizes(displayScale);
    style.FontScaleDpi = displayScale;
}

void drawEditorUi(EditorUiState& state) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("MusDesk workspace", nullptr, flags)) {
        ImGui::TextUnformatted("MusDesk");
        ImGui::SameLine();
        ImGui::TextDisabled("Music visualizer");
        ImGui::Separator();

        const float em = ImGui::GetFontSize();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float controlsWidth = std::clamp(available.x * 0.30f, em * 16.0f, em * 22.0f);
        const float panelHeight = std::max(1.0f, available.y - em * 2.0f);
        if (ImGui::BeginChild("Controls", ImVec2(controlsWidth, panelHeight), ImGuiChildFlags_Borders)) {
            drawControls(state);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Canvas", ImVec2(0, panelHeight), ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            drawPreview(state);
        }
        ImGui::EndChild();
        ImGui::TextDisabled("Ready  |  Canvas preview  |  Audio playback is not available yet");
    }
    ImGui::End();
}
}
