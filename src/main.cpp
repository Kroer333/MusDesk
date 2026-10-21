#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>

int main(int, char*[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL initialization failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow("MusDesk", 1280, 800, 0);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    int exitCode = EXIT_SUCCESS;
    bool running = true;
    while (running) {
        SDL_Event event{};
        // There is no animation yet, so sleep until an event arrives.
        if (!SDL_WaitEvent(&event)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Waiting for events failed: %s", SDL_GetError());
            exitCode = EXIT_FAILURE;
            break;
        }

        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
             event.window.windowID == SDL_GetWindowID(window))) {
            running = false;
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
