#include "musdesk/render/VisualizerRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <iostream>
#include <stdexcept>

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
        bool rejected = false;
        try { renderer.setArtwork({}); } catch (const std::exception&) { rejected = true; }
        require(rejected, "Invalid replacement must be rejected");
        renderer.renderFrame(0, settings);
        require(first == renderer.readPixels(), "Failed replacement must preserve the previous artwork");
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
