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

void ArtworkDialog::open(SDL_Window* window) {
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
    SDL_ShowOpenFileDialog([](void* userdata, const char* const* files, int) {
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
    }, callbackState, window, filters, 1, nullptr, false);
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
