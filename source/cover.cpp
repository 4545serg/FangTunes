#include "cover.h"

#include <3ds.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

const int kTextureSize = 256;

bool read_cover_bytes(const Song& song, std::vector<uint8_t>& out) {
    if (!song.coverInline.empty()) {
        out = song.coverInline;
        return true;
    }
    if (song.coverSize == 0 || song.coverOffset == 0) return false;

    FILE* f = std::fopen(song.path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, (long)song.coverOffset, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    out.resize(song.coverSize);
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got == 0) return false;
    out.resize(got);
    return true;
}

void resize_bilinear(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    float sx = (float)sw / (float)dw;
    float sy = (float)sh / (float)dh;

    for (int y = 0; y < dh; ++y) {
        float fy = ((float)y + 0.5f) * sy - 0.5f;
        if (fy < 0.0f) fy = 0.0f;
        int y0 = (int)fy;
        int y1 = y0 + 1;
        if (y1 >= sh) y1 = sh - 1;
        float wy = fy - (float)y0;
        if (y0 >= sh) y0 = sh - 1;

        for (int x = 0; x < dw; ++x) {
            float fx = ((float)x + 0.5f) * sx - 0.5f;
            if (fx < 0.0f) fx = 0.0f;
            int x0 = (int)fx;
            int x1 = x0 + 1;
            if (x1 >= sw) x1 = sw - 1;
            float wx = fx - (float)x0;
            if (x0 >= sw) x0 = sw - 1;

            const uint8_t* p00 = src + ((size_t)y0 * sw + x0) * 4;
            const uint8_t* p10 = src + ((size_t)y0 * sw + x1) * 4;
            const uint8_t* p01 = src + ((size_t)y1 * sw + x0) * 4;
            const uint8_t* p11 = src + ((size_t)y1 * sw + x1) * 4;

            uint8_t* d = dst + ((size_t)y * dw + x) * 4;
            for (int c = 0; c < 4; ++c) {
                float top = p00[c] + (p10[c] - p00[c]) * wx;
                float bot = p01[c] + (p11[c] - p01[c]) * wx;
                float v = top + (bot - top) * wy;
                if (v < 0.0f) v = 0.0f;
                if (v > 255.0f) v = 255.0f;
                d[c] = (uint8_t)(v + 0.5f);
            }
        }
    }
}

void swizzle_tile(u32* p) {
    static const unsigned char table[12][4] = {
        {2, 8, 16, 4},   {3, 9, 17, 5},   {6, 10, 24, 20}, {7, 11, 25, 21},
        {14, 26, 28, 22}, {15, 27, 29, 23}, {34, 40, 48, 36}, {35, 41, 49, 37},
        {38, 42, 56, 52}, {39, 43, 57, 53}, {46, 58, 60, 54}, {47, 59, 61, 55},
    };

    for (int i = 0; i < 12; ++i) {
        const unsigned char* e = table[i];
        u32 tmp = p[e[0]];
        p[e[0]] = p[e[1]];
        p[e[1]] = p[e[2]];
        p[e[2]] = p[e[3]];
        p[e[3]] = tmp;
    }

    u32 t;
    t = p[12]; p[12] = p[18]; p[18] = t;
    t = p[13]; p[13] = p[19]; p[19] = t;
    t = p[44]; p[44] = p[50]; p[50] = t;
    t = p[45]; p[45] = p[51]; p[51] = t;
}

void swizzle_image(u32* pixels, int width, int height) {
    u32 tile[64];
    for (int y = 0; y < height; y += 8) {
        for (int x = 0; x < width; x += 8) {
            for (int ty = 0; ty < 8; ++ty)
                for (int tx = 0; tx < 8; ++tx)
                    tile[ty * 8 + tx] = pixels[(size_t)(y + ty) * width + (x + tx)];

            swizzle_tile(tile);

            for (int ty = 0; ty < 8; ++ty)
                for (int tx = 0; tx < 8; ++tx)
                    pixels[(size_t)(y + ty) * width + (x + tx)] = tile[ty * 8 + tx];
        }
    }
}

}  // namespace

bool cover_load(const Song& song, C3D_Tex& tex, Tex3DS_SubTexture& sub, C2D_Image& image) {
    std::vector<uint8_t> bytes;
    if (!read_cover_bytes(song, bytes)) return false;

    int w = 0, h = 0, n = 0;
    unsigned char* decoded = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!decoded || w <= 0 || h <= 0) {
        if (decoded) stbi_image_free(decoded);
        return false;
    }

    std::vector<uint8_t> scaled((size_t)kTextureSize * kTextureSize * 4);
    resize_bilinear(decoded, w, h, scaled.data(), kTextureSize, kTextureSize);
    stbi_image_free(decoded);

    std::vector<u32> pixels((size_t)kTextureSize * kTextureSize);
    std::memcpy(pixels.data(), scaled.data(), pixels.size() * sizeof(u32));
    swizzle_image(pixels.data(), kTextureSize, kTextureSize);

    if (!C3D_TexInit(&tex, kTextureSize, kTextureSize, GPU_RGBA8)) return false;
    C3D_TexUpload(&tex, pixels.data());
    C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    sub.width = kTextureSize;
    sub.height = kTextureSize;
    sub.left = 0.0f;
    sub.top = 1.0f;
    sub.right = 1.0f;
    sub.bottom = 0.0f;

    image.tex = &tex;
    image.subtex = &sub;
    return true;
}

void cover_free(C3D_Tex& tex) {
    if (tex.data) {
        C3D_TexDelete(&tex);
        std::memset(&tex, 0, sizeof(tex));
    }
}
