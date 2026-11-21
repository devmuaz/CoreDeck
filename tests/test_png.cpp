#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <miniz.h>

#include "platform/png.h"

using namespace CoreDeck::Platform;

namespace {
    void AppendBe32(std::vector<unsigned char> &out, const std::uint32_t value) {
        out.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
        out.push_back(static_cast<unsigned char>(value & 0xff));
    }

    std::vector<unsigned char> PngChunk(const char *type, const std::vector<unsigned char> &data) {
        std::vector<unsigned char> chunk;
        AppendBe32(chunk, static_cast<std::uint32_t>(data.size()));
        chunk.insert(chunk.end(), type, type + 4);
        chunk.insert(chunk.end(), data.begin(), data.end());
        const auto crc = static_cast<std::uint32_t>(mz_crc32(0, chunk.data() + 4, 4u + data.size()));
        AppendBe32(chunk, crc);
        return chunk;
    }

    std::vector<unsigned char> EncodePng(const int width, const int height, const int channels, const std::vector<unsigned char> &raw) {
        std::vector<unsigned char> ihdr;
        AppendBe32(ihdr, static_cast<std::uint32_t>(width));
        AppendBe32(ihdr, static_cast<std::uint32_t>(height));
        ihdr.push_back(8);
        ihdr.push_back(channels == 4 ? 6 : 2);
        ihdr.push_back(0);
        ihdr.push_back(0);
        ihdr.push_back(0);

        mz_ulong compressedSize = mz_compressBound(static_cast<mz_ulong>(raw.size()));
        std::vector<unsigned char> compressed(compressedSize);
        REQUIRE(mz_compress(compressed.data(), &compressedSize, raw.data(), static_cast<mz_ulong>(raw.size())) == MZ_OK);
        compressed.resize(compressedSize);

        constexpr unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        std::vector<unsigned char> png(signature, signature + 8);
        const auto header = PngChunk("IHDR", ihdr);
        const auto idat = PngChunk("IDAT", compressed);
        const auto iend = PngChunk("IEND", {});
        png.insert(png.end(), header.begin(), header.end());
        png.insert(png.end(), idat.begin(), idat.end());
        png.insert(png.end(), iend.begin(), iend.end());
        return png;
    }

    unsigned char Paeth(const unsigned char left, const unsigned char up, const unsigned char upLeft) {
        const int estimate = static_cast<int>(left) + static_cast<int>(up) - static_cast<int>(upLeft);
        const int distanceLeft = std::abs(estimate - static_cast<int>(left));
        const int distanceUp = std::abs(estimate - static_cast<int>(up));
        const int distanceUpLeft = std::abs(estimate - static_cast<int>(upLeft));
        if (distanceLeft <= distanceUp && distanceLeft <= distanceUpLeft) {
            return left;
        }
        if (distanceUp <= distanceUpLeft) {
            return up;
        }
        return upLeft;
    }
}

TEST_CASE("DecodePng rejects a file that is not a PNG", "[png]") {
    const unsigned char bytes[] = {1, 2, 3, 4};
    REQUIRE_FALSE(DecodePng(bytes, sizeof(bytes)).has_value());
    REQUIRE_FALSE(DecodePng(nullptr, 0).has_value());
}

TEST_CASE("DecodePng restores RGBA rows stored with each filter", "[png]") {
    constexpr int width = 2;
    constexpr int height = 2;
    const unsigned char pixels[] = {
        10,
        20,
        30,
        40,
        50,
        60,
        70,
        80,
        90,
        100,
        110,
        120,
        130,
        140,
        150,
        160,
    };
    const unsigned char filters[] = {0, 1, 2, 3, 4};
    for (const unsigned char filter: filters) {
        std::vector<unsigned char> raw(static_cast<std::size_t>((1 + (width * 4)) * height));
        for (int y = 0; y < height; ++y) {
            unsigned char *row = raw.data() + (static_cast<std::size_t>(y) * (1 + (width * 4)));
            row[0] = filter;
            for (int i = 0; i < width * 4; ++i) {
                const unsigned char original = pixels[(y * width * 4) + i];
                const unsigned char left = i >= 4 ? pixels[(y * width * 4) + i - 4] : 0;
                const unsigned char up = y > 0 ? pixels[((y - 1) * width * 4) + i] : 0;
                const unsigned char upLeft = y > 0 && i >= 4 ? pixels[((y - 1) * width * 4) + i - 4] : 0;
                unsigned char stored = original;
                if (filter == 1) {
                    stored = static_cast<unsigned char>(original - left);
                } else if (filter == 2) {
                    stored = static_cast<unsigned char>(original - up);
                } else if (filter == 3) {
                    stored = static_cast<unsigned char>(original - static_cast<unsigned char>((static_cast<int>(left) + static_cast<int>(up)) / 2));
                } else if (filter == 4) {
                    stored = static_cast<unsigned char>(original - Paeth(left, up, upLeft));
                }
                row[1 + i] = stored;
            }
        }

        const auto encoded = EncodePng(width, height, 4, raw);
        const auto image = DecodePng(encoded.data(), encoded.size());
        REQUIRE(image.has_value());
        REQUIRE(image->Width == width);
        REQUIRE(image->Height == height);
        REQUIRE(image->Rgba.size() == sizeof(pixels));
        REQUIRE(std::memcmp(image->Rgba.data(), pixels, sizeof(pixels)) == 0);
    }
}

TEST_CASE("DecodePng expands 8-bit RGB to opaque RGBA", "[png]") {
    const unsigned char raw[] = {0, 1, 2, 3, 4, 5, 6};
    const auto encoded = EncodePng(2, 1, 3, std::vector<unsigned char>(raw, raw + sizeof(raw)));
    const auto image = DecodePng(encoded.data(), encoded.size());
    REQUIRE(image.has_value());
    const unsigned char expected[] = {1, 2, 3, 255, 4, 5, 6, 255};
    REQUIRE(std::memcmp(image->Rgba.data(), expected, sizeof(expected)) == 0);
}

TEST_CASE("DecodePngFile reads the shipped window icon", "[png]") {
    const auto path = std::filesystem::path(COREDECK_SOURCE_DIR) / "assets" / "icons" / "icon.png";
    const auto image = DecodePngFile(path.string());
    REQUIRE(image.has_value());
    REQUIRE(image->Width == 1024);
    REQUIRE(image->Height == 1024);
    REQUIRE(image->Rgba.size() == 1024u * 1024u * 4u);
}

TEST_CASE("hicolor icons use the application id and decode at their directory size", "[png][linux]") {
    const auto root = std::filesystem::path(COREDECK_SOURCE_DIR) / "packaging" / "linux" / "hicolor";
    for (const int size: {16, 32, 48, 64, 128, 256, 512, 1024}) {
        const auto path = root / (std::to_string(size) + "x" + std::to_string(size)) / "apps" / "com.devmuaz.coredeck.png";
        const auto image = DecodePngFile(path.string());
        REQUIRE(image.has_value());
        REQUIRE(image->Width == size);
        REQUIRE(image->Height == size);
    }
}

TEST_CASE("Linux desktop entry matches the macOS bundle id", "[linux][desktop]") {
    const auto path = std::filesystem::path(COREDECK_SOURCE_DIR) / "packaging" / "linux" / "com.devmuaz.coredeck.desktop.in";
    std::ifstream input(path);
    REQUIRE(input);
    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    REQUIRE(text.find("Exec=coredeck\n") != std::string::npos);
    REQUIRE(text.find("Icon=@APP_BUNDLE_ID@\n") != std::string::npos);
    REQUIRE(text.find("StartupWMClass=@APP_BUNDLE_ID@\n") != std::string::npos);
}
