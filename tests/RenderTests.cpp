#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/presets/PresetStore.hpp"
#include "musdesk/media/AudioImporter.hpp"

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
bool differsIn(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b,
    int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const auto index = ((1919 - y) * 1080 + x) * 4;
            if (a[index] != b[index] || a[index + 1] != b[index + 1] || a[index + 2] != b[index + 2]) return true;
        }
    return false;
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
        if (argc == 4 && (std::string(argv[1]) == "--layouts" || std::string(argv[1]) == "--framing")) {
            const bool framingGallery = std::string(argv[1]) == "--framing";
            renderer.setArtwork(musdesk::loadArtwork(argv[2]));
            const auto audio = musdesk::prepareAudio(argv[3], musdesk::detectMediaTools());
            const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures"
                / (framingGallery ? "framing-gallery" : "layout-gallery");
            std::filesystem::create_directories(folder);
            for (int index = 0; index < (framingGallery ? 3 : 5); ++index) {
                musdesk::CompositionSettings composition;
                composition.title = "MusDesk";
                composition.artist = "FFT visualizer";
                composition.durationSeconds = audio.durationSeconds();
                composition.preset = musdesk::beatPulsePreset();
                composition.format = framingGallery ? musdesk::CompositionFormat::Landscape
                    : static_cast<musdesk::CompositionFormat>(index);
                if (framingGallery && index > 0) {
                    composition.backgroundFraming = {1.4f,0.5f,0.12f};
                    composition.backgroundSoftness = 0;
                }
                if (framingGallery && index == 2) composition.labelFraming = {2,0.75f,0.18f};
                renderer.renderFrame(4.04, composition, audio.reactionAt(4.04, composition.preset.beatThreshold));
                auto pixels = renderer.readPixels();
                std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_CreateSurfaceFrom(
                    renderer.outputWidth(), renderer.outputHeight(), SDL_PIXELFORMAT_RGBA32, pixels.data(),
                    renderer.outputWidth() * 4), SDL_DestroySurface);
                require(surface && SDL_FlipSurface(surface.get(), SDL_FLIP_VERTICAL), "Cannot prepare layout gallery image");
                const auto path = (folder / ("layout-" + std::to_string(index) + ".bmp")).u8string();
                require(SDL_SaveBMP(surface.get(), std::string(path.begin(),path.end()).c_str()), "Cannot save layout gallery image");
            }
            std::cout << "Artwork gallery frames saved.\n";
            return 0;
        }
        if (argc == 2) {
            const auto image = musdesk::loadArtwork(argv[1]);
            renderer.setArtwork(image);
            std::cout << "Real artwork decoded: " << image.width << " x " << image.height << '\n';
        } else {
            renderer.setArtwork({2,2,{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255}});
        }
        musdesk::CompositionSettings settings;
        auto fftSettings = settings;
        fftSettings.preset.ringReactivity = fftSettings.preset.subjectPulse = fftSettings.preset.backgroundReactivity = 0;
        musdesk::AudioReaction spectrumA, spectrumB;
        spectrumA.spectrum[6] = 1;
        spectrumB.spectrum[25] = 1;
        renderer.renderFrame(2, fftSettings, spectrumA);
        const auto lowFrequency = renderer.readPixels();
        renderer.renderFrame(2, fftSettings, spectrumB);
        require(lowFrequency != renderer.readPixels(), "Different FFT bands must deform different ring sectors");
        renderer.renderFrame(9, fftSettings, spectrumB);
        renderer.renderFrame(2, fftSettings, spectrumA);
        require(lowFrequency == renderer.readPixels(), "FFT ring must reproduce identical pixels after seek");
        fftSettings.preset.spectrumGainDb = -12;
        renderer.renderFrame(2, fftSettings, spectrumA);
        require(lowFrequency != renderer.readPixels(), "Spectrum display gain must affect ring pixels without reimporting audio");
        settings.title = "Deterministic frame";
        renderer.renderFrame(0, settings);
        const auto first = renderer.readPixels();
        auto framed = settings;
        framed.backgroundFraming = {2,0,0};
        renderer.renderFrame(0, framed);
        const auto movedBackground = renderer.readPixels();
        require(differsIn(first,movedBackground,0,0,1080,300), "Background framing must change only the chosen artwork crop");
        framed = settings;
        framed.labelFraming = {2,1,1};
        renderer.renderFrame(0, framed);
        const auto movedLabel = renderer.readPixels();
        require(differsIn(first,movedLabel,350,600,730,900), "Label framing must move the rotating artwork label");
        require(!differsIn(first,movedLabel,0,0,1080,300), "Label cropping must not move the background");
        framed = settings;
        framed.backgroundSoftness = 0;
        renderer.renderFrame(0, framed);
        require(differsIn(first,renderer.readPixels(),0,0,1080,300), "Zero softness must use the original sharp artwork");
        renderer.renderFrame(8, settings);
        renderer.renderFrame(0, settings);
        require(first == renderer.readPixels(), "Reset crops must restore identical pixels");
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
        auto isolated = settings;
        isolated.preset.backgroundReactivity = isolated.preset.ringReactivity = 0;
        renderer.renderFrame(4, isolated);
        const auto subjectIdle = renderer.readPixels();
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 0, 1});
        const auto subjectHit = renderer.readPixels();
        require(differsIn(subjectIdle, subjectHit, 100, 380, 980, 1150), "Beat must scale the vinyl");
        require(differsIn(subjectIdle, subjectHit, 100, 1160, 980, 1370), "Beat must scale track title/artist text");
        renderer.renderFrame(9, isolated, musdesk::AudioReaction{1, 1, 1});
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 0, 1});
        require(subjectHit == renderer.readPixels(), "Subject scaling must reproduce the same pixels after seek");
        isolated = settings;
        isolated.preset.subjectPulse = isolated.preset.ringReactivity = 0;
        renderer.renderFrame(4, isolated);
        const auto backgroundIdle = renderer.readPixels();
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 1, 0});
        require(differsIn(backgroundIdle, renderer.readPixels(), 0, 0, 200, 300), "Bass must move/zoom the artwork background");
        isolated.preset.backgroundReactivity = 0;
        isolated.preset.ringReactivity = 1;
        renderer.renderFrame(4, isolated);
        const auto ringIdle = renderer.readPixels();
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 1, 1});
        require(differsIn(ringIdle, renderer.readPixels(), 130, 500, 220, 1000), "Bass/beat must deform the outer ring");
        isolated.preset.ringReactivity = 0;
        renderer.renderFrame(4, isolated, 0.7f);
        const auto legacy = renderer.readPixels();
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0.7f, 1, 1});
        require(legacy == renderer.readPixels(), "Disabled new effects must preserve legacy RMS pixels");
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
        const auto previewTexture = renderer.textureId();
        for (auto format : {musdesk::CompositionFormat::Portrait, musdesk::CompositionFormat::Landscape,
            musdesk::CompositionFormat::Square, musdesk::CompositionFormat::PortraitThreeFour, musdesk::CompositionFormat::LandscapeFourThree}) {
            settings.format = format;
            for (int edge : {720,1080}) {
                settings.shortEdge = edge;
                const auto size = musdesk::outputSize(format, edge);
                renderer.renderFrame(4, settings, spectrumA);
                require(renderer.outputWidth() == size.width && renderer.outputHeight() == size.height
                    && renderer.readPixels().size() == static_cast<std::size_t>(size.width) * size.height * 4,
                    "Framebuffer/readback dimensions must match the selected format");
                require(renderer.textureId() == previewTexture, "Resizing must retain the texture submitted to the preview UI");
            }
        }
        require(glGetError() == GL_NO_ERROR, "OpenGL reported an error during composition rendering");
        std::cout << "1080x1920 readback, animation, exact seek repeatability, opacity and replacement checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << ": " << SDL_GetError() << '\n';
        return 1;
    }
}
