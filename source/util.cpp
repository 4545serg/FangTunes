#include "types.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

std::string format_time(double seconds) {
    if (seconds < 0.0 || seconds != seconds) seconds = 0.0;
    int total = (int)(seconds + 0.5);
    int m = total / 60;
    int s = total % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return std::string(buf);
}

std::string base_name_no_ext(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.resize(dot);
    return name;
}

std::string to_lower_ascii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        unsigned char u = (unsigned char)c;
        if (u >= 'A' && u <= 'Z') c = (char)(u - 'A' + 'a');
    }
    return out;
}

std::string trim_text(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && ((unsigned char)s[a] <= 0x20)) ++a;
    while (b > a && ((unsigned char)s[b - 1] <= 0x20)) --b;
    return s.substr(a, b - a);
}

std::string utf8_fit(const std::string& s, size_t maxChars) {
    if (s.size() <= maxChars) return s;
    // Do not split a UTF-8 sequence.
    size_t count = 0;
    size_t cut = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80) {
            if (count == maxChars) {
                cut = i;
                break;
            }
            ++count;
        }
        cut = i + 1;
    }
    return s.substr(0, cut);
}

static void append_utf8(std::string& out, uint32_t cp) {
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

std::string latin1_to_utf8(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if (c == 0) break;
        append_utf8(out, c);
    }
    return out;
}
