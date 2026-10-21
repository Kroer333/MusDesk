#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace musdesk {
struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// Still PNG/JPEG/WebP, UTF-8 path. Throws on unsupported/corrupt/oversized input.
ImageData loadArtwork(const std::string& path);
ImageData blurredBackground(const ImageData& source);
}
