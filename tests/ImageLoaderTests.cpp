#include "musdesk/media/ImageLoader.hpp"

#include <SDL3/SDL.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include <stb_image_write.h>

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void append(void* context, void* bytes, int length) {
    auto& target = *static_cast<std::vector<std::uint8_t>*>(context);
    const auto* begin = static_cast<const std::uint8_t*>(bytes);
    target.insert(target.end(), begin, begin + length);
}
std::string writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Cannot write image test fixture");
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}
void expectRejected(const std::string& path) {
    try { (void)musdesk::loadArtwork(path); }
    catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid artwork was accepted");
}
}

int main() {
    try {
        const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures";
        std::filesystem::create_directories(folder);
        const std::vector<std::uint8_t> rgba{255,0,0,255, 0,255,0,128, 0,0,255,255, 255,255,255,0};
        std::vector<std::uint8_t> png;
        require(stbi_write_png_to_func(append, &png, 2, 2, 4, rgba.data(), 8) != 0, "PNG fixture encoding failed");
        const auto pngPath = writeFile(folder / std::filesystem::path(u8"okładka ze spacją.png"), png);
        const auto decoded = musdesk::loadArtwork(pngPath);
        require(decoded.width == 2 && decoded.height == 2 && decoded.rgba == rgba,
            "PNG pixels/alpha or Unicode path handling failed");

        std::vector<std::uint8_t> solid(8 * 4 * 4);
        for (std::size_t p = 0; p < solid.size(); p += 4) {
            solid[p] = 100; solid[p+1] = 150; solid[p+2] = 200; solid[p+3] = 255;
        }
        std::vector<std::uint8_t> jpeg;
        require(stbi_write_jpg_to_func(append, &jpeg, 8, 4, 4, solid.data(), 100) != 0, "JPEG fixture encoding failed");
        const auto jpegPath = writeFile(folder / "image.jpg", jpeg);
        const auto photo = musdesk::loadArtwork(jpegPath);
        require(photo.width == 8 && photo.height == 4 && photo.rgba.size() == solid.size(), "JPEG dimensions failed");
        require(std::abs(static_cast<int>(photo.rgba[0]) - 100) < 8 && photo.rgba[3] == 255, "JPEG colors failed");
        const auto fixture = (folder / "codec-sample.webp").u8string();
        const auto webp = musdesk::loadArtwork(std::string(fixture.begin(), fixture.end()));
        require(webp.width > 0 && webp.height > 0
            && webp.rgba.size() == static_cast<std::size_t>(webp.width) * webp.height * 4, "WebP decoding failed");
        const auto blurred = musdesk::blurredBackground({8,4,solid});
        require(blurred.width == 8 && blurred.height == 4 && blurred.rgba == solid,
            "Blur must preserve uniform images including edges");
        png.resize(16);
        expectRejected(writeFile(folder / "truncated.png", png));
        expectRejected(writeFile(folder / "not-an-image.webp", {'R','I','F','F',0,0,0,0,'W','E','B','P'}));
        expectRejected((folder / "missing.png").string());
        std::cout << "PNG/JPEG/WebP, Unicode paths, alpha, blur and invalid-input checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
