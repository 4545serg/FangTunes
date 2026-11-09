#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One track in the library. Metadata is filled in by the ID3 scanner.
struct Song {
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string year;

    int track = 0;
    double duration = 0.0;   // seconds (estimated until playback starts)
    int sampleRate = 44100;
    int channels = 2;
    int bitrate = 0;         // kbps

    // Cover art is kept as a file reference so the scanner stays fast.
    // Inline data is only used for rare unsynchronised ID3 tags.
    uint64_t coverOffset = 0;
    uint32_t coverSize = 0;
    std::string coverMime;
    std::vector<uint8_t> coverInline;

    bool hasCover() const { return coverSize > 0 || !coverInline.empty(); }
};

std::string format_time(double seconds);
std::string base_name_no_ext(const std::string& path);
std::string to_lower_ascii(const std::string& s);
std::string trim_text(const std::string& s);
std::string utf8_fit(const std::string& s, size_t maxChars);
std::string latin1_to_utf8(const uint8_t* data, size_t len);
