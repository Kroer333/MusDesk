#pragma once

#include <array>

namespace musdesk {
struct EditorUiState {
    std::array<float, 3> canvasColor{0.09f, 0.12f, 0.18f};
    float previewScalePercent = 100.0f;
    bool showSafeArea = false;
};

void configureEditorStyle(float displayScale);
void drawEditorUi(EditorUiState& state);
}
