#include "app/EditorUi.hpp"
#include "app/ArtworkDialog.hpp"
#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/media/AudioImporter.hpp"
#include "musdesk/audio/AudioPlayer.hpp"
#include "musdesk/presets/PresetStore.hpp"
#include "musdesk/export/ExportEngine.hpp"

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
#include <chrono>
#include <future>
#include <stop_token>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <iostream>

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

struct StopImportOnExit {
    std::stop_source& source;
    ~StopImportOnExit() { source.request_stop(); }
};

bool isJsonPath(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    auto extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return extension == ".json";
}
}

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "MusDesk " MUSDESK_VERSION "\n";
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "MusDesk " MUSDESK_VERSION "\nUsage: MusDesk [--artwork image.png] [--audio track.mp3] [--preset preset.json]\n";
        return EXIT_SUCCESS;
    }
    std::string startupArtwork;
    std::string startupAudio;
    std::string startupPreset;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 < argc && std::string(argv[i]) == "--artwork") startupArtwork = argv[i + 1];
        else if (i + 1 < argc && std::string(argv[i]) == "--audio") startupAudio = argv[i + 1];
        else if (i + 1 < argc && std::string(argv[i]) == "--preset") startupPreset = argv[i + 1];
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: MusDesk [--artwork image.png] [--audio track.mp3] [--preset preset.json]");
            return EXIT_FAILURE;
        }
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
        musdesk::ArtworkDialog audioDialog;
        musdesk::ArtworkDialog presetOpenDialog;
        musdesk::ArtworkDialog presetSaveDialog;
        musdesk::ArtworkDialog exportDialog;
        std::unique_ptr<musdesk::ExportEngine> exporter;
        bool exportVsyncDisabled = false;
        Uint64 lastExportUiFrame = 0;
        auto importPreset = [&](const std::string& path) {
            editorState.presetStatus.clear();
            try {
                const auto preset = musdesk::loadPreset(path);
                musdesk::applyVisualPreset(editorState, preset);
                editorState.presetStatus = "Preset loaded.";
                SDL_Log("Preset loaded: %s", preset.name.c_str());
            } catch (const std::exception& error) {
                editorState.presetError = error.what();
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
            }
        };
        if (!startupPreset.empty()) importPreset(startupPreset);
        musdesk::MediaTools mediaTools;
        musdesk::AudioPlayer audioPlayer;
        audioPlayer.setVolume(editorState.previewVolume / 100);
        std::shared_ptr<const musdesk::DecodedAudio> audioClip;
        auto toolsFuture = std::async(std::launch::async, musdesk::detectMediaTools);
        std::stop_source audioCancellation;
        std::future<std::shared_ptr<const musdesk::DecodedAudio>> audioFuture;
        StopImportOnExit stopImport{audioCancellation};
        bool resumeAfterSeek = false;
        auto importAudio = [&](const std::string& path) {
            if (!editorState.toolsChecked) {
                startupAudio = path;
                return;
            }
            if (audioFuture.valid()) {
                editorState.audioError = "An audio import is already in progress.";
                return;
            }
            editorState.audioError.clear();
            editorState.audioImportPending = true;
            audioCancellation = std::stop_source{};
            audioFuture = std::async(std::launch::async, [path, tools = mediaTools, token = audioCancellation.get_token()]
                    () -> std::shared_ptr<const musdesk::DecodedAudio> {
                return std::make_shared<musdesk::DecodedAudio>(musdesk::prepareAudio(path, tools, token));
            });
        };
        auto importArtwork = [&](const std::string& path) {
            try {
                const auto image = musdesk::loadArtwork(path);
                const auto absolute = std::filesystem::absolute(std::filesystem::u8path(path)).u8string();
                composition.setArtwork(image);
                editorState.artworkPath.assign(absolute.begin(), absolute.end());
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
        auto playbackError = [&](const std::exception& error) {
            editorState.audioError = error.what();
            editorState.audioPlaying = false;
            editorState.audioReady = false;
            try { audioPlayer.pause(); } catch (const std::exception&) {}
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
        };
        auto refreshPlayback = [&] {
            if (!editorState.audioReady || editorState.exportActive) return;
            try {
                auto playback = audioPlayer.state();
                if (playback.ended && editorState.audioPlaying && editorState.loopAudio && !editorState.audioSeeking) {
                    audioPlayer.play();
                    playback = audioPlayer.state();
                }
                if (!editorState.audioSeeking) editorState.timeSeconds = playback.timeSeconds;
                editorState.audioPlaying = playback.playing;
                editorState.audioEnergy = audioClip->envelope.at(editorState.timeSeconds);
            } catch (const std::exception& error) { playbackError(error); }
        };

        bool running = true;
        Uint64 lastSourceCheck = 0;
        std::string sourceError;
        while (running) {
            const Uint64 frameStart = SDL_GetTicks();
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr
                    && event.drop.windowID == SDL_GetWindowID(resources.window)) {
                    if (editorState.exportBusy) editorState.exportStatus = "Finish or cancel export before importing files.";
                    else if (isJsonPath(event.drop.data)) importPreset(event.drop.data);
                    else if (musdesk::isAudioPath(event.drop.data)) importAudio(event.drop.data);
                    else importArtwork(event.drop.data);
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
            if (toolsFuture.valid() && toolsFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    mediaTools = toolsFuture.get();
                    editorState.ffprobeReady = !mediaTools.ffprobePath.empty();
                    editorState.ffmpegReady = !mediaTools.ffmpegPath.empty();
                    editorState.mediaToolsStatus = mediaTools.status;
                    SDL_Log("%s", mediaTools.status.c_str());
                } catch (const std::exception& error) {
                    editorState.mediaToolsStatus = std::string("Media tool detection failed: ") + error.what();
                }
                editorState.toolsChecked = true;
                if (!startupAudio.empty()) {
                    importAudio(startupAudio);
                    startupAudio.clear();
                }
            }
            if (audioFuture.valid() && audioFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    const auto audio = audioFuture.get();
                    if (audioCancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
                    audioPlayer.load(audio);
                    audioClip = audio;
                    sourceError.clear();
                    lastSourceCheck = 0;
                    editorState.audioPath = audio->path;
                    const auto slash = audio->path.find_last_of("/\\");
                    editorState.audioName = audio->path.substr(slash == std::string::npos ? 0 : slash + 1);
                    editorState.durationSeconds = audio->durationSeconds();
                    editorState.timeSeconds = 0;
                    editorState.audioReady = true;
                    editorState.audioPlaying = false;
                    editorState.audioSeeking = false;
                    editorState.animatePreview = false;
                    resumeAfterSeek = false;
                    anchorTime = 0;
                    anchorTicks = SDL_GetTicksNS();
                    SDL_Log("Audio ready: %.3f seconds, %zu RMS values", audio->durationSeconds(), audio->envelope.values.size());
                } catch (const std::exception& error) {
                    if (!audioCancellation.stop_requested()) {
                        editorState.audioError = error.what();
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
                    }
                }
                editorState.audioImportPending = false;
            }
            if (auto selection = artworkDialog.takeSelection()) {
                if (!selection->error.empty()) editorState.importError = selection->error;
                else if (!selection->path.empty()) importArtwork(selection->path);
            }
            editorState.artworkDialogPending = artworkDialog.pending();
            if (auto selection = audioDialog.takeSelection()) {
                if (!selection->error.empty()) editorState.audioError = selection->error;
                else if (!selection->path.empty()) importAudio(selection->path);
            }
            editorState.audioDialogPending = audioDialog.pending();
            if (auto selection = presetOpenDialog.takeSelection()) {
                if (!selection->error.empty()) editorState.presetError = selection->error;
                else if (!selection->path.empty()) importPreset(selection->path);
            }
            if (auto selection = presetSaveDialog.takeSelection()) {
                if (!selection->error.empty()) editorState.presetError = selection->error;
                else if (!selection->path.empty()) {
                    try {
                        auto path = selection->path;
                        const auto slash = path.find_last_of("/\\");
                        const auto dot = path.find_last_of('.');
                        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) path += ".json";
                        if (!isJsonPath(path)) throw std::runtime_error("Preset files must use the .json extension.");
                        musdesk::savePreset(path, editorState.preset);
                        editorState.presetDirty = false;
                        editorState.presetError.clear();
                        editorState.presetStatus = "Preset saved.";
                    } catch (const std::exception& error) { editorState.presetError = error.what(); }
                }
            }
            editorState.presetDialogPending = presetOpenDialog.pending() || presetSaveDialog.pending();
            if (auto selection = exportDialog.takeSelection()) {
                editorState.exportError = false;
                if (!selection->error.empty()) {
                    editorState.exportStatus = selection->error;
                    editorState.exportError = true;
                } else if (!selection->path.empty()) {
                    try {
                        auto path = selection->path;
                        const auto slash = path.find_last_of("/\\");
                        const auto dot = path.find_last_of('.');
                        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) path += ".mp4";
                        if (!audioClip || editorState.artworkName.empty()) throw std::runtime_error("Import artwork and audio before exporting.");
                        musdesk::validateExportDestination(path, audioClip->path, editorState.artworkPath);
                        audioPlayer.pause();
                        editorState.audioPlaying = false;
                        editorState.audioSeeking = false;
                        resumeAfterSeek = false;
                        const musdesk::CompositionSettings settings{editorState.title.data(), editorState.artist.data(),
                            audioClip->durationSeconds(), editorState.preset};
                        exporter = std::make_unique<musdesk::ExportEngine>(composition, settings, audioClip, mediaTools, path,
                            editorState.exportEncoder == 0 ? musdesk::ExportEncoder::Automatic : musdesk::ExportEncoder::Cpu);
                    } catch (const std::exception& error) {
                        editorState.exportStatus = error.what();
                        editorState.exportError = true;
                    }
                }
            }
            editorState.exportDialogPending = exportDialog.pending();
            if (exporter) {
                exporter->update();
                const auto status = exporter->status();
                editorState.exportActive = status.active();
                editorState.exportProgress = status.phase == musdesk::ExportPhase::Completed ? 1.0f
                    : std::min(0.98f, static_cast<float>(status.framesWritten) / static_cast<float>(status.totalFrames));
                editorState.exportStatus = status.phase == musdesk::ExportPhase::Preparing ? status.message
                    : status.phase == musdesk::ExportPhase::Verifying ? "Verifying MP4 with ffprobe..."
                    : status.phase == musdesk::ExportPhase::Encoding ? "Encoding frame " + std::to_string(status.framesWritten)
                        + " / " + std::to_string(status.totalFrames) : status.message;
                if (!status.encoder.empty()) {
                    editorState.exportStatus += "\n" + status.encoder;
                    if (status.framesWritten && status.elapsedSeconds > 0) {
                        const double fps = status.framesWritten / status.elapsedSeconds;
                        editorState.exportStatus += " | " + std::to_string(static_cast<int>(fps)) + " FPS";
                        if (status.active() && fps > 0) editorState.exportStatus += " | ~"
                            + std::to_string(static_cast<int>((status.totalFrames - status.framesWritten) / fps)) + " s left";
                    }
                }
                editorState.exportError = status.phase == musdesk::ExportPhase::Failed;
                if (!status.active()) {
                    SDL_Log("%s", editorState.exportStatus.c_str());
                    exporter.reset();
                }
            }
            editorState.exportBusy = editorState.exportActive || editorState.exportDialogPending;
            if (audioClip && !editorState.exportActive && (lastSourceCheck == 0 || SDL_GetTicks() - lastSourceCheck >= 1000)) {
                lastSourceCheck = SDL_GetTicks();
                try { musdesk::validateAudioSource(*audioClip); sourceError.clear(); }
                catch (const std::exception& error) { sourceError = error.what(); }
            }
            editorState.exportBlockedReason = !editorState.toolsChecked ? "Checking FFmpeg and ffprobe..."
                : !editorState.ffmpegReady || !editorState.ffprobeReady ? "Install FFmpeg and ffprobe, then restart MusDesk."
                : editorState.audioImportPending ? "Wait for audio preparation to finish."
                : editorState.artworkName.empty() ? "Import artwork before exporting."
                : !editorState.audioReady ? "Import audio with a working preview before exporting."
                : sourceError;
            if (editorState.exportBlockedReason.empty()) {
                try { musdesk::validatePreset(editorState.preset); }
                catch (const std::exception&) { editorState.exportBlockedReason = "Enter a valid preset name and effect settings before exporting."; }
            }
            if (editorState.exportActive && !exportVsyncDisabled) {
                exportVsyncDisabled = SDL_GL_SetSwapInterval(0);
            } else if (!editorState.exportActive && exportVsyncDisabled) {
                SDL_GL_SetSwapInterval(vsyncEnabled ? 1 : 0);
                exportVsyncDisabled = false;
            }
            refreshPlayback();
            if ((SDL_GetWindowFlags(resources.window) & SDL_WINDOW_MINIMIZED) != 0) {
                SDL_Delay(editorState.exportActive ? 1 : 50);
                continue;
            }
            if (editorState.exportActive) {
                if (SDL_GetTicks() - lastExportUiFrame < 33) {
                    SDL_Delay(1);
                    continue;
                }
                lastExportUiFrame = SDL_GetTicks();
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
            if (editorState.audioPath.empty() && editorState.animatePreview) {
                editorState.timeSeconds = std::fmod(anchorTime + (now - anchorTicks) / 1.0e9,
                    editorState.durationSeconds);
            }
            const bool wasAnimating = editorState.animatePreview;
            const double beforeUiTime = editorState.timeSeconds;
            const float beforeVolume = editorState.previewVolume;
            const bool beforeMute = editorState.muteAudio;
            musdesk::drawEditorUi(editorState, composition.textureId());
            if (editorState.audioReady && !editorState.exportBusy && !io.WantCaptureKeyboard) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) editorState.requestPlayPause = true;
                const double shift = (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 5.0 : 0)
                    - (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? 5.0 : 0);
                if (shift != 0) {
                    editorState.timeSeconds = std::clamp(editorState.timeSeconds + shift, 0.0, editorState.durationSeconds);
                    editorState.seekAudio = true;
                }
            }
            if (editorState.requestCancelAudio) {
                editorState.requestCancelAudio = false;
                audioCancellation.request_stop();
            }
            try {
                if (beforeVolume != editorState.previewVolume || beforeMute != editorState.muteAudio)
                    audioPlayer.setVolume(editorState.muteAudio ? 0 : editorState.previewVolume / 100);
                if (editorState.audioReady) {
                    if (editorState.beginAudioSeek) {
                        resumeAfterSeek = audioPlayer.state().playing;
                        audioPlayer.pause();
                        editorState.audioPlaying = false;
                    }
                    if (editorState.seekAudio) audioPlayer.seek(editorState.timeSeconds);
                    if (editorState.endAudioSeek && resumeAfterSeek && editorState.timeSeconds < editorState.durationSeconds)
                        audioPlayer.play();
                    if (editorState.requestPlayPause) {
                        if (audioPlayer.state().playing) {
                            audioPlayer.pause();
                            editorState.audioPlaying = false;
                        } else audioPlayer.play();
                    }
                    if (editorState.requestRestart) {
                        audioPlayer.seek(0);
                        audioPlayer.play();
                    }
                }
            } catch (const std::exception& error) { playbackError(error); }
            editorState.beginAudioSeek = editorState.seekAudio = editorState.endAudioSeek = false;
            editorState.requestPlayPause = editorState.requestRestart = false;
            refreshPlayback();
            if (wasAnimating != editorState.animatePreview || beforeUiTime != editorState.timeSeconds) {
                anchorTime = editorState.timeSeconds;
                anchorTicks = now;
            }
            if (editorState.requestArtwork) {
                editorState.requestArtwork = false;
                artworkDialog.open(resources.window);
            }
            if (editorState.requestAudio) {
                editorState.requestAudio = false;
                audioDialog.open(resources.window, musdesk::FileDialogKind::Audio);
            }
            if (editorState.requestLoadPreset) {
                editorState.requestLoadPreset = false;
                presetOpenDialog.open(resources.window, musdesk::FileDialogKind::PresetOpen);
            }
            if (editorState.requestSavePreset) {
                editorState.requestSavePreset = false;
                try {
                    musdesk::validatePreset(editorState.preset);
                    presetSaveDialog.open(resources.window, musdesk::FileDialogKind::PresetSave);
                } catch (const std::exception& error) { editorState.presetError = error.what(); }
            }
            if (editorState.requestCancelExport) {
                editorState.requestCancelExport = false;
                if (exporter) exporter->cancel();
            }
            if (editorState.requestExport) {
                editorState.requestExport = false;
                try {
                    musdesk::validatePreset(editorState.preset);
                    exportDialog.open(resources.window, musdesk::FileDialogKind::ExportSave);
                    editorState.exportBusy = editorState.exportDialogPending = exportDialog.pending();
                    editorState.exportStatus.clear();
                    editorState.exportError = false;
                } catch (const std::exception& error) {
                    editorState.exportStatus = error.what();
                    editorState.exportError = true;
                }
            }
            const musdesk::CompositionSettings settings{editorState.title.data(), editorState.artist.data(),
                editorState.durationSeconds, editorState.preset};
            if (!editorState.exportActive) composition.renderFrame(editorState.timeSeconds, settings, editorState.audioEnergy);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (!SDL_GL_SwapWindow(resources.window)) {
                return reportSdlFailure("Presenting OpenGL frame failed");
            }

            // A driver may not support VSync. Keep that path from spinning freely.
            const Uint64 frameDuration = SDL_GetTicks() - frameStart;
            if (!editorState.exportActive && !vsyncEnabled && frameDuration < 16) {
                SDL_Delay(static_cast<Uint32>(16 - frameDuration));
            }
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Composition renderer failed: %s", error.what());
        return EXIT_FAILURE;
    }
}
