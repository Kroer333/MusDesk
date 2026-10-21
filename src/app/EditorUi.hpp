#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "musdesk/core/VisualPreset.hpp"

namespace musdesk {
struct EditorUiState {
    float previewScalePercent = 100.0f;
    bool showSafeArea = false;
    std::array<char, 128> title{"Track title"};
    std::array<char, 96> artist{"Artist name"};
    double timeSeconds = 0;
    double durationSeconds = 252;
    VisualPreset preset;
    std::array<char, 97> presetName{"Soft Glow"};
    bool presetDirty = false;
    bool presetDialogPending = false;
    bool requestLoadPreset = false;
    bool requestSavePreset = false;
    std::string presetStatus;
    std::string presetError;
    bool animatePreview = true;
    bool requestArtwork = false;
    bool artworkDialogPending = false;
    std::string artworkName;
    std::string importError;
    bool requestAudio = false;
    bool audioDialogPending = false;
    bool audioImportPending = false;
    bool toolsChecked = false;
    bool ffprobeReady = false;
    bool ffmpegReady = false;
    std::string mediaToolsStatus = "Checking FFmpeg / ffprobe...";
    std::string audioPath;
    std::string audioName;
    std::string audioError;
    bool audioReady = false;
    bool audioPlaying = false;
    bool audioSeeking = false;
    bool requestPlayPause = false;
    bool requestRestart = false;
    bool requestCancelAudio = false;
    bool beginAudioSeek = false;
    bool seekAudio = false;
    bool endAudioSeek = false;
    bool loopAudio = false;
    bool muteAudio = false;
    float previewVolume = 75;
    float audioEnergy = 0;
    bool exportBusy = false;
    bool exportActive = false;
    bool exportDialogPending = false;
    bool requestExport = false;
    bool requestCancelExport = false;
    float exportProgress = 0;
    std::string exportStatus;
    bool exportError = false;
};

void configureEditorStyle(float displayScale);
void applyVisualPreset(EditorUiState& state, const VisualPreset& preset);
void drawEditorUi(EditorUiState& state, std::uint32_t compositionTexture);
}
