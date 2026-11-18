#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "musdesk/core/VisualPreset.hpp"
#include "musdesk/core/AudioAnalysis.hpp"
#include "musdesk/core/Composition.hpp"
#include "musdesk/core/CompositionEditing.hpp"
#include "musdesk/projects/RecoveryStore.hpp"

namespace musdesk {
struct EditorUiState {
    int workspacePanel = 2;
    bool canUndo = false;
    bool canRedo = false;
    bool historyBlocked = false;
    bool requestUndo = false;
    bool requestRedo = false;
    int previewDisplayedShortEdge = 720;
    CompositionFrame previewFrame{};
    CompositionElement previewSelection = CompositionElement::None;
    bool previewDragging = false;
    bool previewDragMoved = false;
    ResizeCorner previewResizeCorner = ResizeCorner::None;
    bool previewSnapping = true;
    bool showAlignmentGuides = true;
    std::optional<float> previewGuideX, previewGuideY;
    bool requestPausePreview = false;
    CanvasPoint previewDragStart{};
    CompositionSettings previewDragSettings;
    CompositionFrame previewDragFrame{};
    CompositionFormat format = CompositionFormat::Portrait;
    int shortEdge = 1080;
    CompositionDesign design;
    ImageFraming backgroundFraming;
    ImageFraming labelFraming;
    float backgroundSoftness = 1;
    int framingTarget = 0;
    const AudioWaveform* waveform = nullptr;
    const AudioRhythm* rhythm = nullptr;
    std::vector<RecoveryCopy> recoveryCopies;
    std::string recoveryStatus;
    int selectedRecovery = 0;
    bool showRecoveryPrompt = false;
    bool requestRecoveryList = false;
    bool requestRecover = false;
    bool requestDeleteRecovery = false;
    std::string projectPath;
    std::string projectStatus;
    std::string projectError;
    std::string missingAudioPath;
    std::string missingArtworkPath;
    bool projectDirty = false;
    bool projectDialogPending = false;
    bool collectingProject = false;
    bool requestCollectProject = false;
    bool requestCancelCollection = false;
    std::string collectionStatus;
    bool requestNewProject = false;
    bool requestOpenProject = false;
    bool requestSaveProject = false;
    bool requestSaveProjectAs = false;
    bool showUnsavedPrompt = false;
    bool discardProjectChanges = false;
    bool cancelProjectAction = false;
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
    std::string artworkPath;
    std::string importError;
    bool requestAudio = false;
    bool audioDialogPending = false;
    bool audioImportPending = false;
    bool toolsChecked = false;
    bool requestCheckTools = false;
    bool requestMediaToolsPage = false;
    bool ffprobeReady = false;
    bool ffmpegReady = false;
    std::string mediaToolsStatus = "Checking FFmpeg / ffprobe...";
    std::string audioPath;
    std::string audioName;
    std::string audioError;
    bool audioReady = false;
    std::string previewError;
    bool requestRetryPreview = false;
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
    int exportEncoder = 0;
    bool exportSelection = false;
    double exportStart = 0;
    double exportEnd = 0;
    std::string exportBlockedReason;
};

void configureEditorStyle(float displayScale);
void applyVisualPreset(EditorUiState& state, const VisualPreset& preset);
void drawEditorUi(EditorUiState& state, std::uint32_t compositionTexture);
}
