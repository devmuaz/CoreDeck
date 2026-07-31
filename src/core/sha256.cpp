//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "sha256.h"

namespace CoreDeck {
    namespace {
        constexpr size_t BLOCK_SIZE = 64;
        constexpr size_t DIGEST_SIZE = 32;
        constexpr size_t FILE_CHUNK_SIZE = static_cast<const size_t>(64 * 1024);

        constexpr std::array<uint32_t, 64> ROUND_CONSTANTS = {
            0x428a2f98,
            0x71374491,
            0xb5c0fbcf,
            0xe9b5dba5,
            0x3956c25b,
            0x59f111f1,
            0x923f82a4,
            0xab1c5ed5,
            0xd807aa98,
            0x12835b01,
            0x243185be,
            0x550c7dc3,
            0x72be5d74,
            0x80deb1fe,
            0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1,
            0xefbe4786,
            0x0fc19dc6,
            0x240ca1cc,
            0x2de92c6f,
            0x4a7484aa,
            0x5cb0a9dc,
            0x76f988da,
            0x983e5152,
            0xa831c66d,
            0xb00327c8,
            0xbf597fc7,
            0xc6e00bf3,
            0xd5a79147,
            0x06ca6351,
            0x14292967,
            0x27b70a85,
            0x2e1b2138,
            0x4d2c6dfc,
            0x53380d13,
            0x650a7354,
            0x766a0abb,
            0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1,
            0xa81a664b,
            0xc24b8b70,
            0xc76c51a3,
            0xd192e819,
            0xd6990624,
            0xf40e3585,
            0x106aa070,
            0x19a4c116,
            0x1e376c08,
            0x2748774c,
            0x34b0bcb5,
            0x391c0cb3,
            0x4ed8aa4a,
            0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee,
            0x78a5636f,
            0x84c87814,
            0x8cc70208,
            0x90befffa,
            0xa4506ceb,
            0xbef9a3f7,
            0xc67178f2,
        };

        constexpr uint32_t RotR(const uint32_t value, const uint32_t bits) {
            return (value >> bits) | (value << (32 - bits));
        }

        class Sha256Context {
        public:
            void Update(const uint8_t *data, size_t size) {
                m_BitCount += static_cast<uint64_t>(size) * 8;

                if (m_BufferUsed > 0) {
                    const size_t needed = BLOCK_SIZE - m_BufferUsed;
                    const size_t take = size < needed ? size : needed;
                    std::memcpy(m_Buffer.data() + m_BufferUsed, data, take);
                    m_BufferUsed += take;
                    data += take;
                    size -= take;

                    if (m_BufferUsed < BLOCK_SIZE) {
                        return;
                    }
                    m_Transform(m_Buffer.data());
                    m_BufferUsed = 0;
                }

                while (size >= BLOCK_SIZE) {
                    m_Transform(data);
                    data += BLOCK_SIZE;
                    size -= BLOCK_SIZE;
                }

                if (size > 0) {
                    std::memcpy(m_Buffer.data(), data, size);
                    m_BufferUsed = size;
                }
            }

            std::string Finish() {
                const uint64_t bitCount = m_BitCount;

                m_Buffer[m_BufferUsed++] = 0x80;
                if (m_BufferUsed > BLOCK_SIZE - 8) {
                    while (m_BufferUsed < BLOCK_SIZE) {
                        m_Buffer[m_BufferUsed++] = 0x00;
                    }
                    m_Transform(m_Buffer.data());
                    m_BufferUsed = 0;
                }
                while (m_BufferUsed < BLOCK_SIZE - 8) {
                    m_Buffer[m_BufferUsed++] = 0x00;
                }
                for (size_t i = 0; i < 8; ++i) {
                    m_Buffer[BLOCK_SIZE - 8 + i] = static_cast<uint8_t>(bitCount >> (56 - (i * 8)));
                }
                m_Transform(m_Buffer.data());

                static constexpr char HEX[] = "0123456789abcdef";
                std::string hex;
                hex.reserve(DIGEST_SIZE * 2);
                for (const uint32_t word: m_State) {
                    for (int shift = 24; shift >= 0; shift -= 8) {
                        const auto byte = static_cast<uint8_t>(word >> shift);
                        hex.push_back(HEX[byte >> 4]);
                        hex.push_back(HEX[byte & 0x0F]);
                    }
                }
                return hex;
            }

        private:
            void m_Transform(const uint8_t *block) {
                std::array<uint32_t, 64> schedule = {};
                for (size_t i = 0; i < 16; ++i) {
                    schedule[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                                  (static_cast<uint32_t>(block[(i * 4) + 1]) << 16) |
                                  (static_cast<uint32_t>(block[(i * 4) + 2]) << 8) |
                                  static_cast<uint32_t>(block[(i * 4) + 3]);
                }
                for (size_t i = 16; i < 64; ++i) {
                    const uint32_t s0 = RotR(schedule[i - 15], 7) ^ RotR(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3);
                    const uint32_t s1 = RotR(schedule[i - 2], 17) ^ RotR(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10);
                    schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
                }

                uint32_t a = m_State[0];
                uint32_t b = m_State[1];
                uint32_t c = m_State[2];
                uint32_t d = m_State[3];
                uint32_t e = m_State[4];
                uint32_t f = m_State[5];
                uint32_t g = m_State[6];
                uint32_t h = m_State[7];

                for (size_t i = 0; i < 64; ++i) {
                    const uint32_t s1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
                    const uint32_t ch = (e & f) ^ (~e & g);
                    const uint32_t temp1 = h + s1 + ch + ROUND_CONSTANTS[i] + schedule[i];
                    const uint32_t s0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
                    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                    const uint32_t temp2 = s0 + maj;

                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }

                m_State[0] += a;
                m_State[1] += b;
                m_State[2] += c;
                m_State[3] += d;
                m_State[4] += e;
                m_State[5] += f;
                m_State[6] += g;
                m_State[7] += h;
            }

            std::array<uint32_t, 8> m_State = {
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
            };
            std::array<uint8_t, BLOCK_SIZE> m_Buffer = {};
            size_t m_BufferUsed = 0;
            uint64_t m_BitCount = 0;
        };
    }

    std::string Sha256Hex(const void *data, const size_t size) {
        Sha256Context context;
        context.Update(static_cast<const uint8_t *>(data), size);
        return context.Finish();
    }

    std::string Sha256Hex(const std::string &data) {
        return Sha256Hex(data.data(), data.size());
    }

    std::string Sha256File(const std::string &path) {
        std::ifstream in(std::filesystem::path(path), std::ios::binary);
        if (!in) {
            return {};
        }

        Sha256Context context;
        std::vector<char> chunk(FILE_CHUNK_SIZE);
        while (in) {
            in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            const std::streamsize read = in.gcount();
            if (read > 0) {
                context.Update(reinterpret_cast<const uint8_t *>(chunk.data()), static_cast<size_t>(read));
            }
        }
        if (in.bad()) {
            return {};
        }
        return context.Finish();
    }

    bool EqualsIgnoreCaseHex(const std::string &a, const std::string &b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            const auto left = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            const auto right = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (left != right) {
                return false;
            }
        }
        return true;
    }
}
