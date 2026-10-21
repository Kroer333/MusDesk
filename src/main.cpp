#include "app/EditorUi.hpp"
#include "app/ArtworkDialog.hpp"
#include "musdesk/render/VisualizerRenderer.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <cstdlib>
#include <cmath>
#include <exception>
#include <string>

namespace {
struct ApplicationResources {
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    bool imguiContextCreated = false;
    bool platformBackendInitialized = false;
    bool rendererBackendInitialized = false;

    ApplicationResources() = default;
    ApplicationResources(const ApplicationResources&) = delete;
    ApplicationResources& operator=(const ApplicationResources&) = delete;

    ~ApplicationResources() {
        // Backends use the GL context and window, so destroy them first.
        if (rendererBackendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
        }
        if (platformBackendInitialized) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (imguiContextCreated) {
            ImGui::DestroyContext();
        }
        if (glContext != nullptr) {
            SDL_GL_DestroyContext(glContext);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
};

int reportSdlFailure(const char* operation) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", operation, SDL_GetError());
    return EXIT_FAILURE;
}
}

int main(int argc, char* argv[]) {
    std::string startupArtwork;
    if (argc == 3 && std::string(argv[1]) == "--artwork") {
        startupArtwork = argv[2];
    } else if (argc != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: MusDesk [--artwork image.png]");
        return EXIT_FAILURE;
    }
    ApplicationResources resources;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return reportSdlFailure("SDL initialization failed");
    }

    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
        !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
        return reportSdlFailure("OpenGL configuration failed");
    }

    resources.window = SDL_CreateWindow("MusDesk", 1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (resources.window == nullptr) {
        return reportSdlFailure("Window creation failed");
    }
    if (!SDL_SetWindowMinimumSize(resources.window, 900, 600)) {
        return reportSdlFailure("Setting minimum window size failed");
    }

    resources.glContext = SDL_GL_CreateContext(resources.window);
    if (resources.glContext == nullptr) {
        return reportSdlFailure("OpenGL context creation failed");
    }
    if (!SDL_GL_MakeCurrent(resources.window, resources.glContext)) {
        return reportSdlFailure("Activating OpenGL context failed");
    }
    const bool vsyncEnabled = SDL_GL_SetSwapInterval(1);
    if (!vsyncEnabled) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "VSync unavailable, using frame delay: %s", SDL_GetError());
    }
    SDL_Log("OpenGL: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    resources.imguiContextCreated = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // The fixed workspace and its settings are session-only for now.
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    const std::string fontPath = std::string(SDL_GetBasePath()) + "assets/Roboto-Medium.ttf";
    SDL_PathInfo fontInfo{};
    if (!SDL_GetPathInfo(fontPath.c_str(), &fontInfo) || fontInfo.type != SDL_PATHTYPE_FILE) {
        return reportSdlFailure("Bundled composition font is missing");
    }
    if (io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f) == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UI font unavailable; using built-in font");
    }
    musdesk::configureEditorStyle(SDL_GetWindowDisplayScale(resources.window));
    if (!ImGui_ImplSDL3_InitForOpenGL(resources.window, resources.glContext)) {
        return reportSdlFailure("ImGui SDL backend initialization failed");
    }
    resources.platformBackendInitialized = true;
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ImGui OpenGL backend initialization failed");
        return EXIT_FAILURE;
    }
    resources.rendererBackendInitialized = true;
    try {
        musdesk::VisualizerRenderer composition(fontPath);
        musdesk::EditorUiState editorState;
        musdesk::ArtworkDialog artworkDialog;
        auto importArtwork = [&](const std::string& path) {
            try {
                const auto image = musdesk::loadArtwork(path);
                composition.setArtwork(image);
                const auto slash = path.find_last_of("/\\");
                editorState.artworkName = path.substr(slash == std::string::npos ? 0 : slash + 1);
                editorState.importError.clear();
                SDL_Log("Artwork imported: %d x %d", image.width, image.height);
            } catch (const std::exception& error) {
                // A failed replacement keeps the previously imported image intact.
                editorState.importError = error.what();
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
            }
        };
        if (!startupArtwork.empty()) importArtwork(startupArtwork);
        double anchorTime = 0;
        Uint64 anchorTicks = SDL_GetTicksNS();

        bool running = true;
        while (running) {
            const Uint64 frameStart = SDL_GetTicks();
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr
                    && event.drop.windowID == SDL_GetWindowID(resources.window)) {
                    importArtwork(event.drop.data);
                }
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                     event.window.windowID == SDL_GetWindowID(resources.window))) {
                    running = false;
                }
                if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED &&
                    event.window.windowID == SDL_GetWindowID(resources.window)) {
                    musdesk::configureEditorStyle(SDL_GetWindowDisplayScale(resources.window));
                }
            }
            if (!running) {
                break;
            }
            if (auto selection = artworkDialog.takeSelection()) {
                if (!selection->error.empty()) editorState.importError = selection->error;
                else if (!selection->path.empty()) importArtwork(selection->path);
            }
            editorState.artworkDialogPending = artworkDialog.pending();
            if ((SDL_GetWindowFlags(resources.window) & SDL_WINDOW_MINIMIZED) != 0) {
                SDL_Delay(50);
                continue;
            }

            int pixelWidth = 0;
            int pixelHeight = 0;
            if (!SDL_GetWindowSizeInPixels(resources.window, &pixelWidth, &pixelHeight)) {
                return reportSdlFailure("Reading framebuffer size failed");
            }
            if (pixelWidth <= 0 || pixelHeight <= 0) {
                SDL_Delay(50);
                continue;
            }
            glViewport(0, 0, pixelWidth, pixelHeight);
            glClearColor(0.055f, 0.07f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            const Uint64 now = SDL_GetTicksNS();
            if (editorState.animatePreview) {
                editorState.timeSeconds = std::fmod(anchorTime + (now - anchorTicks) / 1.0e9,
                    editorState.durationSeconds);
            }
            const bool wasAnimating = editorState.animatePreview;
            const double beforeUiTime = editorState.timeSeconds;
            musdesk::drawEditorUi(editorState, composition.textureId());
            if (wasAnimating != editorState.animatePreview || beforeUiTime != editorState.timeSeconds) {
                anchorTime = editorState.timeSeconds;
                anchorTicks = now;
            }
            if (editorState.requestArtwork) {
                editorState.requestArtwork = false;
                artworkDialog.open(resources.window);
            }
            const musdesk::CompositionSettings settings{editorState.title.data(), editorState.artist.data(),
                editorState.durationSeconds, editorState.discRpm};
            composition.renderFrame(editorState.timeSeconds, settings);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (!SDL_GL_SwapWindow(resources.window)) {
                return reportSdlFailure("Presenting OpenGL frame failed");
            }

            // A driver may not support VSync. Keep that path from spinning freely.
            const Uint64 frameDuration = SDL_GetTicks() - frameStart;
            if (!vsyncEnabled && frameDuration < 16) {
                SDL_Delay(static_cast<Uint32>(16 - frameDuration));
            }
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Composition renderer failed: %s", error.what());
        return EXIT_FAILURE;
    }
}
