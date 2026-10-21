#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace musdesk {
struct EditorUiState {
    float previewScalePercent = 100.0f;
    bool showSafeArea = false;
    std::array<char, 128> title{"Track title"};
    std::array<char, 96> artist{"Artist name"};
    double timeSeconds = 0;
    double durationSeconds = 252;
    float discRpm = 8;
    bool animatePreview = true;
    bool requestArtwork = false;
    bool artworkDialogPending = false;
    std::string artworkName;
    std::string importError;
};

void configureEditorStyle(float displayScale);
void drawEditorUi(EditorUiState& state, std::uint32_t compositionTexture);
}
