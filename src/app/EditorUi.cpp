#include "EditorUi.hpp"
#include "musdesk/core/TimeFormat.hpp"

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
    ImGui::BeginDisabled(state.artworkDialogPending || state.audioDialogPending);
    if (ImGui::Button(state.artworkDialogPending ? "Choosing artwork..." : "Import artwork", ImVec2(-1, 0))) {
        state.requestArtwork = true;
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s", state.artworkName.empty() ? "Choose PNG/JPEG/WebP, or drop an image onto this window." : state.artworkName.c_str());
    if (!state.importError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.55f, 0.45f, 1));
        ImGui::TextWrapped("%s", state.importError.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::TextWrapped("%s", state.mediaToolsStatus.c_str());
    if (state.toolsChecked && (!state.ffmpegReady || !state.ffprobeReady)) {
        ImGui::TextWrapped("Tools: MUSDESK_FFMPEG_DIR, executable directory, or PATH. Restart after installing.");
    }
    ImGui::BeginDisabled(!state.ffprobeReady || !state.ffmpegReady || state.audioDialogPending || state.artworkDialogPending || state.audioImportPending);
    const char* audioButton = state.audioImportPending ? "Preparing audio..."
        : (state.audioDialogPending ? "Choosing audio..." : "Import audio");
    if (ImGui::Button(audioButton, ImVec2(-1, 0))) {
        state.requestAudio = true;
    }
    ImGui::EndDisabled();
    if (state.audioImportPending) {
        ImGui::TextWrapped("Decoding audio and analyzing energy...");
        if (ImGui::Button("Cancel import", ImVec2(-1, 0))) state.requestCancelAudio = true;
    }
    if (!state.audioName.empty()) ImGui::TextWrapped("%s", state.audioName.c_str());
    if (!state.audioError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.55f, 0.45f, 1));
        ImGui::TextWrapped("%s", state.audioError.c_str());
        ImGui::PopStyleColor();
    }
    if (state.audioReady) {
        ImGui::SeparatorText("Preview audio");
        if (ImGui::Button(state.audioPlaying ? "Pause" : "Play")) state.requestPlayPause = true;
        ImGui::SameLine();
        if (ImGui::Button("Restart")) state.requestRestart = true;
        const auto position = formatTime(static_cast<std::uint64_t>(state.timeSeconds));
        const auto duration = formatTime(static_cast<std::uint64_t>(state.durationSeconds));
        ImGui::Text("%s / %s", position.c_str(), duration.c_str());
        const double start = 0;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderScalar("##AudioTime", ImGuiDataType_Double, &state.timeSeconds,
                &start, &state.durationSeconds, "%.2f s", ImGuiSliderFlags_AlwaysClamp)) state.seekAudio = true;
        if (ImGui::IsItemActivated()) {
            state.beginAudioSeek = true;
            state.audioSeeking = true;
        }
        if (ImGui::IsItemDeactivated()) {
            state.endAudioSeek = true;
            state.audioSeeking = false;
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##Volume", &state.previewVolume, 0, 100, "Volume: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Mute", &state.muteAudio);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &state.loopAudio);
        ImGui::ProgressBar(state.audioEnergy, ImVec2(-1, 0), "Audio energy");
        ImGui::TextDisabled("Volume affects monitoring only.");
    }
    ImGui::SeparatorText("Track labels");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Title", "Track title", state.title.data(), state.title.size());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Artist", "Artist name", state.artist.data(), state.artist.size());
    ImGui::SeparatorText("Composition");
    if (state.audioPath.empty()) {
        ImGui::Checkbox("Animate demo", &state.animatePreview);
        ImGui::SetNextItemWidth(-1);
        const double start = 0;
        if (ImGui::SliderScalar("##Time", ImGuiDataType_Double, &state.timeSeconds,
                &start, &state.durationSeconds, "Time: %.2f s", ImGuiSliderFlags_AlwaysClamp)) state.animatePreview = false;
        const double minDuration = 10, maxDuration = 600;
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderScalar("##Duration", ImGuiDataType_Double, &state.durationSeconds,
            &minDuration, &maxDuration, "Demo length: %.0f s", ImGuiSliderFlags_AlwaysClamp);
    } else {
        ImGui::Text("Audio duration: %.3f s", state.durationSeconds);
    }
    state.timeSeconds = std::min(state.timeSeconds, state.durationSeconds);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##Rotation", &state.discRpm, 0, 45, "Rotation: %.1f RPM", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextWrapped(state.audioPath.empty()
        ? "Time and duration are a visual demo; no audio is loaded."
        : "Audio and visuals share the playback timeline. Controls are editor-only.");
    ImGui::Spacing();

    ImGui::SeparatorText("Preview settings");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##PreviewScale", &state.previewScalePercent, 50.0f, 100.0f,
        "Preview scale: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Show safe area", &state.showSafeArea);
    if (ImGui::Button("Reset preview", ImVec2(-1, 0))) {
        state.previewScalePercent = 100;
        state.showSafeArea = false;
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Preview settings are kept for this session only.");

    ImGui::SeparatorText("Export");
    ImGui::BeginDisabled();
    ImGui::Button("Export MP4", ImVec2(-1, 0));
    ImGui::EndDisabled();
    ImGui::TextWrapped("Export is not available yet.");
}

void drawPreview(const EditorUiState& state, std::uint32_t texture) {
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
    drawList->AddImage(static_cast<ImTextureID>(texture), topLeft, bottomRight, ImVec2(0, 1), ImVec2(1, 0));
    drawList->AddRect(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Border));

    if (state.showSafeArea) {
        drawList->AddRect(ImVec2(topLeft.x + width * 0.1f, topLeft.y + height * 0.1f),
            ImVec2(bottomRight.x - width * 0.1f, bottomRight.y - height * 0.1f),
            IM_COL32(100, 210, 190, 150));
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

void drawEditorUi(EditorUiState& state, std::uint32_t compositionTexture) {
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
            drawPreview(state, compositionTexture);
        }
        ImGui::EndChild();
        ImGui::TextDisabled("Composition preview  |  MP4 export is not available yet");
    }
    ImGui::End();
}
}
