#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/presets/PresetStore.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace {
struct Context {
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;
    ~Context() {
        if (gl) SDL_GL_DestroyContext(gl);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char* argv[]) {
    Context context;
    try {
        require(SDL_Init(SDL_INIT_VIDEO), "SDL initialization failed");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        context.window = SDL_CreateWindow("MusDesk render checks", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(context.window != nullptr, "Hidden GL window creation failed");
        context.gl = SDL_GL_CreateContext(context.window);
        require(context.gl != nullptr, "GL context creation failed");
        require(SDL_GL_MakeCurrent(context.window, context.gl), "GL context activation failed");
        musdesk::VisualizerRenderer renderer(std::string(SDL_GetBasePath()) + "assets/Roboto-Medium.ttf");
        if (argc == 2) {
            const auto image = musdesk::loadArtwork(argv[1]);
            renderer.setArtwork(image);
            std::cout << "Real artwork decoded: " << image.width << " x " << image.height << '\n';
        } else {
            renderer.setArtwork({2,2,{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255}});
        }
        musdesk::CompositionSettings settings;
        settings.title = "Deterministic frame";
        renderer.renderFrame(0, settings);
        const auto first = renderer.readPixels();
        renderer.renderFrame(10, settings);
        const auto different = renderer.readPixels();
        require(first != different, "Different timestamps must change the composition");
        renderer.renderFrame(0, settings);
        require(first == renderer.readPixels(), "Seeking back must reproduce identical pixels");
        renderer.renderFrame(0, settings, 1);
        const auto reactive = renderer.readPixels();
        require(first != reactive, "Audio energy must change the rim at the same timestamp");
        renderer.renderFrame(10, settings, 0.2f);
        renderer.renderFrame(0, settings, 1);
        require(reactive == renderer.readPixels(), "Audio-reactive rendering must be repeatable after seeking");
        settings.preset = musdesk::beatPulsePreset();
        renderer.renderFrame(12.3, settings, 0.7f);
        const auto presetFrame = renderer.readPixels();
        const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures";
        std::filesystem::create_directories(folder);
        const auto file = (folder / "render-preset.json").u8string();
        const std::string presetPath(file.begin(), file.end());
        musdesk::savePreset(presetPath, settings.preset);
        settings.preset = musdesk::softGlowPreset();
        renderer.renderFrame(12.3, settings, 0.7f);
        require(presetFrame != renderer.readPixels(), "Built-in presets must produce distinct visuals");
        settings.preset = musdesk::loadPreset(presetPath);
        renderer.renderFrame(12.3, settings, 0.7f);
        require(presetFrame == renderer.readPixels(), "Loading a saved preset must reproduce identical pixels");
        settings.preset.pulseStrength = 0;
        renderer.renderFrame(0, settings, 0);
        const auto noPulse = renderer.readPixels();
        renderer.renderFrame(0, settings, 1);
        require(noPulse == renderer.readPixels(), "Zero pulse strength must disable audio modulation");
        settings.preset = musdesk::softGlowPreset();
        bool rejected = false;
        try { renderer.setArtwork({}); } catch (const std::exception&) { rejected = true; }
        require(rejected, "Invalid replacement must be rejected");
        renderer.renderFrame(0, settings);
        require(first == renderer.readPixels(), "Failed replacement must preserve the previous artwork");
        renderer.clearArtwork();
        renderer.renderFrame(0, settings);
        const auto cleared = renderer.readPixels();
        require(cleared != first, "New/missing-media projects must not display artwork from the previous project");
        renderer.clearArtwork();
        renderer.renderFrame(0, settings);
        require(cleared == renderer.readPixels(), "Clearing artwork repeatedly must be safe");
        for (std::size_t p = 3; p < first.size(); p += 4) {
            require(first[p] == 255, "Composition must remain opaque including text edges");
        }
        require(glGetError() == GL_NO_ERROR, "OpenGL reported an error during composition rendering");
        std::cout << "1080x1920 readback, animation, exact seek repeatability, opacity and replacement checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << ": " << SDL_GetError() << '\n';
        return 1;
    }
}
