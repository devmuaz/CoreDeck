//
// Created by AbdulMuaz Aqeel on 07/11/2026.
//

#include "png.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include <miniz.h>

namespace CoreDeck::Platform {
    namespace {
        constexpr unsigned char PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        constexpr int MAX_DIMENSION = 8192;

        std::uint32_t ReadBe32(const unsigned char *data) {
            return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) | (static_cast<std::uint32_t>(data[2]) << 8) |
                   static_cast<std::uint32_t>(data[3]);
        }

        bool TypeIs(const unsigned char *type, const char *name) {
            return std::memcmp(type, name, 4) == 0;
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

        bool Unfilter(unsigned char *raw, const int width, const int height, const int channels) {
            const std::size_t stride = static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
            const std::size_t rowBytes = stride + 1;
            for (int y = 0; y < height; ++y) {
                unsigned char *row = raw + (static_cast<std::size_t>(y) * rowBytes);
                unsigned char *pixels = row + 1;
                const unsigned char *previous = y > 0 ? raw + ((static_cast<std::size_t>(y - 1) * rowBytes) + 1) : nullptr;
                const unsigned char filter = row[0];
                switch (filter) {
                    case 0:
                        break;
                    case 1:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const unsigned char left = i >= static_cast<std::size_t>(channels) ? pixels[i - channels] : 0;
                            pixels[i] = static_cast<unsigned char>(pixels[i] + left);
                        }
                        break;
                    case 2:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const unsigned char up = previous != nullptr ? previous[i] : 0;
                            pixels[i] = static_cast<unsigned char>(pixels[i] + up);
                        }
                        break;
                    case 3:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const unsigned char left = i >= static_cast<std::size_t>(channels) ? pixels[i - channels] : 0;
                            const unsigned char up = previous != nullptr ? previous[i] : 0;
                            pixels[i] = static_cast<unsigned char>(pixels[i] + static_cast<unsigned char>((static_cast<int>(left) + static_cast<int>(up)) / 2));
                        }
                        break;
                    case 4:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const unsigned char left = i >= static_cast<std::size_t>(channels) ? pixels[i - channels] : 0;
                            const unsigned char up = previous != nullptr ? previous[i] : 0;
                            const unsigned char upLeft =
                                previous != nullptr && i >= static_cast<std::size_t>(channels) ? previous[i - channels] : 0;
                            pixels[i] = static_cast<unsigned char>(pixels[i] + Paeth(left, up, upLeft));
                        }
                        break;
                    default:
                        return false;
                }
            }
            return true;
        }
    }

    std::optional<PngImage> DecodePng(const unsigned char *data, const std::size_t size) {
        if (data == nullptr || size < 8 || std::memcmp(data, PNG_SIGNATURE, 8) != 0) {
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        bool sawHeader = false;
        bool sawEnd = false;
        std::vector<unsigned char> idat;
        std::size_t offset = 8;
        while (offset + 12 <= size) {
            const std::uint32_t length = ReadBe32(data + offset);
            if (length > size - offset - 12) {
                return std::nullopt;
            }
            const unsigned char *type = data + offset + 4;
            const unsigned char *chunk = type + 4;
            const std::uint32_t expectedCrc = ReadBe32(chunk + length);
            const auto actualCrc = static_cast<std::uint32_t>(mz_crc32(0, type, 4u + length));
            if (expectedCrc != actualCrc) {
                return std::nullopt;
            }

            if (TypeIs(type, "IHDR")) {
                if (sawHeader || length != 13 || offset != 8) {
                    return std::nullopt;
                }
                const std::uint32_t headerWidth = ReadBe32(chunk);
                const std::uint32_t headerHeight = ReadBe32(chunk + 4);
                const unsigned char bitDepth = chunk[8];
                const unsigned char colorType = chunk[9];
                const unsigned char compression = chunk[10];
                const unsigned char filter = chunk[11];
                const unsigned char interlace = chunk[12];
                if (headerWidth == 0 || headerHeight == 0 || headerWidth > MAX_DIMENSION || headerHeight > MAX_DIMENSION) {
                    return std::nullopt;
                }
                if (bitDepth != 8 || compression != 0 || filter != 0 || interlace != 0) {
                    return std::nullopt;
                }
                if (colorType == 2) {
                    channels = 3;
                } else if (colorType == 6) {
                    channels = 4;
                } else {
                    return std::nullopt;
                }
                width = static_cast<int>(headerWidth);
                height = static_cast<int>(headerHeight);
                sawHeader = true;
            } else if (TypeIs(type, "IDAT")) {
                if (!sawHeader) {
                    return std::nullopt;
                }
                idat.insert(idat.end(), chunk, chunk + length);
            } else if (TypeIs(type, "IEND")) {
                sawEnd = true;
                break;
            } else if ((type[0] & 0x20) == 0) {
                return std::nullopt;
            }

            offset += 12u + length;
        }
        if (!sawHeader || !sawEnd || idat.empty()) {
            return std::nullopt;
        }

        const std::size_t stride = static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
        const std::size_t rowBytes = stride + 1;
        const std::size_t rawSize = rowBytes * static_cast<std::size_t>(height);
        std::vector<unsigned char> raw(rawSize);
        mz_ulong destLen = static_cast<mz_ulong>(rawSize);
        if (mz_uncompress(raw.data(), &destLen, idat.data(), static_cast<mz_ulong>(idat.size())) != MZ_OK ||
            destLen != rawSize) {
            return std::nullopt;
        }
        if (!Unfilter(raw.data(), width, height, channels)) {
            return std::nullopt;
        }

        PngImage image;
        image.Width = width;
        image.Height = height;
        image.Rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (int y = 0; y < height; ++y) {
            const unsigned char *row = raw.data() + (static_cast<std::size_t>(y) * rowBytes) + 1;
            unsigned char *dest = image.Rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u);
            if (channels == 4) {
                std::memcpy(dest, row, stride);
            } else {
                for (int x = 0; x < width; ++x) {
                    dest[static_cast<ptrdiff_t>(x * 4)] = row[static_cast<ptrdiff_t>(x * 3)];
                    dest[static_cast<ptrdiff_t>((x * 4) + 1)] = row[static_cast<ptrdiff_t>((x * 3) + 1)];
                    dest[static_cast<ptrdiff_t>((x * 4) + 2)] = row[static_cast<ptrdiff_t>((x * 3) + 2)];
                    dest[static_cast<ptrdiff_t>((x * 4) + 3)] = 255;
                }
            }
        }
        return image;
    }

    std::optional<PngImage> DecodePngFile(const std::string &path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return std::nullopt;
        }
        input.seekg(0, std::ios::end);
        const auto end = input.tellg();
        if (end < 0) {
            return std::nullopt;
        }
        input.seekg(0, std::ios::beg);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
        if (!bytes.empty()) {
            input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!input) {
                return std::nullopt;
            }
        }
        return DecodePng(bytes.data(), bytes.size());
    }
}
