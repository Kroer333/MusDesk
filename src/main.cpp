#include "app/EditorUi.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <cstdlib>

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

int main(int, char*[]) {
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
    musdesk::EditorUiState editorState;

    bool running = true;
    while (running) {
        const Uint64 frameStart = SDL_GetTicks();
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
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
        musdesk::drawEditorUi(editorState);
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
}
