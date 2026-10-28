#pragma once

#include <memory>
#include <optional>
#include <string>

struct SDL_Window;

namespace musdesk {
struct ArtworkSelection {
    std::string path;
    std::string error;
};

enum class FileDialogKind { Artwork, Audio, PresetOpen, PresetSave, ExportSave, ProjectOpen, ProjectSave };

class ArtworkDialog {
public:
    ArtworkDialog();
    void open(SDL_Window* window, FileDialogKind kind = FileDialogKind::Artwork);
    bool pending() const;
    std::optional<ArtworkSelection> takeSelection();

private:
    struct State;
    std::shared_ptr<State> state_;
};
}
