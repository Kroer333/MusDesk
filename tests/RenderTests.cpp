#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/presets/PresetStore.hpp"
#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <cmath>
#include <numbers>

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
    int x0, int y0, int x1, int y1, int width = 1080, int height = 1920) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const auto index = ((height-1-y)*width+x)*4;
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
        if (argc >= 4 && std::string(argv[1]) == "--profile-audio") {
            std::cout << std::unitbuf;
            std::ofstream report(argv[2]);
            require(bool(report), "Cannot create preview profile report");
            report << "track,beats,region,edge,mean_ms,p95_ms,lookup_us\n";
            const auto tools = musdesk::detectMediaTools();
            renderer.setArtwork({2,2,{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255}});
            for (int source = 3; source < argc; ++source) {
                std::cout << "Preparing audio sample " << source-2 << '\n';
                const auto audio = musdesk::prepareAudio(argv[source],tools);
                double denseTime = 0;
                std::size_t maximum = 0, last = 0;
                for (std::size_t first = 0; first < audio.rhythm.beats.size(); ++first) {
                    while (last < audio.rhythm.beats.size()
                        && audio.rhythm.beats[last].timeSeconds < audio.rhythm.beats[first].timeSeconds+1) ++last;
                    if (last-first > maximum) { maximum = last-first; denseTime = audio.rhythm.beats[first].timeSeconds; }
                }
                const auto lookupStart = SDL_GetTicksNS();
                double checksum = 0;
                for (int query = 0; query < 10000; ++query) {
                    const auto reaction = audio.reactionAt(audio.durationSeconds()*query/10000,0.55f,musdesk::BeatBand::Bass);
                    checksum += reaction.beat+reaction.kick;
                }
                const double lookupUs = (SDL_GetTicksNS()-lookupStart)/1e7;
                musdesk::CompositionSettings settings;
                settings.durationSeconds = audio.durationSeconds();
                settings.preset = musdesk::sideKickPreset();
                const auto name = std::filesystem::u8path(argv[source]).filename().string();
                for (int edge : {1080,720,540,360}) for (int region = 0; region < 2; ++region) {
                    settings.shortEdge = edge >= 720 ? edge : 1080;
                    std::vector<double> timings;
                    const double begin = region ? denseTime : 0;
                    for (int frame = -3; frame < 12; ++frame) {
                        const double time = std::min(audio.durationSeconds()-0.001,begin+std::max(0,frame)/60.0);
                        const auto started = SDL_GetTicksNS();
                        if (edge >= 720) renderer.renderFrame(time,settings,audio.reactionAt(time,0.55f,musdesk::BeatBand::Bass));
                        else renderer.renderPreviewFrame(time,settings,audio.reactionAt(time,0.55f,musdesk::BeatBand::Bass),edge);
                        glFinish();
                        if (frame >= 0) timings.push_back((SDL_GetTicksNS()-started)/1e6);
                    }
                    const auto mean = std::accumulate(timings.begin(),timings.end(),0.0)/timings.size();
                    std::sort(timings.begin(),timings.end());
                    const auto p95 = timings[static_cast<std::size_t>((timings.size()-1)*0.95)];
                    report << name << ',' << audio.rhythm.beats.size() << ',' << (region ? "dense" : "opening")
                        << ',' << edge << ',' << mean << ',' << p95 << ',' << lookupUs << '\n';
                    report.flush();
                    std::cout << name << " edge=" << edge << " dense=" << region << " mean=" << mean
                        << " ms p95=" << p95 << " ms lookup=" << lookupUs << " us checksum=" << checksum << '\n';
                }
            }
            return 0;
        }
        if (argc == 4 && (std::string(argv[1]) == "--layouts" || std::string(argv[1]) == "--framing" || std::string(argv[1]) == "--styles" || std::string(argv[1]) == "--scenes")) {
            const bool framingGallery = std::string(argv[1]) == "--framing";
            const bool styleGallery = std::string(argv[1]) == "--styles";
            const bool sceneGallery = std::string(argv[1]) == "--scenes";
            renderer.setArtwork(musdesk::loadArtwork(argv[2]));
            const auto audio = musdesk::prepareAudio(argv[3], musdesk::detectMediaTools());
            const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures"
                / (sceneGallery ? "scene-gallery" : (styleGallery ? "style-gallery" : (framingGallery ? "framing-gallery" : "layout-gallery")));
            std::filesystem::create_directories(folder);
            for (int index = 0; index < (sceneGallery ? 4 : (framingGallery ? 3 : 5)); ++index) {
                musdesk::CompositionSettings composition;
                composition.title = "MusDesk";
                composition.artist = "FFT visualizer";
                composition.durationSeconds = audio.durationSeconds();
                composition.preset = musdesk::beatPulsePreset();
                if (styleGallery) {
                    if (index == 0) composition.preset = musdesk::softGlowPreset();
                    if (index == 2) composition.preset = musdesk::sideKickPreset();
                    if (index >= 3) composition.preset = musdesk::spectrumPulsePreset();
                    if (index == 4) composition.preset.spectrumStrength = 4;
                    composition.title = composition.preset.name;
                    composition.artist = index == 4 ? "Spectrum strength: 4" : "MusDesk";
                }
                composition.format = styleGallery ? musdesk::CompositionFormat::Portrait : (framingGallery ? musdesk::CompositionFormat::Landscape
                    : static_cast<musdesk::CompositionFormat>(index));
                if (sceneGallery) {
                    const char* names[]{"Vinyl","Artwork card","Spectrum bars","Background & titles"};
                    composition.title = names[index];
                    composition.artist = "MusDesk";
                    composition.format = musdesk::CompositionFormat::Landscape;
                    composition.design.scene = static_cast<musdesk::SceneKind>(index);
                    if (index >= 2) composition.design.background = musdesk::BackgroundKind::Gradient;
                }
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
        fftSettings.preset.spectrumEnabled = true;
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
        fftSettings.preset.glowEnabled = false;
        renderer.renderFrame(2, fftSettings, spectrumA);
        const auto outlineOnly = renderer.readPixels();
        fftSettings.preset.spectrumEnabled = false;
        renderer.renderFrame(2, fftSettings, spectrumA);
        require(outlineOnly != renderer.readPixels(), "Spectrum outline must remain visible with glow disabled");
        fftSettings.preset.glowEnabled = true;
        renderer.renderFrame(2, fftSettings, spectrumA);
        const auto glowOnly = renderer.readPixels();
        fftSettings.preset.spectrumEnabled = true;
        renderer.renderFrame(2, fftSettings, spectrumA);
        const auto balancedSpectrum = renderer.readPixels();
        for (int y = 395; y < 1125; ++y) {
            for (int x = 175; x < 905; ++x) {
                const double dx = x+0.5-540, dy = y+0.5-760;
                if (dx*dx+dy*dy >= 365*365) continue;
                const auto pixel = ((1919-y)*1080+x)*4;
                require(std::equal(glowOnly.begin()+pixel,glowOnly.begin()+pixel+3,
                    balancedSpectrum.begin()+pixel),
                    "Enabling spectrum must preserve the record radius and artwork pixels");
            }
        }
        // Compare only the additive line, excluding artwork and background asymmetry.
        std::array<double,4> quadrants{};
        double mirrorDifference = 0;
        for (int y = 300; y < 1220; ++y) {
            for (int x = 80; x < 1000; ++x) {
                const double dx = x+0.5-540, dy = y+0.5-760;
                if (dx*dx+dy*dy < 380*380) continue;
                const auto pixel = ((1919-y)*1080+x)*4;
                const auto mirror = ((1919-y)*1080+(1079-x))*4;
                mirrorDifference += std::abs((int(balancedSpectrum[pixel])-int(glowOnly[pixel]))
                    -(int(balancedSpectrum[mirror])-int(glowOnly[mirror])));
                for (int c = 0; c < 3; ++c)
                    quadrants[(dy >= 0 ? 2 : 0)+(dx >= 0 ? 1 : 0)] +=
                        int(balancedSpectrum[pixel+c])-int(glowOnly[pixel+c]);
            }
        }
        const auto [minimum,maximum] = std::minmax_element(quadrants.begin(),quadrants.end());
        require(*minimum > 1000 && *maximum < *minimum*2,
            "An irregular spectrum must still react around the entire record");
        require(mirrorDifference > 1000,
            "Spectrum detail must not form a mirrored pattern");
        fftSettings.preset.spectrumEnabled = false;
        renderer.renderFrame(2, fftSettings, spectrumB);
        require(glowOnly == renderer.readPixels(), "Glow alone must not deform with FFT bands");
        auto angularSettings = fftSettings;
        angularSettings.format = musdesk::CompositionFormat::Landscape;
        angularSettings.preset.spectrumEnabled = true;
        angularSettings.preset.glowEnabled = false;
        angularSettings.preset.backgroundBrightness = 0;
        angularSettings.preset.spectrumStrength = 4;
        angularSettings.preset.pulseStrength = 1;
        angularSettings.preset.spectrumGainDb = 0;
        musdesk::AudioReaction broadSpectrum;
        broadSpectrum.spectrum.fill(0.8f);
        renderer.renderFrame(2,angularSettings,broadSpectrum);
        const auto angularFrame = renderer.readPixels();
        auto previewPixels = angularFrame;
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> angularSurface(SDL_CreateSurfaceFrom(
            1920,1080,SDL_PIXELFORMAT_RGBA32,previewPixels.data(),1920*4),SDL_DestroySurface);
        const auto angularPath = std::filesystem::path(SDL_GetBasePath())/"test-fixtures"/"angular-spectrum.bmp";
        std::filesystem::create_directories(angularPath.parent_path());
        require(angularSurface && SDL_FlipSurface(angularSurface.get(),SDL_FLIP_VERTICAL)
            && SDL_SaveBMP(angularSurface.get(),angularPath.string().c_str()), "Cannot save angular spectrum check frame");
        std::array<int,128> radii{};
        for (int node = 0; node < 128; ++node) {
            const double angle = node/128.0*2*std::numbers::pi-std::numbers::pi;
            int brightest = -1;
            for (int radius = 335; radius < 450; ++radius) {
                const int x = static_cast<int>(std::lround(520+radius*std::cos(angle)-0.5));
                const int y = static_cast<int>(std::lround(540+radius*std::sin(angle)-0.5));
                const int value = angularFrame[((1079-y)*1920+x)*4];
                if (value > brightest) { brightest = value; radii[node] = radius; }
            }
        }
        int variation = 0;
        for (int node = 0; node < 128; ++node)
            variation += std::abs(radii[node]-radii[(node+1)%128]);
        require(variation > 128*3, "Adjacent spectrum vertices must produce distinct angular peaks");
        renderer.renderFrame(2.12,angularSettings,broadSpectrum);
        require(angularFrame != renderer.readPixels(), "Spectrum vertices must evolve with playback time");
        renderer.renderFrame(2,angularSettings,broadSpectrum);
        require(angularFrame == renderer.readPixels(), "Angular spectrum must remain reproducible after seek");
        auto controls = angularSettings;
        controls.preset.spectrumStrength = 2;
        musdesk::AudioReaction controlAudio = broadSpectrum;
        controlAudio.energy = 0.7f;
        renderer.renderFrame(2,controls,controlAudio);
        const auto spectrumReference = renderer.readPixels();
        for (auto change : std::array{
            +[](musdesk::VisualPreset& p) { p.spectrumPoints = 32; },
            +[](musdesk::VisualPreset& p) { p.spectrumSharpness = 0; },
            +[](musdesk::VisualPreset& p) { p.spectrumRandomness = 0; },
            +[](musdesk::VisualPreset& p) { p.spectrumSpeed = 0; },
            +[](musdesk::VisualPreset& p) { p.spectrumLineWidth = 2; },
            +[](musdesk::VisualPreset& p) { p.spectrumGap = 0.16f; },
            +[](musdesk::VisualPreset& p) { p.spectrumIntensity = 0.3f; },
            +[](musdesk::VisualPreset& p) { p.spectrumColor = {1,0,0}; }}) {
            auto adjusted = controls;
            change(adjusted.preset);
            renderer.renderFrame(2,adjusted,controlAudio);
            require(renderer.readPixels() != spectrumReference,"Spectrum style control must affect rendered pixels");
        }
        auto adjusted = controls;
        adjusted.preset.glowColor = {1,0,0};
        adjusted.preset.glowWidth = 3;
        renderer.renderFrame(2,adjusted,controlAudio);
        require(!differsIn(renderer.readPixels(),spectrumReference,60,60,1000,1020,1920,1080),
            "Disabled glow controls must not recolor or deform spectrum");
        controls.preset.spectrumEnabled = false;
        controls.preset.glowEnabled = true;
        renderer.renderFrame(2,controls,controlAudio);
        const auto glowReference = renderer.readPixels();
        for (auto change : std::array{
            +[](musdesk::VisualPreset& p) { p.glowWidth = 2; },
            +[](musdesk::VisualPreset& p) { p.glowSoftness = 0.3f; },
            +[](musdesk::VisualPreset& p) { p.glowBase = 0; },
            +[](musdesk::VisualPreset& p) { p.glowReactivity = 0; },
            +[](musdesk::VisualPreset& p) { p.glowIntensity = 0.3f; },
            +[](musdesk::VisualPreset& p) { p.glowColor = {1,0,0}; }}) {
            adjusted = controls;
            change(adjusted.preset);
            renderer.renderFrame(2,adjusted,controlAudio);
            require(renderer.readPixels() != glowReference,"Glow style control must affect rendered pixels");
        }
        adjusted = controls;
        adjusted.preset.spectrumColor = {1,0,0};
        renderer.renderFrame(2,adjusted,controlAudio);
        require(renderer.readPixels() == glowReference,"Disabled spectrum must not affect glow");
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
        require(differsIn(subjectIdle, subjectHit, 100, 1420, 980, 1650), "Overlay pulse must include timeline and timestamps");
        renderer.renderFrame(9, isolated, musdesk::AudioReaction{1, 1, 1});
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 0, 1});
        require(subjectHit == renderer.readPixels(), "Subject scaling must reproduce the same pixels after seek");
        auto cameraSettings = settings;
        cameraSettings.preset = musdesk::sideKickPreset();
        musdesk::AudioReaction cameraAudio;
        cameraAudio.kick = 1;
        renderer.renderFrame(4, cameraSettings, cameraAudio);
        const auto rightKick = renderer.readPixels();
        cameraAudio.kick = -1;
        renderer.renderFrame(4, cameraSettings, cameraAudio);
        require(rightKick != renderer.readPixels(), "Opposite kicks must shift the composition in opposite directions");
        cameraAudio.kick = 1;
        renderer.renderFrame(4, cameraSettings, cameraAudio);
        require(rightKick == renderer.readPixels(), "Camera rendering must reproduce identical pixels after seek");
        isolated = settings;
        isolated.preset.subjectPulse = isolated.preset.ringReactivity = 0;
        renderer.renderFrame(4, isolated);
        const auto backgroundIdle = renderer.readPixels();
        renderer.renderFrame(4, isolated, musdesk::AudioReaction{0, 1, 0});
        require(differsIn(backgroundIdle, renderer.readPixels(), 0, 0, 200, 300), "Bass must move/zoom the artwork background");
        isolated.preset.backgroundReactivity = 0;
        isolated.preset.ringReactivity = 1;
        isolated.preset.wavesEnabled = true;
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
        for (int displayEdge : {250,360,420,540,700,720,900,1500}) {
            renderer.renderPreviewFrame(4,settings,spectrumA,displayEdge);
            const auto expected = musdesk::previewSize(settings.format,displayEdge,settings.shortEdge);
            require(renderer.outputWidth() == expected.width && renderer.outputHeight() == expected.height,
                "Preview framebuffer must match the displayed size budget");
            require(renderer.textureId() == previewTexture,"Preview resizing must retain its texture ID");
        }
        renderer.renderFrame(4,settings,spectrumA);
        const auto restoredOutput = musdesk::outputSize(settings.format,settings.shortEdge);
        require(renderer.outputWidth() == restoredOutput.width && renderer.outputHeight() == restoredOutput.height,
            "Export rendering must restore full output size after reduced-resolution preview");
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
        for (auto format : {musdesk::CompositionFormat::Portrait,musdesk::CompositionFormat::Landscape,
            musdesk::CompositionFormat::Square,musdesk::CompositionFormat::PortraitThreeFour,musdesk::CompositionFormat::LandscapeFourThree}) {
            std::vector<std::uint8_t> previous;
            for (int scene=0; scene<4; ++scene) {
                musdesk::CompositionSettings variant;
                variant.format = format;
                variant.shortEdge = 720;
                variant.design.scene = static_cast<musdesk::SceneKind>(scene);
                variant.preset = musdesk::spectrumPulsePreset();
                renderer.renderFrame(4,variant,controlAudio);
                const auto pixels = renderer.readPixels();
                require(pixels != previous,"Different scene types must produce different compositions");
                renderer.renderFrame(8,variant,controlAudio);
                renderer.renderFrame(4,variant,controlAudio);
                require(renderer.readPixels() == pixels,"Every scene must remain repeatable after seek");
                previous = pixels;
            }
        }
        musdesk::CompositionSettings backgroundOnly;
        backgroundOnly.format = musdesk::CompositionFormat::Square;
        backgroundOnly.shortEdge = 720;
        backgroundOnly.design.scene = musdesk::SceneKind::Titles;
        backgroundOnly.design.background = musdesk::BackgroundKind::Solid;
        backgroundOnly.design.showTitle = backgroundOnly.design.showArtist = false;
        backgroundOnly.design.showProgress = backgroundOnly.design.showTimestamps = false;
        renderer.renderFrame(4,backgroundOnly,controlAudio);
        const auto solid = renderer.readPixels();
        for (std::size_t pixel=0; pixel<solid.size(); pixel+=4)
            require(std::equal(solid.begin(),solid.begin()+3,solid.begin()+pixel),
                "Hiding all elements over a solid background must leave only that color");
        renderer.clearArtwork();
        renderer.renderFrame(4,backgroundOnly,controlAudio);
        require(renderer.readPixels() == solid,"Solid title-only scenes must not depend on artwork");
        backgroundOnly.design.background = musdesk::BackgroundKind::Gradient;
        renderer.renderFrame(4,backgroundOnly,controlAudio);
        require(renderer.readPixels() != solid,"Gradient background must differ from solid fill without artwork");
        require(glGetError() == GL_NO_ERROR, "OpenGL reported an error during composition rendering");
        std::cout << "1080x1920 readback, animation, exact seek repeatability, opacity and replacement checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << ": " << SDL_GetError() << '\n';
        return 1;
    }
}
