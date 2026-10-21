#pragma once

#include <string>

namespace musdesk {
struct MediaTools {
    std::string ffmpegPath;
    std::string ffprobePath;
    std::string status;
};

struct AudioMetadata {
    std::string path;
    double durationSeconds;
};

// Searches the configured directory, executable directory, then PATH.
MediaTools detectMediaTools();
bool isAudioPath(const std::string& path);
AudioMetadata importAudio(const std::string& path, const MediaTools& tools);
}
