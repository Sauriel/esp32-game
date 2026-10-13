#include "PlayerPng.h"
#include <miniz.h>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace PlayerPng {
namespace {
constexpr unsigned width = 144, height = 192;
uint32_t be32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
uint32_t crcUpdate(uint32_t crc, const uint8_t* p, size_t n) {
    while (n--) {
        crc ^= *p++;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
    }
    return crc;
}
int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int da = abs(p - a), db = abs(p - b), dc = abs(p - c);
    return da <= db && da <= dc ? a : (db <= dc ? b : c);
}
struct Decoder {
    void* context;
    Read read;
    WriteRow write;
    tinfl_decompressor inflater{};
    uint8_t dictionary[TINFL_LZ_DICT_SIZE];
    uint8_t previous[width * 4]{};
    uint8_t row[width * 4 + 1];
    uint8_t argb[width * 2];
    uint8_t palette[256][4]{};
    uint8_t input[512];
    size_t outputOffset = 0, rowOffset = 0, rowBytes = 0;
    unsigned y = 0, channels = 0, depth = 0, type = 0, paletteSize = 0;
    uint16_t transparent[3]{};
    bool hasTransparency = false, done = false;

    bool exact(uint8_t* p, size_t n) {
        while (n) {
            const int count = read(context, p, n);
            if (count <= 0 || size_t(count) > n) return false;
            p += count; n -= count;
        }
        return true;
    }
    bool emitRow() {
        const unsigned filter = row[0];
        if (filter > 4 || y >= height) return false;
        const unsigned bpp = (channels * depth + 7) / 8;
        auto* pixels = row + 1;
        for (size_t i = 0; i < rowBytes; ++i) {
            const int a = i >= bpp ? pixels[i - bpp] : 0;
            const int b = previous[i];
            const int c = i >= bpp ? previous[i - bpp] : 0;
            const int prediction = filter == 1 ? a : filter == 2 ? b :
                filter == 3 ? (a + b) / 2 : filter == 4 ? paeth(a, b, c) : 0;
            pixels[i] = uint8_t(pixels[i] + prediction);
        }
        for (unsigned x = 0; x < width; ++x) {
            unsigned r, g, b, a = 255;
            if (type == 3) {
                const unsigned index = (pixels[x * depth / 8] >> (8 - depth - (x * depth % 8))) & ((1U << depth) - 1);
                if (index >= paletteSize) return false;
                r = palette[index][0]; g = palette[index][1]; b = palette[index][2]; a = palette[index][3];
            } else {
                const auto* p = pixels + x * channels;
                if (type == 0 || type == 4) {
                    r = g = b = p[0];
                    if (type == 4) a = p[1];
                    else if (hasTransparency && r == transparent[0]) a = 0;
                } else {
                    r = p[0]; g = p[1]; b = p[2];
                    if (type == 6) a = p[3];
                    else if (hasTransparency && r == transparent[0] && g == transparent[1] && b == transparent[2]) a = 0;
                }
            }
            const uint16_t value = (a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
            argb[x * 2] = uint8_t(value); argb[x * 2 + 1] = uint8_t(value >> 8);
        }
        if (!write(context, y++, argb, sizeof(argb))) return false;
        memcpy(previous, pixels, rowBytes);
        rowOffset = 0;
        return true;
    }
    bool inflate(const uint8_t* data, size_t length) {
        if (done) return length == 0;
        bool moreOutput = false;
        do {
            size_t consumed = length, produced = sizeof(dictionary) - outputOffset;
            const auto status = tinfl_decompress(&inflater, data, &consumed, dictionary,
                dictionary + outputOffset, &produced,
                TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_HAS_MORE_INPUT);
            if (status < 0) return false;
            for (size_t i = 0; i < produced; ++i) {
                if (y >= height) return false;
                row[rowOffset++] = dictionary[outputOffset + i];
                if (rowOffset == rowBytes + 1 && !emitRow()) return false;
            }
            outputOffset = (outputOffset + produced) % sizeof(dictionary);
            data += consumed; length -= consumed;
            if (status == TINFL_STATUS_DONE) { done = true; return length == 0; }
            if (!consumed && !produced) return false;
            moreOutput = status == TINFL_STATUS_HAS_MORE_OUTPUT;
        } while (length || moreOutput);
        return true;
    }
    bool run(const char*& error) {
        uint8_t header[13];
        const uint8_t signature[] = {137,80,78,71,13,10,26,10};
        if (!exact(header, 8) || memcmp(header, signature, 8)) return false;
        bool ihdr = false, idat = false, idatEnded = false, plte = false, trns = false;
        // Datei-/Chunk-Limit verhindert unbegrenzte Ladephasen und grosse Allokationen.
        size_t total = 8;
        for (unsigned chunks = 0; chunks < 4096; ++chunks) {
            if (!exact(header, 8)) return false;
            const uint32_t length = be32(header);
            if (length > 256 * 1024 || total + length + 12 > 256 * 1024) return false;
            total += length + 12;
            const bool isHeader = !memcmp(header + 4, "IHDR", 4);
            const bool isData = !memcmp(header + 4, "IDAT", 4);
            const bool isPalette = !memcmp(header + 4, "PLTE", 4);
            const bool isTransparency = !memcmp(header + 4, "tRNS", 4);
            const bool isEnd = !memcmp(header + 4, "IEND", 4);
            if (!ihdr && !isHeader) return false;
            if (isHeader && (ihdr || length != 13)) return false;
            if (isData && (idatEnded || (type == 3 && !plte))) return false;
            if (!isData && idat) idatEnded = true;
            if (isPalette && (plte || idat || !length || length > 768 || length % 3)) return false;
            if (isTransparency && (trns || idat || (type == 3 ? !plte || length > paletteSize :
                    type == 0 ? length != 2 : type == 2 ? length != 6 : true))) return false;
            if (isEnd && (length || !idat)) return false;
            if (!isHeader && !isData && !isPalette && !isTransparency && !isEnd && !(header[4] & 32)) {
                error = "PNG: unbekannter Pflicht-Chunk"; return false;
            }
            uint32_t crc = crcUpdate(0xffffffff, header + 4, 4);
            for (uint32_t offset = 0; offset < length;) {
                const size_t n = length - offset < sizeof(input) ? length - offset : sizeof(input);
                if (!exact(input, n)) return false;
                crc = crcUpdate(crc, input, n);
                if (isHeader) {
                    if (be32(input) != width || be32(input + 4) != height) {
                        error = "player.png muss 144 x 192 sein"; return false;
                    }
                    depth = input[8]; type = input[9];
                    channels = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : type == 6 ? 4 : 0;
                    if (!channels || (type == 3 ? depth != 1 && depth != 2 && depth != 4 && depth != 8 : depth != 8) || input[10] || input[11] || input[12]) {
                        error = "PNG: 8 Bit, ohne Interlace exportieren"; return false;
                    }
                    rowBytes = (width * channels * depth + 7) / 8;
                    ihdr = true;
                } else if (isPalette) {
                    for (size_t i = 0; i < n; ++i) palette[(offset + i) / 3][(offset + i) % 3] = input[i];
                } else if (isTransparency) {
                    if (type == 3) {
                        for (size_t i = 0; i < n; ++i) palette[offset + i][3] = input[i];
                    } else {
                        for (size_t i = 0; i < n / 2; ++i) transparent[i] = uint16_t(input[i * 2]) << 8 | input[i * 2 + 1];
                        hasTransparency = true;
                    }
                } else if (isData && !inflate(input, n)) return false;
                offset += n;
            }
            uint8_t checksum[4];
            if (!exact(checksum, 4) || be32(checksum) != (crc ^ 0xffffffff)) return false;
            if (isPalette) {
                paletteSize = length / 3; plte = true;
                for (unsigned i = 0; i < paletteSize; ++i) palette[i][3] = 255;
            }
            if (isTransparency) trns = true;
            if (isData) idat = true;
            if (isEnd) {
                uint8_t extra;
                return done && y == height && rowOffset == 0 && read(context, &extra, 1) == 0;
            }
        }
        return false;
    }
};
static_assert(sizeof(Decoder) <= 48 * 1024, "PNG workspace exceeds the budget");
}
size_t workspaceBytes() { return sizeof(Decoder); }
bool decode(void* context, Read read, WriteRow write, const char*& error) {
    error = "PNG beschaedigt oder unvollstaendig";
    // Ein begrenzter temporärer Heapblock statt grossem Task-Stack/Framebuffer.
    std::unique_ptr<Decoder> decoder(new (std::nothrow) Decoder{});
    if (!decoder) { error = "Nicht genug Heap fuer PNG-Lader"; return false; }
    decoder->context = context; decoder->read = read; decoder->write = write;
    tinfl_init(&decoder->inflater);
    const bool result = decoder->run(error);
    if (result) error = nullptr;
    return result;
}
}
