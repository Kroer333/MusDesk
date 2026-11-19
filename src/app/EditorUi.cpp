#include "EditorUi.hpp"
#include "musdesk/core/TimeFormat.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace musdesk {
void applyVisualPreset(EditorUiState& state, const VisualPreset& preset) {
    validatePreset(preset);
    state.preset = preset;
    state.presetName.fill('\0');
    std::copy(preset.name.begin(), preset.name.end(), state.presetName.begin());
    state.presetDirty = false;
    state.presetError.clear();
    state.presetStatus = "Preset applied.";
}

namespace {
bool effectSlider(const char* id, float& value, float low, float high, const char* label) {
    ImGui::SetNextItemWidth(-1);
    return ImGui::SliderFloat(id,&value,low,high,label,ImGuiSliderFlags_AlwaysClamp);
}
void drawPresetControls(EditorUiState& state) {
    ImGui::SeparatorText("Presets");
    if (ImGui::Button("Soft Glow")) applyVisualPreset(state, softGlowPreset());
    ImGui::SameLine();
    if (ImGui::Button("Beat Pulse")) applyVisualPreset(state, beatPulsePreset());
    if (ImGui::Button("Side Kick")) applyVisualPreset(state, sideKickPreset());
    ImGui::SameLine();
    if (ImGui::Button("Spectrum Pulse")) applyVisualPreset(state, spectrumPulsePreset());
    ImGui::TextWrapped("%s%s", state.preset.name.c_str(), state.presetDirty ? " (modified)" : "");
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::InputTextWithHint("##PresetName", "Preset name", state.presetName.data(), state.presetName.size());
    if (changed) state.preset.name = state.presetName.data();
    ImGui::BeginDisabled(state.presetDialogPending || state.artworkDialogPending || state.audioDialogPending);
    if (ImGui::Button("Load JSON")) state.requestLoadPreset = true;
    ImGui::SameLine();
    if (ImGui::Button("Save JSON")) state.requestSavePreset = true;
    ImGui::EndDisabled();
    auto& preset = state.preset;
    ImGui::SeparatorText("Audio response");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##Pulse", &preset.pulseStrength, 0, 2, "Master response: %.2f", ImGuiSliderFlags_AlwaysClamp);
    int band = static_cast<int>(preset.beatBand);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##BeatBand", &band, "Bass: 35-180 Hz\0Drums: 180-2500 Hz\0Wide: 35-2500 Hz\0")) {
        preset.beatBand = static_cast<BeatBand>(band);
        changed = true;
    }
    float minimumDb = static_cast<float>(beatLevelDb(preset.beatThreshold));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##BeatThreshold", &minimumDb, -60, 0, "Hit gate: %.1f dBFS", ImGuiSliderFlags_AlwaysClamp)) {
        preset.beatThreshold = beatLevelFromDb(minimumDb);
        changed = true;
    }
    ImGui::TextWrapped("One level gate for overlay pulse and Side Kick, measured in the selected band. Raise it to reject quieter attacks.");
    ImGui::SeparatorText("Independent effects");
    const bool hasHalo = state.design.scene == SceneKind::Vinyl || state.design.scene == SceneKind::Cover;
    ImGui::BeginDisabled(!hasHalo);
    changed |= ImGui::Checkbox("Glow", &preset.glowEnabled);
    if (ImGui::CollapsingHeader("Glow settings",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!preset.glowEnabled);
        changed |= ImGui::ColorEdit3("Glow color",preset.glowColor.data(),ImGuiColorEditFlags_NoInputs);
        changed |= effectSlider("##Glow",preset.glowIntensity,0,3,"Intensity: %.2f");
        changed |= effectSlider("##GlowWidth",preset.glowWidth,0.2f,3,"Width: %.2fx");
        changed |= effectSlider("##GlowSoftness",preset.glowSoftness,0.25f,3,"Softness: %.2f");
        changed |= effectSlider("##GlowBase",preset.glowBase,0,1,"Steady glow: %.2f");
        changed |= effectSlider("##GlowReaction",preset.glowReactivity,0,3,"Audio reaction: %.2f");
        changed |= effectSlider("##GlowAttack",preset.glowAttackMs,0,250,"Extra attack: %.0f ms");
        changed |= effectSlider("##GlowRelease",preset.glowReleaseMs,0,1000,"Extra release: %.0f ms");
        if (ImGui::Button("Reset glow")) { resetGlow(preset); changed = true; }
        ImGui::TextWrapped("Steady glow 0 uses audio only. Extra attack/release soften the existing RMS response. Glow color also tints rim waves and the progress accent.");
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    const bool pulseOpen = ImGui::CollapsingHeader("Size pulse");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Independent group sizes. Both use the shared hit gate and master audio response.");
    if (pulseOpen) {
        if (ImGui::BeginTable("##PulseGroups",2)) {
            const auto pulseRow = [&](const char* label,bool& enabled,float& strength,bool available) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(label);
                ImGui::BeginDisabled(!available);
                changed |= ImGui::Checkbox(label,&enabled);
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(!enabled);
                float percent = strength*100;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##Strength",&percent,0,12,"%.1f%%",ImGuiSliderFlags_AlwaysClamp)) {
                    strength = percent/100;
                    changed = true;
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::PopID();
            };
            pulseRow("Foreground",preset.subjectEnabled,preset.subjectPulse,state.design.scene != SceneKind::Titles);
            pulseRow("Labels + time",preset.labelsEnabled,preset.labelsPulse,
                state.design.showTitle || state.design.showArtist || state.design.showProgress || state.design.showTimestamps);
            ImGui::EndTable();
        }
    }
    const bool movingArtwork = state.design.background == BackgroundKind::Artwork && state.design.animateBackground;
    ImGui::BeginDisabled(!movingArtwork);
    changed |= ImGui::Checkbox("Reactive background", &preset.backgroundEnabled);
    ImGui::BeginDisabled(!preset.backgroundEnabled);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##BackgroundReaction", &preset.backgroundReactivity, 0, 1,
        "Background reaction: %.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(state.design.scene != SceneKind::Vinyl);
    changed |= ImGui::Checkbox("Rim waves", &preset.wavesEnabled);
    ImGui::BeginDisabled(!preset.wavesEnabled);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##RingReaction", &preset.ringReactivity, 0, 1,
        "Ring waves: %.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    const bool bars = state.design.scene == SceneKind::Bars;
    ImGui::BeginDisabled(state.design.scene != SceneKind::Vinyl);
    changed |= ImGui::Checkbox("Spectrum outline", &preset.spectrumEnabled);
    ImGui::EndDisabled();
    if (bars) ImGui::TextWrapped("This composition uses spectrum bars. Color, brightness, width, gain and smoothing below control them.");
    if (ImGui::CollapsingHeader("Spectrum settings",ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginDisabled(!bars && (state.design.scene != SceneKind::Vinyl || !preset.spectrumEnabled));
    changed |= ImGui::ColorEdit3("Spectrum color",preset.spectrumColor.data(),ImGuiColorEditFlags_NoInputs);
    changed |= effectSlider("##SpectrumIntensity",preset.spectrumIntensity,0,3,"Line brightness: %.2f");
    changed |= effectSlider("##SpectrumWidth",preset.spectrumLineWidth,0.25f,3,"Line width: %.2fx");
    ImGui::BeginDisabled(bars);
    changed |= effectSlider("##SpectrumGap",preset.spectrumGap,0.015f,0.25f,"Gap: %.3f radii");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderInt("##SpectrumPoints",&preset.spectrumPoints,32,256,"Points: %d",ImGuiSliderFlags_AlwaysClamp);
    changed |= effectSlider("##SpectrumSharpness",preset.spectrumSharpness,0,1,"Sharpness: %.2f");
    changed |= effectSlider("##SpectrumRandomness",preset.spectrumRandomness,0,1,"Randomness: %.2f");
    changed |= effectSlider("##SpectrumSpeed",preset.spectrumSpeed,0,4,"Motion speed: %.2fx");
    ImGui::EndDisabled();
    changed |= effectSlider("##SpectrumAttack",preset.spectrumAttackMs,0,250,"Extra attack: %.0f ms");
    changed |= effectSlider("##SpectrumRelease",preset.spectrumReleaseMs,0,1000,"Extra release: %.0f ms");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##Spectrum", &preset.spectrumStrength, 0, 4,
        "Spectrum ring: %.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##SpectrumGain", &preset.spectrumGainDb, -24, 24,
        "Spectrum gain: %+.0f dB", ImGuiSliderFlags_AlwaysClamp);
    float balancePercent = preset.spectrumBalance * 100;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##SpectrumBalance", &balancePercent, -100, 100,
        "Spectrum balance: %+.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
        preset.spectrumBalance = balancePercent / 100;
        changed = true;
    }
    if (ImGui::Button("Reset spectrum tuning")) {
        preset.spectrumGainDb = preset.spectrumBalance = 0;
        changed = true;
    }
    if (ImGui::Button("Reset spectrum")) { resetSpectrum(preset); changed = true; }
    ImGui::TextWrapped("Balance: bass (-), treble (+). Spectrum shows all FFT bands independently of the hit gate.");
    ImGui::TextWrapped("Sharpness 0 rounds the points; 1 uses straight edges. Randomness 0 removes procedural motion. Extra attack/release smooth the cached FFT. Gap and deformation are limited by layout clearance.");
    ImGui::EndDisabled();
    }
    changed |= ImGui::Checkbox("Alternating screen kick", &preset.cameraEnabled);
    ImGui::BeginDisabled(!preset.cameraEnabled);
    float kickPercent = preset.cameraStrength * 100;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##CameraStrength", &kickPercent, 0, 5, "Screen kick: %.1f%%", ImGuiSliderFlags_AlwaysClamp)) {
        preset.cameraStrength = kickPercent / 100;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Audio response amplifies effects after hit selection; it does not change the gate.");
    ImGui::SeparatorText("Motion and lighting");
    ImGui::SetNextItemWidth(-1);
    const double minRpm = -45, maxRpm = 45, minSpeed = 0, maxSpeed = 3;
    ImGui::BeginDisabled(state.design.scene != SceneKind::Vinyl);
    changed |= ImGui::SliderScalar("##Rotation", ImGuiDataType_Double, &preset.discRpm,
        &minRpm, &maxRpm, "Rotation: %.1f RPM", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!movingArtwork);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderScalar("##BackgroundSpeed", ImGuiDataType_Double, &preset.backgroundSpeed,
        &minSpeed, &maxSpeed, "Background speed: %.2fx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##BackgroundZoom", &preset.backgroundZoom, 1.1f, 1.5f, "Background zoom: %.2fx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderFloat("##BackgroundBrightness", &preset.backgroundBrightness, 0, 1.5f,
        "Background light: %.2f", ImGuiSliderFlags_AlwaysClamp);
    if (changed) {
        state.presetDirty = true;
        state.presetStatus.clear();
    }
    if (!state.presetStatus.empty()) ImGui::TextWrapped("%s", state.presetStatus.c_str());
    if (!state.presetError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.55f, 0.45f, 1));
        ImGui::TextWrapped("%s", state.presetError.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
}

void drawDesignControls(EditorUiState& state) {
    auto& design = state.design;
    ImGui::SeparatorText("Composition style");
    int scene = static_cast<int>(design.scene);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##SceneKind",&scene,"Vinyl\0Artwork card\0Spectrum bars\0Background & titles\0"))
        design.scene = static_cast<SceneKind>(scene);
    if (design.scene == SceneKind::Cover) ImGui::TextWrapped("A stationary artwork card. Crop it with Foreground artwork framing in Media.");
    if (design.scene == SceneKind::Bars) {
        ImGui::BeginDisabled(state.foregroundPlacementLocked);
        effectSlider("##BarsHeight",design.barsHeight,0.25f,1.5f,"Bars height: %.2fx");
        ImGui::EndDisabled();
        ImGui::TextWrapped("32 audio bands. Adjust color, width, gain and smoothing in Effects > Spectrum settings.");
    }
    if (ImGui::CollapsingHeader("Visible elements",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Track title",&design.showTitle);
        ImGui::Checkbox("Artist",&design.showArtist);
        ImGui::Checkbox("Progress line",&design.showProgress);
        ImGui::Checkbox("Time labels",&design.showTimestamps);
    }
    if (ImGui::CollapsingHeader("Element placement")) {
        ImGui::BeginDisabled(design.scene == SceneKind::Titles || state.foregroundPlacementLocked);
        effectSlider("##ForegroundScale",design.foregroundScale,0.5f,1.5f,"Foreground size: %.2fx");
        effectSlider("##ForegroundX",design.foregroundX,-0.3f,0.3f,"Foreground X: %+.2f");
        effectSlider("##ForegroundY",design.foregroundY,-0.3f,0.3f,"Foreground Y: %+.2f");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.labelsPlacementLocked);
        effectSlider("##LabelsScale",design.labelsScale,0.5f,1.5f,"Labels + timeline size: %.2fx");
        effectSlider("##LabelsX",design.labelsX,-0.3f,0.3f,"Labels X: %+.2f");
        effectSlider("##LabelsY",design.labelsY,-0.3f,0.3f,"Labels Y: %+.2f");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.foregroundPlacementLocked && state.labelsPlacementLocked);
        if (ImGui::Button("Reset placement")) {
            if (!state.foregroundPlacementLocked) {
                design.foregroundScale = 1;
                design.foregroundX = design.foregroundY = 0;
            }
            if (!state.labelsPlacementLocked) {
                design.labelsScale = 1;
                design.labelsX = design.labelsY = 0;
            }
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Offsets are fractions of the canvas. Placement stays inside the frame; adjust overlapping elements as needed.");
    }
    if (ImGui::CollapsingHeader("Background design")) {
        int background = static_cast<int>(design.background);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##BackgroundKind",&background,"Artwork\0Gradient\0Solid color\0"))
            design.background = static_cast<BackgroundKind>(background);
        if (design.background != BackgroundKind::Artwork) {
            ImGui::ColorEdit3(design.background == BackgroundKind::Solid ? "Background color" : "Top color",
                design.backgroundTop.data(),ImGuiColorEditFlags_NoInputs);
            if (design.background == BackgroundKind::Gradient)
                ImGui::ColorEdit3("Bottom color",design.backgroundBottom.data(),ImGuiColorEditFlags_NoInputs);
        }
        ImGui::BeginDisabled(design.background != BackgroundKind::Artwork);
        ImGui::Checkbox("Animate background artwork",&design.animateBackground);
        ImGui::EndDisabled();
        if (!needsArtwork(design)) ImGui::TextWrapped("This design can export without artwork; audio is still required.");
    }
}

void drawFramingControls(EditorUiState& state) {
    if (!ImGui::CollapsingHeader("Artwork framing")) return;
    ImGui::BeginDisabled(state.artworkPath.empty());
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##FramingTarget", &state.framingTarget, "Background\0" "Foreground artwork\0");
    auto& framing = state.framingTarget == 0 ? state.backgroundFraming : state.labelFraming;
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##ArtworkZoom", &framing.zoom, 1, 3, "Crop zoom: %.2fx", ImGuiSliderFlags_AlwaysClamp);
    float focusX = framing.focusX * 100, focusY = framing.focusY * 100;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##FocusX", &focusX, 0, 100, "Horizontal focus: %.0f%%", ImGuiSliderFlags_AlwaysClamp))
        framing.focusX = focusX / 100;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##FocusY", &focusY, 0, 100, "Vertical focus: %.0f%%", ImGuiSliderFlags_AlwaysClamp))
        framing.focusY = focusY / 100;
    if (state.framingTarget == 0) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##BackgroundSoftness", &state.backgroundSoftness, 0, 1, "Background softness: %.2f", ImGuiSliderFlags_AlwaysClamp);
    }
    if (ImGui::Button("Reset this crop")) framing = {};
    ImGui::TextWrapped("Focus: 0 is left/top, 100 is right/bottom. Zoom in for more movement. Crops are saved with the project.");
    ImGui::EndDisabled();
}

void trackSeekInteraction(EditorUiState& state) {
    if (ImGui::IsItemActivated()) {
        state.beginAudioSeek = true;
        state.audioSeeking = true;
    }
    if (ImGui::IsItemDeactivated()) {
        state.endAudioSeek = true;
        state.audioSeeking = false;
    }
}

void drawWaveform(EditorUiState& state) {
    if (!state.waveform || state.waveform->peaks.empty() || state.durationSeconds <= 0) return;
    const auto origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(std::max(1.0f, ImGui::GetContentRegionAvail().x), ImGui::GetTextLineHeight() * 3.5f);
    ImGui::InvisibleButton("##WaveformSeek", size);
    trackSeekInteraction(state);
    const bool hovered = ImGui::IsItemHovered();
    const double mouseFraction = std::clamp(static_cast<double>((ImGui::GetIO().MousePos.x - origin.x) / size.x), 0.0, 1.0);
    if (ImGui::IsItemActive() || ImGui::IsItemDeactivated()) {
        state.timeSeconds = mouseFraction * state.durationSeconds;
        state.seekAudio = true;
    }
    if (!ImGui::IsItemVisible()) return;
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 bottom(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, bottom, ImGui::GetColorU32(ImGuiCol_FrameBg), 4);
    const float center = origin.y + size.y / 2;
    const float amplitude = size.y * 0.44f;
    const float playhead = origin.x + static_cast<float>(std::clamp(state.timeSeconds / state.durationSeconds, 0.0, 1.0)) * size.x;
    draw->AddLine(ImVec2(origin.x, center), ImVec2(bottom.x, center), ImGui::GetColorU32(ImGuiCol_Border));
    const auto& peaks = state.waveform->peaks;
    const auto columns = static_cast<std::size_t>(std::clamp(size.x, 1.0f, 4096.0f));
    for (std::size_t column = 0; column < columns; ++column) {
        const auto first = column * peaks.size() / columns;
        const auto end = std::min(peaks.size(), ((column + 1) * peaks.size() + columns - 1) / columns);
        auto peak = peaks[first];
        for (auto i = first + 1; i < end; ++i) {
            peak.minimum = std::min(peak.minimum, peaks[i].minimum);
            peak.maximum = std::max(peak.maximum, peaks[i].maximum);
        }
        const float x = origin.x + (column + 0.5f) * size.x / columns;
        draw->AddLine(ImVec2(x, center - peak.maximum * amplitude), ImVec2(x, center - peak.minimum * amplitude),
            ImGui::GetColorU32(x <= playhead ? ImGuiCol_PlotHistogram : ImGuiCol_TextDisabled));
    }
    if (state.rhythm) {
        int previousPixel = -1;
        for (const auto& beat : state.rhythm->events(state.preset.beatBand)) {
            const int pixel = static_cast<int>(beat.timeSeconds / state.durationSeconds * size.x);
            if (pixel == previousPixel) continue;
            previousPixel = pixel;
            const float x = origin.x + pixel;
            draw->AddLine(ImVec2(x, origin.y + 1), ImVec2(x, origin.y + 7),
                ImGui::GetColorU32(beat.strength >= state.preset.beatThreshold ? ImGuiCol_SliderGrabActive : ImGuiCol_Border), 2);
        }
    }
    draw->AddLine(ImVec2(playhead, origin.y), ImVec2(playhead, bottom.y), ImGui::GetColorU32(ImGuiCol_Text), 2);
    draw->AddRect(origin, bottom, ImGui::GetColorU32(hovered ? ImGuiCol_SliderGrabActive : ImGuiCol_Border), 4);
    if (hovered || ImGui::IsItemActive())
        ImGui::SetTooltip("Seek: %.2f s / %.2f s\nClick or drag to seek", mouseFraction * state.durationSeconds, state.durationSeconds);
}

void drawAudioPreviewControls(EditorUiState& state) {
    ImGui::SeparatorText("Preview audio");
    if (!state.previewError.empty()) {
        ImGui::TextWrapped("%s", state.previewError.c_str());
        if (ImGui::Button("Retry audio output")) state.requestRetryPreview = true;
    }
    ImGui::BeginDisabled(!state.audioReady);
    if (ImGui::Button(state.audioPlaying ? "Pause" : "Play")) state.requestPlayPause = true;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) state.requestRestart = true;
    ImGui::EndDisabled();
    const auto position = formatTime(static_cast<std::uint64_t>(state.timeSeconds));
    const auto duration = formatTime(static_cast<std::uint64_t>(state.durationSeconds));
    ImGui::Text("%s / %s", position.c_str(), duration.c_str());
    ImGui::BeginDisabled(state.audioImportPending);
    drawWaveform(state);
    const double start = 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderScalar("##AudioTime", ImGuiDataType_Double, &state.timeSeconds,
            &start, &state.durationSeconds, "%.2f s", ImGuiSliderFlags_AlwaysClamp)) state.seekAudio = true;
    trackSeekInteraction(state);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!state.audioReady);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##Volume", &state.previewVolume, 0, 100, "Volume: %.0f%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Mute", &state.muteAudio);
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &state.loopAudio);
    ImGui::EndDisabled();
    ImGui::ProgressBar(state.audioEnergy, ImVec2(-1, 0), "Audio energy");
    if (state.rhythm) {
        ImGui::ProgressBar(state.rhythm->bass.at(state.timeSeconds), ImVec2(-1, 0), "Bass energy");
        ImGui::ProgressBar(state.rhythm->pulseAt(state.timeSeconds, state.preset.beatThreshold,state.preset.beatBand), ImVec2(-1, 0), "Hit pulse");
        ImGui::Text("Detected attacks: %zu", state.rhythm->events(state.preset.beatBand).size());
    }
    ImGui::TextDisabled("Volume affects monitoring only.");
}

void drawControls(EditorUiState& state) {
    if (state.collectingProject) {
        ImGui::TextUnformatted("Collecting project and media...");
        if (ImGui::Button("Cancel collection")) state.requestCancelCollection = true;
    }
    ImGui::BeginDisabled(state.exportBusy || state.projectDialogPending);
    if (state.workspacePanel == 0) {
        ImGui::SeparatorText("Project");
        ImGui::TextWrapped("%s%s", state.projectPath.empty() ? "Untitled project" : state.projectPath.c_str(), state.projectDirty ? " *" : "");
        ImGui::BeginDisabled(state.audioImportPending || !state.toolsChecked || state.artworkDialogPending
            || state.audioDialogPending || state.presetDialogPending || state.showUnsavedPrompt || state.showRecoveryPrompt);
        if (ImGui::Button("New")) state.requestNewProject = true;
        ImGui::SameLine();
        if (ImGui::Button("Open")) state.requestOpenProject = true;
        ImGui::SameLine();
        if (ImGui::Button("Save")) state.requestSaveProject = true;
        ImGui::SameLine();
        if (ImGui::Button("Save As")) state.requestSaveProjectAs = true;
        if (ImGui::Button("Collect project...")) state.requestCollectProject = true;
        ImGui::TextWrapped("Copy this composition and its media into a new folder.");
        if (ImGui::Button("Review recovery copies")) state.requestRecoveryList = true;
        ImGui::EndDisabled();
        if (!state.collectionStatus.empty()) ImGui::TextWrapped("%s", state.collectionStatus.c_str());
        if (!state.recoveryStatus.empty()) ImGui::TextWrapped("%s", state.recoveryStatus.c_str());
        if (!state.projectStatus.empty()) ImGui::TextWrapped("%s", state.projectStatus.c_str());
        if (!state.projectError.empty()) ImGui::TextWrapped("%s", state.projectError.c_str());
        if (!state.missingArtworkPath.empty()) {
            ImGui::TextWrapped("Artwork unavailable: %s", state.missingArtworkPath.c_str());
            if (ImGui::Button("Locate artwork")) state.requestArtwork = true;
        }
        if (!state.missingAudioPath.empty()) {
            ImGui::TextWrapped("Audio unavailable: %s", state.missingAudioPath.c_str());
            ImGui::BeginDisabled(state.audioImportPending || !state.ffmpegReady || !state.ffprobeReady);
            if (ImGui::Button("Locate audio")) state.requestAudio = true;
            ImGui::EndDisabled();
        }
        ImGui::Spacing();
    }
    if (state.workspacePanel == 1) {
        drawDesignControls(state);
        ImGui::SeparatorText("Output format");
        int selectedFormat = static_cast<int>(state.format);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##OutputFormat", &selectedFormat, "9:16 portrait\0" "16:9 landscape\0" "1:1 square\0" "3:4 portrait\0" "4:3 landscape\0"))
            state.format = static_cast<CompositionFormat>(selectedFormat);
        int resolution = state.shortEdge == 720 ? 0 : 1;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##OutputResolution", &resolution, "720 px short edge\0" "1080 px short edge\0")) state.shortEdge = resolution == 0 ? 720 : 1080;
        const auto dimensions = outputSize(state.format, state.shortEdge);
        ImGui::TextDisabled("%d x %d  /  %s", dimensions.width, dimensions.height, formatName(state.format));
        ImGui::TextDisabled("Target: 30 FPS");
        ImGui::Spacing();
    }
    if (state.workspacePanel == 3) drawPresetControls(state);

    if (state.workspacePanel == 2) {
        ImGui::SeparatorText("Media");
        ImGui::BeginDisabled(state.artworkDialogPending || state.audioDialogPending || state.presetDialogPending);
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
        drawFramingControls(state);
        ImGui::TextWrapped("%s", state.mediaToolsStatus.c_str());
        if (state.toolsChecked && (!state.ffmpegReady || !state.ffprobeReady)) {
            ImGui::TextWrapped("Audio import and MP4 export need FFmpeg and ffprobe. Download a Windows build, extract it, and copy ffmpeg.exe and ffprobe.exe next to MusDesk.exe.");
            if (ImGui::Button("FFmpeg downloads",ImVec2(-1,0))) state.requestMediaToolsPage = true;
            ImGui::BeginDisabled(state.audioImportPending || state.audioDialogPending);
            if (ImGui::Button("Check tools again",ImVec2(-1,0))) state.requestCheckTools = true;
            ImGui::EndDisabled();
            ImGui::TextWrapped("Download: ffmpeg.org/download.html. CPU export needs libx264 and AAC. You can also use MUSDESK_FFMPEG_DIR or PATH.");
        }
        ImGui::BeginDisabled(!state.ffprobeReady || !state.ffmpegReady || state.audioDialogPending || state.artworkDialogPending || state.presetDialogPending || state.audioImportPending);
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
        if (!state.audioPath.empty()) drawAudioPreviewControls(state);
    }
    if (state.workspacePanel == 1) {
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
    }

    ImGui::EndDisabled();
    if (state.workspacePanel == 4 || state.exportActive) {
        ImGui::SeparatorText("Export");
        ImGui::BeginDisabled(state.exportBusy);
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##ExportEncoder", &state.exportEncoder, "Automatic (GPU / CPU)\0CPU (veryfast)\0");
        if (ImGui::Checkbox("Export a selected range",&state.exportSelection) && state.exportEnd <= state.exportStart) {
            state.exportStart = 0;
            state.exportEnd = state.durationSeconds;
        }
        if (state.exportSelection) {
            ImGui::TextUnformatted("Start (seconds)");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputDouble("##ExportStart",&state.exportStart,0.1,1,"%.3f");
            ImGui::TextUnformatted("End (seconds)");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputDouble("##ExportEnd",&state.exportEnd,0.1,1,"%.3f");
            if (ImGui::Button("Start at playhead")) state.exportStart = state.timeSeconds;
            if (ImGui::Button("End at playhead")) state.exportEnd = state.timeSeconds;
            if (ImGui::Button("5 s from playhead")) {
                state.exportStart = std::clamp(state.timeSeconds,0.0,state.durationSeconds);
                if (state.exportStart >= state.durationSeconds) state.exportStart = std::max(0.0,state.durationSeconds-5);
                state.exportEnd = std::min(state.exportStart+5,state.durationSeconds);
            }
        }
        ImGui::EndDisabled();
        const bool rangeValid = !state.exportSelection || (std::isfinite(state.exportStart) && std::isfinite(state.exportEnd)
            && state.exportStart >= 0 && state.exportEnd <= state.durationSeconds
            && state.exportEnd-state.exportStart >= 1.0/48000);
        if (!rangeValid) ImGui::TextWrapped("Choose 0 <= start < end <= audio duration.");
        if (rangeValid && state.exportSelection) ImGui::Text("Selected: %.3f s",state.exportEnd-state.exportStart);
        const bool ready = state.exportBlockedReason.empty();
        ImGui::BeginDisabled(!ready || !rangeValid || state.exportBusy || state.audioImportPending || state.artworkDialogPending
            || state.audioDialogPending || state.presetDialogPending || state.projectDialogPending);
        if (ImGui::Button("Export MP4", ImVec2(-1, 0))) state.requestExport = true;
        ImGui::EndDisabled();
        if (!ready) ImGui::TextWrapped("%s", state.exportBlockedReason.c_str());
        if (state.exportDialogPending) ImGui::TextUnformatted("Choosing export destination...");
        if (state.exportActive) {
            ImGui::ProgressBar(state.exportProgress, ImVec2(-1, 0));
            if (ImGui::Button("Cancel export", ImVec2(-1, 0))) state.requestCancelExport = true;
        }
        if (state.exportError) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.55f, 0.45f, 1));
        if (!state.exportStatus.empty()) ImGui::TextWrapped("%s", state.exportStatus.c_str());
        if (state.exportError) ImGui::PopStyleColor();
        const auto exportSize = outputSize(state.format, state.shortEdge);
        ImGui::TextWrapped("H.264 / AAC, %d x %d, 30 FPS. Original audio; preview volume and loop are ignored. Range selection is session-only. Visuals retain original track time.", exportSize.width, exportSize.height);
    }
}

void drawPreviewAudioOverlay(EditorUiState& state, ImVec2 corner, ImVec2 available) {
    const bool wasOpen = state.audioOverlayOpen;
    const bool disabled = state.workspacePanel == 2 || state.historyBlocked || state.previewDragging;
    if (disabled) state.audioOverlayOpen = false;
    const float em = ImGui::GetFontSize();
    const float width = std::min(available.x,state.audioOverlayOpen ? 22*em : 8*em);
    const float height = state.audioOverlayOpen ? 0 : ImGui::GetFrameHeight()+12;
    ImGui::SetNextWindowPos(corner,ImGuiCond_Always,ImVec2(1,0));
    ImGui::SetNextWindowSize(ImVec2(width,height));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width,0),ImVec2(width,available.y));
    ImGui::SetNextWindowBgAlpha(disabled ? 0.65f : 0.98f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(10,6));
    auto background = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
    if (disabled) { background.x *= 0.65f; background.y *= 0.65f; background.z *= 0.65f; }
    ImGui::PushStyleColor(ImGuiCol_WindowBg,background);
    const bool visible = ImGui::Begin("##AudioPreviewOverlay",nullptr,ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus
        | (state.audioOverlayOpen ? ImGuiWindowFlags_AlwaysAutoResize : 0));
    if (visible) {
        ImGui::BeginDisabled(disabled);
        if (state.audioOverlayOpen) {
            if (!state.audioName.empty()) ImGui::TextWrapped("%s",state.audioName.c_str());
            if (state.audioPath.empty()) ImGui::TextWrapped("Import audio in Media and preview audio.");
            else drawAudioPreviewControls(state);
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Audio  <");
        }
        ImGui::EndDisabled();
        const bool interacting = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsAnyItemActive();
        state.audioOverlayOpen = !disabled && (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) || interacting);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (wasOpen && !state.audioOverlayOpen && state.audioSeeking) {
        state.audioSeeking = false;
        state.endAudioSeek = true;
    }
}

void drawPreview(EditorUiState& state, std::uint32_t texture) {
    const auto panelStart = ImGui::GetCursorScreenPos();
    const auto panelSize = ImGui::GetContentRegionAvail();
    CompositionSettings settings{state.title.data(),state.artist.data(),state.durationSeconds,state.preset,
        state.format,state.shortEdge,state.backgroundFraming,state.labelFraming,state.backgroundSoftness,state.design};
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", formatName(state.format));
    ImGui::SameLine();
    ImGui::BeginDisabled(state.historyBlocked || state.previewDragging);
    ImGui::Checkbox("Snap",&state.previewSnapping);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Align the composition to center axes and 10%% margins before pulse/camera effects. Alt bypasses snapping.");
    ImGui::SameLine();
    ImGui::Checkbox("Guides",&state.showAlignmentGuides);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(state.historyBlocked || state.previewDragging);
    ImGui::Checkbox("Lock foreground",&state.foregroundPlacementLocked);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lock position and size for this session. Effects and Undo/Redo remain available.");
    ImGui::SameLine();
    ImGui::Checkbox("Lock labels",&state.labelsPlacementLocked);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lock position and size for this session. Effects and Undo/Redo remain available.");
    ImGui::EndDisabled();
    ImGui::TextDisabled("Drag: move. Corners: resize. Arrows: 1px / Shift: 10px.");
    ImGui::BeginDisabled(state.historyBlocked || state.previewDragging || state.placementLocked(state.previewSelection)
        || !elementBounds(settings,state.previewSelection));
    const auto apply = [&](CompositionDesign design) {
        if (state.previewSelection == CompositionElement::Foreground) {
            state.design.foregroundX = design.foregroundX;
            state.design.foregroundY = design.foregroundY;
            state.design.foregroundScale = design.foregroundScale;
        } else if (state.previewSelection == CompositionElement::Labels) {
            state.design.labelsX = design.labelsX;
            state.design.labelsY = design.labelsY;
            state.design.labelsScale = design.labelsScale;
        }
        settings.design = state.design;
    };
    if (ImGui::Button("Center X")) apply(editCompositionElement(settings,state.previewSelection,CompositionEdit::CenterHorizontal));
    ImGui::SameLine();
    if (ImGui::Button("Center Y")) apply(editCompositionElement(settings,state.previewSelection,CompositionEdit::CenterVertical));
    ImGui::SameLine();
    if (ImGui::Button("Reset position")) apply(editCompositionElement(settings,state.previewSelection,CompositionEdit::ResetPosition));
    ImGui::SameLine();
    if (ImGui::Button("Reset size")) apply(editCompositionElement(settings,state.previewSelection,CompositionEdit::ResetSize));
    ImGui::EndDisabled();
    ImGui::Separator();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float padding = ImGui::GetFontSize();
    const float usableWidth = available.x - 2.0f * padding;
    const float usableHeight = available.y - 2.0f * padding;
    if (usableWidth <= 0.0f || usableHeight <= 0.0f) {
        return;
    }

    const auto previewSize = outputSize(state.format, state.shortEdge);
    const float aspectRatio = static_cast<float>(previewSize.width) / previewSize.height;
    const float width = std::min(usableWidth, usableHeight * aspectRatio)
        * (state.previewScalePercent / 100.0f);
    const float height = width / aspectRatio;
    const auto framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
    state.previewDisplayedShortEdge = static_cast<int>(std::ceil(std::min(
        width*framebufferScale.x,height*framebufferScale.y)));
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

    const auto layout = layoutFor(settings);
    const auto mouse = ImGui::GetMousePos();
    const CanvasPoint normalized{(mouse.x-topLeft.x)/width,(mouse.y-topLeft.y)/height};
    const auto hovered = hitComposition(settings,state.previewFrame,normalized,
        5*layout.canvas.width/width/static_cast<float>(state.previewFrame.cameraZoom));
    const float handleSize = ImGui::GetFontSize()*0.28f;
    const auto handleAt = [&](ImVec2 position) {
        if (state.placementLocked(state.previewSelection)) return ResizeCorner::None;
        if (const auto bounds = elementBounds(settings,state.previewSelection)) {
            const auto corners = boundsCorners(*bounds);
            for (int i=0; i<4; ++i) {
                const auto point = canvasToPreview(corners[i],layout,state.previewFrame,state.previewSelection);
                if (std::abs(position.x-(topLeft.x+point.x*width)) <= handleSize*2
                    && std::abs(position.y-(topLeft.y+point.y*height)) <= handleSize*2)
                    return static_cast<ResizeCorner>(i);
            }
        }
        return ResizeCorner::None;
    };
    const auto hoveredHandle = handleAt(mouse);
    state.previewGuideX.reset();
    state.previewGuideY.reset();
    ImGui::SetCursorScreenPos(topLeft);
    ImGui::InvisibleButton("##MoveComposition",ImVec2(width,height));
    if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())
        || ImGui::IsKeyPressed(ImGuiKey_Tab,false) || !ImGui::IsWindowFocused()) state.previewKeyboardFocus = false;
    if (!state.historyBlocked && (ImGui::IsItemActivated() || ImGui::IsItemClicked(ImGuiMouseButton_Left))) {
        const auto clicked = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
        const CanvasPoint start{(clicked.x-topLeft.x)/width,(clicked.y-topLeft.y)/height};
        state.previewResizeCorner = handleAt(clicked);
        if (state.previewResizeCorner == ResizeCorner::None)
            state.previewSelection = hitComposition(settings,state.previewFrame,start);
        state.previewKeyboardFocus = state.previewSelection != CompositionElement::None;
        state.previewDragging = state.previewKeyboardFocus && !state.placementLocked(state.previewSelection);
        state.previewDragMoved = false;
        if (state.previewDragging) {
            state.previewDragSettings = settings;
            state.previewDragFrame = state.previewFrame;
            state.previewDragStart = previewToCanvas(start,layout,state.previewDragFrame);
        }
    }
    if (state.historyBlocked || state.placementLocked(state.previewSelection)) state.previewDragging = false;
    if (state.previewDragging) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
            apply(state.previewDragSettings.design);
            state.previewDragging = false;
        } else {
            const auto clicked = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
            state.previewDragMoved |= std::hypot(mouse.x-clicked.x,mouse.y-clicked.y) >= ImGui::GetIO().MouseDragThreshold;
            if (state.previewDragMoved) {
                const auto startLayout = layoutFor(state.previewDragSettings);
                const auto current = previewToCanvas(normalized,startLayout,state.previewDragFrame);
                const bool resizing = state.previewResizeCorner != ResizeCorner::None;
                const float pulseScale = static_cast<float>(state.previewSelection == CompositionElement::Labels
                    ? state.previewDragFrame.labelsScale : state.previewDragFrame.subjectScale);
                const float scale = resizing ? pulseScale : 1;
                const CanvasPoint delta{(current.x-state.previewDragStart.x)/scale,(current.y-state.previewDragStart.y)/scale};
                if (resizing) apply(resizeCompositionElement(state.previewDragSettings,state.previewSelection,
                    state.previewResizeCorner,delta));
                else {
                    apply(moveCompositionElement(state.previewDragSettings,state.previewSelection,delta));
                    if (state.previewSnapping && !ImGui::GetIO().KeyAlt) {
                        auto moved = state.previewDragSettings;
                        moved.design = state.design;
                        const float zoom = static_cast<float>(state.previewDragFrame.cameraZoom)*pulseScale;
                        const auto aligned = snapCompositionElement(moved,state.previewSelection,
                            {7*layout.canvas.width/width/zoom,7*layout.canvas.height/height/zoom});
                        apply(aligned.design);
                        state.previewGuideX = aligned.guideX;
                        state.previewGuideY = aligned.guideY;
                    }
                }
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !ImGui::IsItemActive()) state.previewDragging = false;
        }
    }
    if (!state.historyBlocked && !state.previewDragging && !state.audioOverlayOpen && state.previewKeyboardFocus
        && state.previewSelection != CompositionElement::None && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput) {
        const int dx = int(ImGui::IsKeyPressed(ImGuiKey_RightArrow))-int(ImGui::IsKeyPressed(ImGuiKey_LeftArrow));
        const int dy = int(ImGui::IsKeyPressed(ImGuiKey_DownArrow))-int(ImGui::IsKeyPressed(ImGuiKey_UpArrow));
        const bool holdingArrow = ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_RightArrow)
            || ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow);
        state.previewArrowKeysCaptured = holdingArrow || dx != 0 || dy != 0;
        if (!state.placementLocked(state.previewSelection)
            && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeySuper) {
            state.previewKeyboardEditing = holdingArrow;
            const float step = ImGui::GetIO().KeyShift ? 10.0f : 1.0f;
            const CanvasPoint pixels{step*dx,step*dy};
            settings.design = state.design;
            if (pixels.x != 0 || pixels.y != 0) apply(nudgeCompositionElement(settings,state.previewSelection,pixels));
        }
    }
    if (!state.historyBlocked && (state.previewDragging || ImGui::IsItemHovered())) {
        const auto corner = state.previewDragging ? state.previewResizeCorner : hoveredHandle;
        if (corner != ResizeCorner::None) ImGui::SetMouseCursor(corner == ResizeCorner::TopLeft || corner == ResizeCorner::BottomRight
            ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW);
        else if (state.previewDragging || (hovered != CompositionElement::None && !state.placementLocked(hovered)))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    settings.design = state.design;
    if (!state.historyBlocked) {
        if (const auto bounds = elementBounds(settings,state.previewSelection)) {
            const auto updatedLayout = layoutFor(settings);
            const auto a = canvasToPreview({bounds->left,bounds->top},updatedLayout,state.previewFrame,state.previewSelection);
            const auto b = canvasToPreview({bounds->right,bounds->bottom},updatedLayout,state.previewFrame,state.previewSelection);
            drawList->PushClipRect(topLeft,bottomRight,true);
            if (state.showAlignmentGuides) {
                const auto line = [&](CanvasPoint from,CanvasPoint to,bool active) {
                    const auto p = canvasToPreview(from,updatedLayout,state.previewFrame,state.previewSelection);
                    const auto q = canvasToPreview(to,updatedLayout,state.previewFrame,state.previewSelection);
                    drawList->AddLine(ImVec2(topLeft.x+p.x*width,topLeft.y+p.y*height),
                        ImVec2(topLeft.x+q.x*width,topLeft.y+q.y*height),
                        active ? IM_COL32(255,208,100,240) : IM_COL32(100,210,190,70),active ? 1.8f : 1.0f);
                };
                for (float fraction : {0.1f,0.5f,0.9f}) {
                    const float x = fraction*updatedLayout.canvas.width, y = fraction*updatedLayout.canvas.height;
                    line({x,0},{x,float(updatedLayout.canvas.height)},state.previewGuideX && std::abs(*state.previewGuideX-x)<0.01f);
                    line({0,y},{float(updatedLayout.canvas.width),y},state.previewGuideY && std::abs(*state.previewGuideY-y)<0.01f);
                }
            }
            drawList->AddRect(ImVec2(topLeft.x+a.x*width,topLeft.y+a.y*height),
                ImVec2(topLeft.x+b.x*width,topLeft.y+b.y*height),IM_COL32(100,230,205,230),0,0,1.5f);
            if (!state.placementLocked(state.previewSelection)) for (const auto& corner : boundsCorners(*bounds)) {
                const auto p = canvasToPreview(corner,updatedLayout,state.previewFrame,state.previewSelection);
                const ImVec2 center(topLeft.x+p.x*width,topLeft.y+p.y*height);
                drawList->AddRectFilled(ImVec2(center.x-handleSize,center.y-handleSize),
                    ImVec2(center.x+handleSize,center.y+handleSize),IM_COL32(100,230,205,255),1);
            }
            drawList->PopClipRect();
        }
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(available);
    drawPreviewAudioOverlay(state,{panelStart.x+panelSize.x,panelStart.y},panelSize);
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
        ImGui::TextUnformatted("MusDesk " MUSDESK_VERSION);
        ImGui::SameLine();
        ImGui::TextDisabled("Music visualizer");
        ImGui::SameLine();
        ImGui::BeginDisabled(state.historyBlocked || !state.canUndo);
        if (ImGui::Button("Undo")) state.requestUndo = true;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Ctrl+Z: labels, effects, framing and output settings. Media imports start a new history.");
        ImGui::SameLine();
        ImGui::BeginDisabled(state.historyBlocked || !state.canRedo);
        if (ImGui::Button("Redo")) state.requestRedo = true;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+Y or Ctrl+Shift+Z");
        ImGui::Separator();

        const float em = ImGui::GetFontSize();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float controlsWidth = std::clamp(available.x * 0.30f, em * 16.0f, em * 22.0f);
        const float panelHeight = std::max(1.0f, available.y - em * 2.0f);
        if (ImGui::BeginChild("Controls", ImVec2(controlsWidth, panelHeight), ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##WorkspacePanel", &state.workspacePanel,
                "Project\0Composition\0Media and preview audio\0Effects\0Export\0");
            ImGui::PushID(state.workspacePanel);
            if (ImGui::BeginChild("PanelContents")) drawControls(state);
            ImGui::EndChild();
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Canvas", ImVec2(0, panelHeight), ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            drawPreview(state, compositionTexture);
        }
        ImGui::EndChild();
        const auto size = outputSize(state.format, state.shortEdge);
        ImGui::TextDisabled("Composition preview  |  MP4: %d x %d / 30 FPS", size.width, size.height);
    }
    ImGui::End();
    if (state.showRecoveryPrompt) ImGui::OpenPopup("Recover session");
    if (ImGui::BeginPopupModal("Recover session", &state.showRecoveryPrompt, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Recovery copies from inactive sessions were found.");
        for (int i = 0; i < static_cast<int>(state.recoveryCopies.size()); ++i) {
            const auto& copy = state.recoveryCopies[i];
            if (ImGui::Selectable((copy.title + "##recovery" + std::to_string(i)).c_str(), state.selectedRecovery == i))
                state.selectedRecovery = i;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", copy.savedAt.c_str());
        }
        const bool selected = state.selectedRecovery >= 0 && state.selectedRecovery < static_cast<int>(state.recoveryCopies.size());
        if (selected && !state.recoveryCopies[state.selectedRecovery].error.empty())
            ImGui::TextWrapped("%s", state.recoveryCopies[state.selectedRecovery].error.c_str());
        if (!state.recoveryStatus.empty()) ImGui::TextWrapped("%s", state.recoveryStatus.c_str());
        ImGui::BeginDisabled(!selected || (selected && !state.recoveryCopies[state.selectedRecovery].error.empty())
            || !state.toolsChecked || state.audioImportPending || state.exportBusy || state.projectDialogPending);
        if (ImGui::Button("Recover")) {
            state.requestRecover = true;
            state.showRecoveryPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Keep for later")) {
            state.showRecoveryPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!selected);
        if (ImGui::Button("Delete copy...")) ImGui::OpenPopup("Delete recovery copy?");
        ImGui::EndDisabled();
        if (ImGui::BeginPopupModal("Delete recovery copy?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Delete the selected recovery copy? This cannot be undone.");
            if (ImGui::Button("Delete recovery copy")) {
                state.requestDeleteRecovery = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel deletion")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::EndPopup();
    }
    if (state.showUnsavedPrompt) ImGui::OpenPopup("Unsaved project");
    if (ImGui::BeginPopupModal("Unsaved project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save the project before continuing?");
        ImGui::BeginDisabled(state.exportBusy || state.audioImportPending || state.projectDialogPending);
        if (ImGui::Button("Save changes")) {
            state.requestSaveProject = true;
            state.showUnsavedPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Discard changes")) {
            state.discardProjectChanges = true;
            state.showUnsavedPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.cancelProjectAction = true;
            state.showUnsavedPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
}
