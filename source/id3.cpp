#include "id3.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <string>
#include <vector>

namespace {

const size_t kMaxTagBytes = 4 * 1024 * 1024;
const size_t kProbeBytes = 64 * 1024;

uint32_t syncsafe32(const uint8_t* p) {
    return ((uint32_t)(p[0] & 0x7F) << 21) |
           ((uint32_t)(p[1] & 0x7F) << 14) |
           ((uint32_t)(p[2] & 0x7F) << 7) |
           (uint32_t)(p[3] & 0x7F);
}

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back((char)cp);
    } else if (cp <= 0x7FF) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

std::vector<uint8_t> deunsync(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(data[i]);
        if (data[i] == 0xFF && i + 1 < len && data[i + 1] == 0x00) ++i;
    }
    return out;
}

bool decode_text_frame(const uint8_t* d, size_t n, std::string& out) {
    out.clear();
    if (n == 0) return false;

    uint8_t enc = d[0];
    if (enc == 0 || enc > 3) {
        out = latin1_to_utf8(d + 1, n - 1);
    } else if (enc == 3) {
        size_t len = 1;
        while (len < n && d[len] != 0) ++len;
        out.assign((const char*)d + 1, len - 1);
    } else {
        // UTF-16 with or without BOM.
        size_t p = 1;
        bool little = (enc == 1);
        if (enc == 1 && n >= 3) {
            if (d[1] == 0xFF && d[2] == 0xFE) {
                little = true;
                p = 3;
            } else if (d[1] == 0xFE && d[2] == 0xFF) {
                little = false;
                p = 3;
            }
        } else if (enc == 2) {
            little = false;
        }

        while (p + 1 < n) {
            uint16_t u = little ? (uint16_t)(d[p] | (d[p + 1] << 8))
                                : (uint16_t)((d[p] << 8) | d[p + 1]);
            p += 2;
            if (u == 0) break;
            uint32_t cp = u;
            if (u >= 0xD800 && u <= 0xDBFF && p + 1 < n) {
                uint16_t lo = little ? (uint16_t)(d[p] | (d[p + 1] << 8))
                                     : (uint16_t)((d[p] << 8) | d[p + 1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    p += 2;
                    cp = 0x10000u + (((uint32_t)u - 0xD800u) << 10) + ((uint32_t)lo - 0xDC00u);
                } else {
                    cp = 0xFFFD;
                }
            } else if (u >= 0xDC00 && u <= 0xDFFF) {
                cp = 0xFFFD;
            }
            append_utf8(out, cp);
        }
    }

    out = trim_text(out);
    return !out.empty();
}

size_t skip_description(const uint8_t* d, size_t n, size_t p, uint8_t enc) {
    if (enc == 1 || enc == 2) {
        while (p + 1 < n) {
            if (d[p] == 0 && d[p + 1] == 0) return p + 2;
            p += 2;
        }
        return n;
    }
    while (p < n && d[p] != 0) ++p;
    return (p < n) ? p + 1 : n;
}

bool parse_apic(const uint8_t* d, size_t n, bool v22, bool canSeek,
                uint64_t contentFileOffset, Song& song) {
    if (n < 4) return false;

    size_t p = 0;
    uint8_t enc = d[p++];
    std::string mime;
    uint8_t picType = 0;
    char fmt[4] = {0, 0, 0, 0};

    if (v22) {
        if (p + 4 > n) return false;
        fmt[0] = (char)d[p++];
        fmt[1] = (char)d[p++];
        fmt[2] = (char)d[p++];
        picType = d[p++];
        if (strcasecmp(fmt, "JPG") == 0 || strcasecmp(fmt, "JPEG") == 0)
            mime = "image/jpeg";
        else if (strcasecmp(fmt, "PNG") == 0)
            mime = "image/png";
    } else {
        size_t mimeStart = p;
        while (p < n && d[p] != 0) ++p;
        mime.assign((const char*)d + mimeStart, p - mimeStart);
        if (p < n) ++p;
        if (p >= n) return false;
        picType = d[p++];
    }

    p = skip_description(d, n, p, enc);
    if (p >= n) return false;

    size_t imageLen = n - p;
    if (imageLen < 16) return false;

    if (mime.empty()) {
        if (imageLen >= 3 && d[p] == 0xFF && d[p + 1] == 0xD8 && d[p + 2] == 0xFF)
            mime = "image/jpeg";
        else if (imageLen >= 8 && d[p] == 0x89 && d[p + 1] == 'P' && d[p + 2] == 'N' &&
                 d[p + 3] == 'G')
            mime = "image/png";
    }

    // Keep a front cover if we already have one; otherwise accept any picture.
    if (song.hasCover() && picType != 3) return true;

    if (canSeek) {
        song.coverOffset = contentFileOffset + p;
        song.coverSize = (uint32_t)imageLen;
        song.coverInline.clear();
    } else {
        song.coverOffset = 0;
        song.coverSize = 0;
        song.coverInline.assign(d + p, d + n);
    }
    song.coverMime = mime;
    return true;
}

void set_if_empty(std::string& dst, const std::string& value) {
    if (dst.empty() && !value.empty()) dst = value;
}

void read_id3v1(FILE* f, Song& song) {
    if (fseek(f, -128, SEEK_END) != 0) return;
    uint8_t tag[128];
    if (fread(tag, 1, sizeof(tag), f) != sizeof(tag)) return;
    if (std::memcmp(tag, "TAG", 3) != 0) return;

    set_if_empty(song.title, latin1_to_utf8(tag + 3, 30));
    set_if_empty(song.artist, latin1_to_utf8(tag + 33, 30));
    set_if_empty(song.album, latin1_to_utf8(tag + 63, 30));
    set_if_empty(song.year, latin1_to_utf8(tag + 93, 4));

    if (tag[125] == 0 && tag[126] != 0 && song.track == 0)
        song.track = tag[126];
}

struct Mp3Header {
    int version = 0;  // 1, 2 or 25
    int layer = 3;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 2;
    int frameLen = 0;
};

bool parse_mp3_header(const uint8_t* h, Mp3Header& out) {
    if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return false;

    int verBits = (h[1] >> 3) & 3;
    if (verBits == 1) return false;
    int layerBits = (h[1] >> 1) & 3;
    if (layerBits == 0) return false;
    int brIdx = (h[2] >> 4) & 0xF;
    if (brIdx == 0 || brIdx == 15) return false;
    int srIdx = (h[2] >> 2) & 3;
    if (srIdx == 3) return false;
    int pad = (h[2] >> 1) & 1;
    int mode = (h[3] >> 6) & 3;

    int version = (verBits == 3) ? 1 : (verBits == 2 ? 2 : 25);
    int layer = 4 - layerBits;

    static const int brMpeg1[16] = {0, 32, 40, 48, 56, 64, 80, 96,
                                    112, 128, 160, 192, 224, 256, 320, 0};
    static const int brMpeg2[16] = {0, 8, 16, 24, 32, 40, 48, 56,
                                    64, 80, 96, 112, 128, 144, 160, 0};
    static const int sr1[3] = {44100, 48000, 32000};
    static const int sr2[3] = {22050, 24000, 16000};
    static const int sr25[3] = {11025, 12000, 8000};

    int bitrate = (version == 1) ? brMpeg1[brIdx] : brMpeg2[brIdx];
    int sampleRate = (version == 1) ? sr1[srIdx] : (version == 2 ? sr2[srIdx] : sr25[srIdx]);
    if (bitrate == 0 || sampleRate == 0) return false;

    int frameLen = 0;
    if (layer == 1)
        frameLen = (12 * bitrate * 1000 / sampleRate + pad) * 4;
    else if (layer == 2)
        frameLen = 144 * bitrate * 1000 / sampleRate + pad;
    else
        frameLen = (version == 1 ? 144 : 72) * bitrate * 1000 / sampleRate + pad;

    if (frameLen < 4) return false;

    out.version = version;
    out.layer = layer;
    out.bitrate = bitrate;
    out.sampleRate = sampleRate;
    out.channels = (mode == 3) ? 1 : 2;
    out.frameLen = frameLen;
    return true;
}

}  // namespace

bool id3_read_metadata(const char* path, Song& song) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;

    uint8_t hdr[10];
    if (std::fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        std::fclose(f);
        return true;
    }

    if (std::memcmp(hdr, "ID3", 3) == 0) {
        int version = hdr[3];
        bool tagUnsync = (hdr[5] & 0x80) != 0;
        size_t tagSize = syncsafe32(hdr + 6);
        size_t readSize = std::min(tagSize, kMaxTagBytes);

        std::vector<uint8_t> data(readSize);
        if (readSize > 0) {
            size_t got = std::fread(data.data(), 1, readSize, f);
            data.resize(got);
        }

        if (tagUnsync) data = deunsync(data.data(), data.size());

        size_t pos = 0;
        if (!tagUnsync && readSize == tagSize) {
            if (version == 3 && (hdr[5] & 0x40) && data.size() >= 4)
                pos = 4 + be32(data.data());
            else if (version == 4 && (hdr[5] & 0x40) && data.size() >= 4)
                pos = syncsafe32(data.data());
        }

        size_t frameHeaderLen = (version == 2) ? 6 : 10;
        while (pos + frameHeaderLen <= data.size()) {
            const uint8_t* fh = data.data() + pos;
            char id[5] = {0, 0, 0, 0, 0};
            size_t frameSize = 0;
            uint16_t frameFlags = 0;

            if (version == 2) {
                std::memcpy(id, fh, 3);
                frameSize = ((size_t)fh[3] << 16) | ((size_t)fh[4] << 8) | fh[5];
            } else {
                std::memcpy(id, fh, 4);
                frameSize = (version == 4) ? syncsafe32(fh + 4) : be32(fh + 4);
                frameFlags = (uint16_t)(((uint16_t)fh[8] << 8) | fh[9]);
            }

            if (id[0] == 0) break;
            if (frameSize == 0 || pos + frameHeaderLen + frameSize > data.size()) break;

            const uint8_t* content = data.data() + pos + frameHeaderLen;
            uint64_t contentFileOffset = 10 + pos + frameHeaderLen;
            size_t contentSize = frameSize;

            std::vector<uint8_t> frameCopy;
            bool frameUnsync = (version == 4) && (frameFlags & 0x0002);
            if (frameUnsync) {
                frameCopy = deunsync(content, contentSize);
                content = frameCopy.data();
                contentSize = frameCopy.size();
            }

            // v2.4 optional data-length indicator.
            if (version == 4 && (frameFlags & 0x0001) && contentSize >= 4) {
                content += 4;
                contentSize -= 4;
                contentFileOffset += 4;
            }

            std::string value;
            bool isText = false;
            bool textOk = false;

            if (std::strcmp(id, "TIT2") == 0 || std::strcmp(id, "TT2") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk) set_if_empty(song.title, value);
            } else if (std::strcmp(id, "TPE1") == 0 || std::strcmp(id, "TP1") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk) set_if_empty(song.artist, value);
            } else if (std::strcmp(id, "TPE2") == 0 || std::strcmp(id, "TP2") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk && song.artist.empty()) song.artist = value;
            } else if (std::strcmp(id, "TALB") == 0 || std::strcmp(id, "TAL") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk) set_if_empty(song.album, value);
            } else if (std::strcmp(id, "TCON") == 0 || std::strcmp(id, "TCO") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk) set_if_empty(song.genre, value);
            } else if (std::strcmp(id, "TDRC") == 0 || std::strcmp(id, "TYER") == 0 ||
                       std::strcmp(id, "TYE") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk) set_if_empty(song.year, value.substr(0, 4));
            } else if (std::strcmp(id, "TRCK") == 0 || std::strcmp(id, "TRK") == 0) {
                isText = true;
                textOk = decode_text_frame(content, contentSize, value);
                if (textOk && song.track == 0) song.track = std::atoi(value.c_str());
            } else if (std::strcmp(id, "APIC") == 0) {
                parse_apic(content, contentSize, false, !tagUnsync && !frameUnsync,
                           contentFileOffset, song);
            } else if (std::strcmp(id, "PIC") == 0) {
                parse_apic(content, contentSize, true, !tagUnsync && !frameUnsync,
                           contentFileOffset, song);
            }

            (void)isText;
            (void)textOk;
            pos += frameHeaderLen + frameSize;
        }
    }

    read_id3v1(f, song);
    std::fclose(f);
    return true;
}

void mp3_probe(const char* path, Song& song) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return;

    long fileSize = 0;
    if (std::fseek(f, 0, SEEK_END) == 0) fileSize = std::ftell(f);
    if (fileSize <= 0) {
        std::fclose(f);
        return;
    }

    long audioStart = 0;
    uint8_t hdr[10];
    if (std::fseek(f, 0, SEEK_SET) == 0 && std::fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr) &&
        std::memcmp(hdr, "ID3", 3) == 0) {
        audioStart = 10 + (long)syncsafe32(hdr + 6);
        if (audioStart > fileSize) audioStart = 0;
    }

    if (std::fseek(f, audioStart, SEEK_SET) != 0) {
        std::fclose(f);
        return;
    }

    std::vector<uint8_t> data(kProbeBytes);
    size_t got = std::fread(data.data(), 1, data.size(), f);
    data.resize(got);

    for (size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i] != 0xFF || (data[i + 1] & 0xE0) != 0xE0) continue;
        Mp3Header h;
        if (!parse_mp3_header(data.data() + i, h)) continue;

        // Reject false sync words by checking that another frame follows.
        size_t next = i + (size_t)h.frameLen;
        if (next + 1 < data.size()) {
            if (data[next] != 0xFF || (data[next + 1] & 0xE0) != 0xE0) continue;
        }

        song.sampleRate = h.sampleRate;
        song.channels = h.channels;
        song.bitrate = h.bitrate;

        double bytes = (double)(fileSize - audioStart - (long)i);
        if (bytes > 0.0)
            song.duration = bytes * 8.0 / ((double)h.bitrate * 1000.0);
        break;
    }

    std::fclose(f);
}
