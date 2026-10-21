#include "musdesk/media/ImageLoader.hpp"

#include <SDL3/SDL.h>
#include <src/webp/decode.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 8192
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

namespace musdesk {
ImageData loadArtwork(const std::string& path) {
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(SDL_IOFromFile(path.c_str(), "rb"), SDL_CloseIO);
    if (!stream) {
        throw std::runtime_error(std::string("Cannot open artwork: ") + SDL_GetError());
    }
    const Sint64 length = SDL_GetIOSize(stream.get());
    constexpr Sint64 maxFileBytes = 64 * 1024 * 1024;
    if (length <= 0 || length > maxFileBytes) {
        throw std::runtime_error("Artwork must be a nonempty PNG/JPEG/WebP file smaller than 64 MiB.");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (SDL_ReadIO(stream.get(), bytes.data(), bytes.size()) != bytes.size()) {
        throw std::runtime_error("Cannot read the complete artwork file.");
    }
    constexpr std::array<std::uint8_t, 8> pngSignature{137, 80, 78, 71, 13, 10, 26, 10};
    const bool png = bytes.size() >= pngSignature.size()
        && std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin());
    const bool jpeg = bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff;
    const bool webp = bytes.size() >= 12 && bytes[0] == 'R' && bytes[1] == 'I'
        && bytes[2] == 'F' && bytes[3] == 'F' && bytes[8] == 'W'
        && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P';
    if (webp) {
        WebPBitstreamFeatures features{};
        if (WebPGetFeatures(bytes.data(), bytes.size(), &features) != VP8_STATUS_OK) {
            throw std::runtime_error("Invalid or incomplete WebP artwork.");
        }
        if (features.has_animation) {
            throw std::runtime_error("Animated WebP is not supported. Choose a still image.");
        }
        if (features.width <= 0 || features.height <= 0 || features.width > 8192 || features.height > 8192
            || static_cast<std::uint64_t>(features.width) * features.height > 32 * 1024 * 1024) {
            throw std::runtime_error("Invalid artwork dimensions (maximum 8192 per side and 32 megapixels).");
        }
        ImageData image{features.width, features.height,
            std::vector<std::uint8_t>(static_cast<std::size_t>(features.width) * features.height * 4)};
        if (!WebPDecodeRGBAInto(bytes.data(), bytes.size(), image.rgba.data(), image.rgba.size(), image.width * 4)) {
            throw std::runtime_error("Cannot decode WebP artwork.");
        }
        return image;
    }
    if (!png && !jpeg) {
        throw std::runtime_error("Unsupported artwork format. Choose a still PNG, JPEG or WebP image.");
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels)
        || width <= 0 || height <= 0 || width > 8192 || height > 8192
        || static_cast<std::uint64_t>(width) * height > 32 * 1024 * 1024) {
        throw std::runtime_error("Invalid artwork dimensions (maximum 8192 per side and 32 megapixels).");
    }
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4),
        stbi_image_free);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error(std::string("Cannot decode artwork: ") + (reason ? reason : "invalid image data"));
    }
    const std::size_t byteCount = static_cast<std::size_t>(width) * height * 4;
    return {width, height, std::vector<std::uint8_t>(pixels.get(), pixels.get() + byteCount)};
}

ImageData blurredBackground(const ImageData& source) {
    if (source.width <= 0 || source.height <= 0
        || source.rgba.size() != static_cast<std::size_t>(source.width) * source.height * 4) {
        throw std::runtime_error("Invalid RGBA image for background blur.");
    }
    const float scale = std::min(1.0f, 512.0f / std::max(source.width, source.height));
    ImageData output;
    output.width = std::max(1, static_cast<int>(source.width * scale));
    output.height = std::max(1, static_cast<int>(source.height * scale));
    output.rgba.resize(static_cast<std::size_t>(output.width) * output.height * 4);
    for (int y = 0; y < output.height; ++y) {
        for (int x = 0; x < output.width; ++x) {
            const int sx = std::min(source.width - 1, x * source.width / output.width);
            const int sy = std::min(source.height - 1, y * source.height / output.height);
            for (int c = 0; c < 4; ++c) {
                output.rgba[(y * output.width + x) * 4 + c] = source.rgba[(sy * source.width + sx) * 4 + c];
            }
        }
    }
    // A gentle blur keeps background details visible behind the composition.
    auto temporary = output.rgba;
    constexpr int radius = 2;
    for (int pass = 0; pass < 2; ++pass) {
        for (int axis = 0; axis < 2; ++axis) {
            for (int y = 0; y < output.height; ++y) {
                for (int x = 0; x < output.width; ++x) {
                    for (int c = 0; c < 4; ++c) {
                        int sum = 0;
                        for (int offset = -radius; offset <= radius; ++offset) {
                            const int sx = std::clamp(x + (axis == 0 ? offset : 0), 0, output.width - 1);
                            const int sy = std::clamp(y + (axis == 1 ? offset : 0), 0, output.height - 1);
                            sum += output.rgba[(sy * output.width + sx) * 4 + c];
                        }
                        temporary[(y * output.width + x) * 4 + c] = static_cast<std::uint8_t>(sum / (radius * 2 + 1));
                    }
                }
            }
            output.rgba.swap(temporary);
        }
    }
    return output;
}
}
