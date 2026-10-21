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

class ArtworkDialog {
public:
    ArtworkDialog();
    void open(SDL_Window* window);
    bool pending() const;
    std::optional<ArtworkSelection> takeSelection();

private:
    struct State;
    std::shared_ptr<State> state_;
};
}
