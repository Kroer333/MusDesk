#include "ArtworkDialog.hpp"

#include <SDL3/SDL.h>

#include <mutex>

namespace musdesk {
struct ArtworkDialog::State {
    std::mutex mutex;
    bool pending = false;
    std::optional<ArtworkSelection> selection;
};

ArtworkDialog::ArtworkDialog() : state_(std::make_shared<State>()) {}

void ArtworkDialog::open(SDL_Window* window, FileDialogKind kind) {
    {
        std::lock_guard lock(state_->mutex);
        if (state_->pending) {
            return;
        }
        state_->pending = true;
    }
    // SDL can call back on another thread, even after the editor has closed.
    // Keep the queue alive and only upload textures on the main thread.
    auto* callbackState = new std::shared_ptr<State>(state_);
    static constexpr SDL_DialogFileFilter filters[] = {{"PNG / JPEG / WebP artwork", "png;jpg;jpeg;webp"}};
    static constexpr SDL_DialogFileFilter audioFilters[] = {{"MP3 / WAV / OGG audio", "mp3;wav;ogg"}};
    static constexpr SDL_DialogFileFilter jsonFilters[] = {{"MusDesk preset JSON", "json"}};
    static constexpr SDL_DialogFileFilter videoFilters[] = {{"MP4 video", "mp4"}};
    static constexpr SDL_DialogFileFilter projectFilters[] = {{"MusDesk project", "musdesk"}};
    const auto callback = [](void* userdata, const char* const* files, int) {
        std::unique_ptr<std::shared_ptr<State>> owner(static_cast<std::shared_ptr<State>*>(userdata));
        ArtworkSelection result;
        if (files == nullptr) {
            result.error = SDL_GetError();
        } else if (files[0] != nullptr) {
            result.path = files[0];
        }
        std::lock_guard lock((*owner)->mutex);
        (*owner)->selection = std::move(result);
        (*owner)->pending = false;
    };
    if (kind == FileDialogKind::ProjectSave) {
        SDL_ShowSaveFileDialog(callback, callbackState, window, projectFilters, 1, "project.musdesk");
    } else if (kind == FileDialogKind::ExportSave) {
        SDL_ShowSaveFileDialog(callback, callbackState, window, videoFilters, 1, "MusDesk.mp4");
    } else if (kind == FileDialogKind::PresetSave) {
        SDL_ShowSaveFileDialog(callback, callbackState, window, jsonFilters, 1, "preset.json");
    } else {
        const auto* selectedFilters = kind == FileDialogKind::Audio ? audioFilters
            : (kind == FileDialogKind::PresetOpen ? jsonFilters
                : (kind == FileDialogKind::ProjectOpen ? projectFilters : filters));
        SDL_ShowOpenFileDialog(callback, callbackState, window, selectedFilters, 1, nullptr, false);
    }
}

bool ArtworkDialog::pending() const {
    std::lock_guard lock(state_->mutex);
    return state_->pending;
}

std::optional<ArtworkSelection> ArtworkDialog::takeSelection() {
    std::lock_guard lock(state_->mutex);
    auto result = std::move(state_->selection);
    state_->selection.reset();
    return result;
}
}
