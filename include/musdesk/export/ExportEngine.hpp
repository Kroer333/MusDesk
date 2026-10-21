#pragma once

#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/media/AudioImporter.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace musdesk {
std::uint64_t exportFrameCount(std::uint64_t sampleFrames);
double exportFrameTime(std::uint64_t index);

enum class ExportPhase { Encoding, Verifying, Completed, Cancelled, Failed };
struct ExportStatus {
    ExportPhase phase = ExportPhase::Encoding;
    std::uint64_t framesWritten = 0;
    std::uint64_t totalFrames = 0;
    std::string message;
    bool active() const { return phase == ExportPhase::Encoding || phase == ExportPhase::Verifying; }
};

// update() and destruction belong to the thread owning the renderer's GL context.
// Keep its artwork unchanged while an export is active.
class ExportEngine {
public:
    ExportEngine(VisualizerRenderer& renderer, CompositionSettings settings,
        std::shared_ptr<const DecodedAudio> audio, MediaTools tools, std::string outputPath);
    ~ExportEngine();
    ExportEngine(const ExportEngine&) = delete;
    ExportEngine& operator=(const ExportEngine&) = delete;
    void update();
    void cancel();
    ExportStatus status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
