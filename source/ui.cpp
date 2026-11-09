#include "ui.h"
#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

#define RGB(r, g, b) C2D_Color32((r), (g), (b), 255)
#define RGBA(r, g, b, a) C2D_Color32((r), (g), (b), (a))

const u32 kTopBgTop = RGBA(9, 12, 18, 255);
const u32 kTopBgBottom = RGBA(19, 25, 35, 255);
const u32 kTopText = RGBA(246, 248, 251, 255);
const u32 kTopMuted = RGBA(154, 162, 176, 255);
const u32 kTopDim = RGBA(101, 110, 126, 255);
const u32 kAccent = RGBA(46, 213, 115, 255);
const u32 kAccentDark = RGBA(10, 30, 20, 255);
const u32 kPanel = RGBA(27, 34, 46, 255);
const u32 kPanelLight = RGBA(40, 50, 66, 255);

const u32 kBottomBg = RGBA(245, 245, 247, 255);
const u32 kBottomText = RGBA(20, 20, 24, 255);
const u32 kBottomMuted = RGBA(124, 124, 130, 255);
const u32 kBottomLine = RGBA(224, 224, 229, 255);
const u32 kRowCurrent = RGBA(229, 248, 237, 255);
const u32 kRowSelected = RGBA(238, 238, 243, 255);
const u32 kWhite = RGBA(255, 255, 255, 255);
const u32 kPill = RGBA(236, 236, 241, 255);
const u32 kPillText = RGBA(60, 60, 67, 255);
const u32 kAccentPill = RGBA(220, 252, 231, 255);
const u32 kAccentPillText = RGBA(21, 128, 61, 255);

const float kListTop = 56.0f;
const float kListBottom = 200.0f;
const float kListHeight = kListBottom - kListTop;
const float kRowHeight = 38.0f;

enum TextAlign {
    AlignLeft = 0,
    AlignCenter = 1,
    AlignRight = 2,
};

u8 lerp_u8(u8 a, u8 b, float t) {
    float v = (float)a + ((float)b - (float)a) * t;
    if (v < 0.0f) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (u8)(v + 0.5f);
}

u32 lerp_color(u32 a, u32 b, float t) {
    u8 ar = (a >> 0) & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    u8 br = (b >> 0) & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ba = (b >> 24) & 0xFF;
    return C2D_Color32(lerp_u8(ar, br, t), lerp_u8(ag, bg, t), lerp_u8(ab, bb, t),
                       lerp_u8(aa, ba, t));
}

void rounded_rect(float x, float y, float w, float h, float r, u32 color, float z = 0.0f) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (r * 2.0f > w) r = w * 0.5f;
    if (r * 2.0f > h) r = h * 0.5f;

    C2D_DrawRectSolid(x + r, y, z, w - 2.0f * r, h, color);
    C2D_DrawRectSolid(x, y + r, z, w, h - 2.0f * r, color);
    if (r > 0.0f) {
        C2D_DrawCircleSolid(x + r, y + r, z, r, color);
        C2D_DrawCircleSolid(x + w - r, y + r, z, r, color);
        C2D_DrawCircleSolid(x + r, y + h - r, z, r, color);
        C2D_DrawCircleSolid(x + w - r, y + h - r, z, r, color);
    }
}

float text_width(C2D_TextBuf buf, const std::string& s, float scale) {
    if (s.empty()) return 0.0f;
    C2D_Text t;
    C2D_TextParse(&t, buf, s.c_str());
    float w = 0.0f;
    C2D_TextGetDimensions(&t, scale, scale, &w, nullptr);
    return w;
}

std::string fit_text(C2D_TextBuf buf, const std::string& s, float scale, float maxWidth) {
    if (s.empty() || maxWidth <= 0.0f) return s;
    if (text_width(buf, s, scale) <= maxWidth) return s;

    std::vector<size_t> offsets;
    offsets.push_back(0);
    for (size_t i = 1; i < s.size(); ++i) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) offsets.push_back(i);
    }
    offsets.push_back(s.size());

    size_t lo = 0;
    size_t hi = offsets.size() - 1;
    std::string best;

    while (lo <= hi) {
        size_t mid = (lo + hi) / 2;
        std::string candidate = s.substr(0, offsets[mid]) + "...";
        float w = text_width(buf, candidate, scale);
        if (w <= maxWidth) {
            best = candidate;
            if (mid == offsets.size() - 1) break;
            lo = mid + 1;
        } else {
            if (mid == 0) break;
            hi = mid - 1;
        }
    }

    if (best.empty()) best = "...";
    return best;
}

void text(C2D_TextBuf buf, const std::string& s, float x, float y, float scale, u32 color,
          TextAlign align = AlignLeft, bool bold = false, float maxWidth = 0.0f,
          float z = 0.5f) {
    if (s.empty()) return;

    std::string value = maxWidth > 0.0f ? fit_text(buf, s, scale, maxWidth) : s;
    C2D_Text t;
    C2D_TextParse(&t, buf, value.c_str());
    C2D_TextOptimize(&t);

    float w = 0.0f;
    C2D_TextGetDimensions(&t, scale, scale, &w, nullptr);
    if (align == AlignCenter)
        x -= w * 0.5f;
    else if (align == AlignRight)
        x -= w;

    C2D_DrawText(&t, C2D_WithColor, x, y, z, scale, scale, color);
    if (bold) C2D_DrawText(&t, C2D_WithColor, x + 0.7f, y, z, scale, scale, color);
}

void wrap_two_lines(C2D_TextBuf buf, const std::string& s, float scale, float maxWidth,
                    std::string& line1, std::string& line2) {
    line1.clear();
    line2.clear();

    std::vector<std::string> words;
    std::string current;
    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) words.push_back(current);
    if (words.empty()) return;

    size_t i = 0;
    while (i < words.size()) {
        std::string candidate = line1.empty() ? words[i] : line1 + " " + words[i];
        if (text_width(buf, candidate, scale) <= maxWidth) {
            line1 = candidate;
            ++i;
        } else {
            break;
        }
    }

    if (i < words.size()) {
        while (i < words.size()) {
            std::string candidate = line2.empty() ? words[i] : line2 + " " + words[i];
            if (text_width(buf, candidate, scale) <= maxWidth) {
                line2 = candidate;
                ++i;
            } else {
                break;
            }
        }
        if (i < words.size()) {
            std::string rest = line2.empty() ? words[i] : line2 + " " + words[i];
            line2 = fit_text(buf, rest, scale, maxWidth);
        }
    }

    if (line1.empty()) {
        line1 = fit_text(buf, s, scale, maxWidth);
        line2.clear();
    }
}

void draw_note(float cx, float cy, float scale, u32 color, float z = 0.0f) {
    C2D_DrawCircleSolid(cx - 5.0f * scale, cy + 6.0f * scale, z, 5.2f * scale, color);
    C2D_DrawRectSolid(cx + 0.8f * scale, cy - 15.0f * scale, z, 2.6f * scale,
                      21.0f * scale, color);
    C2D_DrawTriangle(cx + 3.4f * scale, cy - 15.0f * scale, color,
                     cx + 13.0f * scale, cy - 10.0f * scale, color,
                     cx + 3.4f * scale, cy - 4.0f * scale, color, z);
}

void draw_play_icon(float cx, float cy, float scale, u32 color, float z = 0.0f) {
    C2D_DrawTriangle(cx - 5.0f * scale, cy - 9.0f * scale, color,
                     cx - 5.0f * scale, cy + 9.0f * scale, color,
                     cx + 10.0f * scale, cy, color, z);
}

void draw_pause_icon(float cx, float cy, float scale, u32 color, float z = 0.0f) {
    C2D_DrawRectSolid(cx - 7.0f * scale, cy - 9.0f * scale, z, 4.5f * scale,
                      18.0f * scale, color);
    C2D_DrawRectSolid(cx + 2.5f * scale, cy - 9.0f * scale, z, 4.5f * scale,
                      18.0f * scale, color);
}

void draw_prev_icon(float cx, float cy, float scale, u32 color, float z = 0.0f) {
    C2D_DrawRectSolid(cx - 9.0f * scale, cy - 7.0f * scale, z, 2.6f * scale,
                      14.0f * scale, color);
    C2D_DrawTriangle(cx + 6.0f * scale, cy - 7.0f * scale, color,
                     cx + 6.0f * scale, cy + 7.0f * scale, color,
                     cx - 6.0f * scale, cy, color, z);
}

void draw_next_icon(float cx, float cy, float scale, u32 color, float z = 0.0f) {
    C2D_DrawRectSolid(cx + 6.4f * scale, cy - 7.0f * scale, z, 2.6f * scale,
                      14.0f * scale, color);
    C2D_DrawTriangle(cx - 6.0f * scale, cy - 7.0f * scale, color,
                     cx - 6.0f * scale, cy + 7.0f * scale, color,
                     cx + 6.0f * scale, cy, color, z);
}

void draw_equalizer(float x, float y, float w, float h, float time, u32 color) {
    float bars[3];
    for (int i = 0; i < 3; ++i) {
        float phase = time * (4.0f + i * 0.7f) + i * 1.7f;
        float v = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(phase));
        bars[i] = h * v;
    }
    float barW = (w - 4.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float bx = x + i * (barW + 2.0f);
        C2D_DrawRectSolid(bx, y + h - bars[i], 0.0f, barW, bars[i], color);
    }
}

void draw_progress(float x, float y, float w, float h, float progress, u32 bg, u32 fg,
                   bool knob) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    rounded_rect(x, y, w, h, h * 0.5f, bg);
    float fill = w * progress;
    if (fill > 0.5f) rounded_rect(x, y, fill, h, h * 0.5f, fg);
    if (knob && fill > 1.0f) C2D_DrawCircleSolid(x + fill, y + h * 0.5f, 0.0f, h + 2.0f, fg);
}

void draw_top_gradient() {
    const int bands = 24;
    for (int i = 0; i < bands; ++i) {
        float t = (float)i / (float)(bands - 1);
        float y = (240.0f / (float)bands) * (float)i;
        C2D_DrawRectSolid(0.0f, y, 0.0f, 400.0f, 240.0f / (float)bands + 1.0f,
                          lerp_color(kTopBgTop, kTopBgBottom, t));
    }
}

void draw_bottom_header_bg() {
    const int bands = 8;
    for (int i = 0; i < bands; ++i) {
        float t = (float)i / (float)(bands - 1);
        float y = (56.0f / (float)bands) * (float)i;
        C2D_DrawRectSolid(0.0f, y, 0.6f, 320.0f, 56.0f / (float)bands + 1.0f,
                          lerp_color(kWhite, kBottomBg, t));
    }
    C2D_DrawRectSolid(0.0f, 55.0f, 0.6f, 320.0f, 1.0f, kBottomLine);
}

void draw_bottom_footer_bg() {
    C2D_DrawRectSolid(0.0f, 200.0f, 0.6f, 320.0f, 40.0f, kWhite);
    C2D_DrawRectSolid(0.0f, 200.0f, 0.6f, 320.0f, 1.0f, kBottomLine);
}

void draw_cover_thumb(AppState& st, float x, float y, float size, bool current, int index,
                      float z = 0.5f) {
    if (current && st.coverValid) {
        float scale = size / 256.0f;
        C2D_DrawImageAt(st.cover, x, y, z, nullptr, scale, scale);
        return;
    }

    static const u32 kThumbColors[8] = {
        RGBA(46, 84, 120, 255),  RGBA(116, 62, 96, 255),
        RGBA(52, 104, 84, 255),  RGBA(122, 92, 44, 255),
        RGBA(82, 64, 124, 255),  RGBA(42, 92, 112, 255),
        RGBA(112, 62, 52, 255),  RGBA(62, 84, 62, 255),
    };

    u32 color = kThumbColors[(unsigned)(index * 5 + 3) % 8];
    rounded_rect(x, y, size, size, 7.0f, color, z);
    C2D_DrawCircleSolid(x + size * 0.5f, y + size * 0.42f, z, size * 0.22f,
                        RGBA(255, 255, 255, 26));
    draw_note(x + size * 0.5f, y + size * 0.52f, size / 34.0f,
              current ? kAccent : RGBA(255, 255, 255, 150), z);
}

float max_scroll(const AppState& st) {
    float content = (float)st.songs.size() * kRowHeight;
    float m = content - kListHeight;
    return m > 0.0f ? m : 0.0f;
}

void ensure_selected_visible(AppState& st) {
    if (st.songs.empty()) return;
    float top = (float)st.selected * kRowHeight;
    float bottom = top + kRowHeight;
    if (top < st.scrollY) st.scrollY = top;
    if (bottom > st.scrollY + kListHeight) st.scrollY = bottom - kListHeight;
    float m = max_scroll(st);
    if (st.scrollY < 0.0f) st.scrollY = 0.0f;
    if (st.scrollY > m) st.scrollY = m;
}

// Transient message shown at the bottom of the top screen. It is drawn on a
// high z so it covers the controls/help text underneath it.
void draw_toast(AppState& st) {
    if (st.toast.empty() || osGetTime() >= st.toastUntil) return;

    float w = text_width(st.textBuf, st.toast, 0.32f);
    float boxW = w + 26.0f;
    rounded_rect(200.0f - boxW * 0.5f, 211.0f, boxW, 21.0f, 10.5f,
                 RGBA(12, 17, 26, 240), 0.8f);
    text(st.textBuf, st.toast, 200.0f, 216.0f, 0.32f, kTopText, AlignCenter, false,
         0.0f, 0.9f);
}

}  // namespace

void ui_draw_loading(C3D_RenderTarget* top, C3D_RenderTarget* bottom, C2D_TextBuf buf,
                     const char* message) {
    C2D_TextBufClear(buf);

    C3D_FrameBegin(0);

    C2D_TargetClear(top, kTopBgTop);
    C2D_SceneBegin(top);
    draw_top_gradient();
    C2D_DrawCircleSolid(200.0f, 118.0f, 0.0f, 86.0f, RGBA(46, 213, 115, 18));
    text(buf, "FangTune", 200.0f, 82.0f, 1.0f, kAccent, AlignCenter, true);
    text(buf, "MP3 player for Nintendo 3DS", 200.0f, 126.0f, 0.42f, kTopMuted, AlignCenter);
    draw_note(200.0f, 166.0f, 2.0f, RGBA(46, 213, 115, 220));

    C2D_TargetClear(bottom, kBottomBg);
    C2D_SceneBegin(bottom);
    draw_note(160.0f, 94.0f, 2.4f, kBottomMuted);
    text(buf, message ? message : "Scanning sdmc:/Music ...", 160.0f, 128.0f, 0.5f,
         kBottomText, AlignCenter);
    text(buf, "MP3 files in sdmc:/Music and sdmc:/", 160.0f, 154.0f, 0.34f, kBottomMuted,
         AlignCenter);

    C3D_FrameEnd(0);
}

void ui_draw_top(AppState& st, C3D_RenderTarget* target) {
    C2D_TargetClear(target, kTopBgTop);
    C2D_SceneBegin(target);

    draw_top_gradient();
    C2D_DrawCircleSolid(98.0f, 121.0f, 0.0f, 112.0f, RGBA(46, 213, 115, 16));

    text(st.textBuf, "FangTune", 22.0f, 12.0f, 0.5f, kAccent, AlignLeft, true);
    text(st.textBuf, "MP3 PLAYER", 378.0f, 15.0f, 0.28f, kTopDim, AlignRight);

    if (st.current < 0 || st.current >= (int)st.songs.size()) {
        C2D_DrawCircleSolid(200.0f, 120.0f, 0.0f, 54.0f, RGBA(46, 213, 115, 18));
        draw_note(200.0f, 112.0f, 3.0f, RGBA(46, 213, 115, 230));

        if (st.songs.empty()) {
            text(st.textBuf, "No music yet", 200.0f, 152.0f, 0.7f, kTopText, AlignCenter, true);
            text(st.textBuf, "Copy MP3 files to sdmc:/Music", 200.0f, 180.0f, 0.38f,
                 kTopMuted, AlignCenter);
            text(st.textBuf, "then press SELECT to rescan", 200.0f, 198.0f, 0.34f, kTopDim,
                 AlignCenter);
        } else {
            text(st.textBuf, "Library ready", 200.0f, 152.0f, 0.7f, kTopText, AlignCenter, true);
            text(st.textBuf, "Tap a song on the bottom screen", 200.0f, 180.0f, 0.38f,
                 kTopMuted, AlignCenter);
            text(st.textBuf, "A play/pause   L/R track   D-pad seek", 200.0f, 198.0f, 0.32f,
                 kTopDim, AlignCenter);
        }
        draw_toast(st);
        return;
    }

    const Song& song = st.songs[st.current];

    const float artX = 22.0f;
    const float artY = 46.0f;
    const float artSize = 150.0f;

    rounded_rect(artX + 3.0f, artY + 5.0f, artSize, artSize, 12.0f, RGBA(0, 0, 0, 90));

    if (st.coverValid) {
        float scale = artSize / 256.0f;
        C2D_DrawImageAt(st.cover, artX, artY, 0.5f, nullptr, scale, scale);
        C2D_DrawRectSolid(artX, artY, 0.6f, artSize, 1.0f, RGBA(255, 255, 255, 26));
        C2D_DrawRectSolid(artX, artY + artSize - 1.0f, 0.6f, artSize, 1.0f,
                          RGBA(255, 255, 255, 18));
    } else {
        rounded_rect(artX, artY, artSize, artSize, 12.0f, kPanel);
        draw_note(artX + artSize * 0.5f, artY + artSize * 0.5f, 4.2f, kTopMuted);
    }

    text(st.textBuf, "NOW PLAYING", 196.0f, 48.0f, 0.32f, kAccent, AlignLeft, true);

    std::string line1, line2;
    wrap_two_lines(st.textBuf, song.title, 0.56f, 184.0f, line1, line2);
    text(st.textBuf, line1, 196.0f, 66.0f, 0.56f, kTopText, AlignLeft, true);
    if (!line2.empty())
        text(st.textBuf, line2, 196.0f, 84.0f, 0.56f, kTopText, AlignLeft, true);

    text(st.textBuf, song.artist, 196.0f, 108.0f, 0.4f, kTopMuted, AlignLeft, false, 184.0f);
    text(st.textBuf, song.album, 196.0f, 125.0f, 0.32f, kTopDim, AlignLeft, false, 184.0f);

    double duration = audio_duration();
    if (duration <= 0.0) duration = song.duration;
    double position = audio_loaded() ? audio_position() : 0.0;
    if (position > duration) position = duration;
    float progress = duration > 0.0 ? (float)(position / duration) : 0.0f;

    draw_progress(196.0f, 149.0f, 184.0f, 4.0f, progress, RGBA(48, 57, 72, 255), kAccent,
                  true);

    text(st.textBuf, format_time(position), 196.0f, 157.0f, 0.3f, kTopDim, AlignLeft);
    text(st.textBuf, format_time(duration), 380.0f, 157.0f, 0.3f, kTopDim, AlignRight);

    const float cy = 194.0f;
    C2D_DrawCircleSolid(238.0f, cy, 0.0f, 16.0f, kPanelLight);
    draw_prev_icon(238.0f, cy, 1.0f, kTopText);

    C2D_DrawCircleSolid(290.0f, cy, 0.0f, 22.0f, kAccent);
    if (st.playing && !st.paused)
        draw_pause_icon(290.0f, cy, 1.0f, kAccentDark);
    else
        draw_play_icon(290.0f, cy, 1.0f, kAccentDark);

    C2D_DrawCircleSolid(342.0f, cy, 0.0f, 16.0f, kPanelLight);
    draw_next_icon(342.0f, cy, 1.0f, kTopText);

    if (st.shuffle)
        text(st.textBuf, "SHUFFLE", 196.0f, 220.0f, 0.28f, kAccent, AlignLeft, true);
    else
        text(st.textBuf, "A play/pause   L/R track   D-pad seek", 290.0f, 220.0f, 0.25f,
             kTopDim, AlignCenter);

    draw_toast(st);
}

void ui_draw_bottom(AppState& st, C3D_RenderTarget* target) {
    C2D_TargetClear(target, kBottomBg);
    C2D_SceneBegin(target);

    float time = (float)(osGetTime() % 100000) / 1000.0f;

    // Song rows. They are allowed to spill into the header/footer, which are
    // painted opaquely afterwards.
    for (size_t i = 0; i < st.songs.size(); ++i) {
        float y = kListTop + (float)i * kRowHeight - st.scrollY;
        if (y + kRowHeight < kListTop || y > kListBottom) continue;

        bool isCurrent = ((int)i == st.current);
        bool isSelected = ((int)i == st.selected);

        if (isCurrent)
            C2D_DrawRectSolid(0.0f, y, 0.0f, 320.0f, kRowHeight, kRowCurrent);
        else if (isSelected)
            C2D_DrawRectSolid(0.0f, y, 0.0f, 320.0f, kRowHeight, kRowSelected);

        if (isCurrent) C2D_DrawRectSolid(0.0f, y, 0.0f, 3.0f, kRowHeight, kAccent);

        draw_cover_thumb(st, 14.0f, y + 3.0f, 32.0f, isCurrent, (int)i);

        const Song& song = st.songs[i];
        text(st.textBuf, song.title, 56.0f, y + 5.0f, 0.46f,
             isCurrent ? RGBA(15, 90, 48, 255) : kBottomText, AlignLeft, isCurrent,
             224.0f);
        text(st.textBuf, song.artist, 56.0f, y + 21.0f, 0.32f, kBottomMuted, AlignLeft,
             false, 224.0f);

        if (isCurrent) {
            draw_equalizer(286.0f, y + 12.0f, 17.0f, 14.0f, time, kAccent);
        } else {
            text(st.textBuf, format_time(song.duration), 306.0f, y + 13.0f, 0.3f,
                 kBottomMuted, AlignRight);
        }

        C2D_DrawRectSolid(56.0f, y + kRowHeight - 1.0f, 0.0f, 250.0f, 1.0f, kBottomLine);
    }

    // Scrollbar.
    float contentHeight = (float)st.songs.size() * kRowHeight;
    if (contentHeight > kListHeight) {
        float trackH = kListHeight - 4.0f;
        float thumbH = trackH * (kListHeight / contentHeight);
        if (thumbH < 24.0f) thumbH = 24.0f;
        float m = max_scroll(st);
        float thumbY = kListTop + 2.0f;
        if (m > 0.0f) thumbY += (st.scrollY / m) * (trackH - thumbH);
        rounded_rect(315.0f, kListTop + 2.0f, 3.0f, trackH, 1.5f, RGBA(226, 226, 231, 255));
        rounded_rect(315.0f, thumbY, 3.0f, thumbH, 1.5f, RGBA(190, 190, 197, 255));
    }

    draw_bottom_header_bg();

    text(st.textBuf, "Library", 16.0f, 8.0f, 0.85f, kBottomText, AlignLeft, true, 0.0f,
         0.7f);

    char subtitle[64];
    if (st.shuffle)
        std::snprintf(subtitle, sizeof(subtitle), "%d songs  -  Shuffle on",
                      (int)st.songs.size());
    else
        std::snprintf(subtitle, sizeof(subtitle), "%d songs", (int)st.songs.size());
    text(st.textBuf, subtitle, 16.0f, 38.0f, 0.36f, st.shuffle ? kAccentPillText : kBottomMuted,
         AlignLeft, false, 0.0f, 0.7f);

    rounded_rect(232.0f, 14.0f, 72.0f, 26.0f, 13.0f,
                 st.shuffle ? kAccentPill : kPill, 0.7f);
    text(st.textBuf, "Shuffle", 268.0f, 20.0f, 0.34f,
         st.shuffle ? kAccentPillText : kPillText, AlignCenter, st.shuffle, 0.0f, 0.7f);

    if (st.songs.empty()) {
        draw_note(160.0f, 100.0f, 2.4f, kBottomMuted);
        text(st.textBuf, "No songs found", 160.0f, 132.0f, 0.55f, kBottomText, AlignCenter,
             true);
        text(st.textBuf, "Copy MP3 files to sdmc:/Music", 160.0f, 158.0f, 0.36f,
             kBottomMuted, AlignCenter);
        text(st.textBuf, "Press SELECT to rescan", 160.0f, 178.0f, 0.32f, kBottomMuted,
             AlignCenter);
    }

    draw_bottom_footer_bg();

    if (st.current >= 0 && st.current < (int)st.songs.size()) {
        const Song& song = st.songs[st.current];
        double duration = audio_duration();
        if (duration <= 0.0) duration = song.duration;
        double position = audio_loaded() ? audio_position() : 0.0;
        if (position > duration) position = duration;
        float progress = duration > 0.0 ? (float)(position / duration) : 0.0f;

        C2D_DrawRectSolid(0.0f, 200.0f, 0.7f, 320.0f, 3.0f, RGBA(229, 229, 234, 255));
        C2D_DrawRectSolid(0.0f, 200.0f, 0.7f, 320.0f * progress, 3.0f, kAccent);

        draw_cover_thumb(st, 12.0f, 207.0f, 30.0f, true, st.current, 0.7f);
        text(st.textBuf, song.title, 50.0f, 206.0f, 0.4f, kBottomText, AlignLeft, true,
             220.0f, 0.7f);
        text(st.textBuf, song.artist, 50.0f, 221.0f, 0.3f, kBottomMuted, AlignLeft, false,
             220.0f, 0.7f);

        C2D_DrawCircleSolid(294.0f, 220.0f, 0.7f, 14.0f, kAccent);
        if (st.playing && !st.paused)
            draw_pause_icon(294.0f, 220.0f, 0.72f, kAccentDark, 0.7f);
        else
            draw_play_icon(294.0f, 220.0f, 0.72f, kAccentDark, 0.7f);
    } else {
        text(st.textBuf, "Nothing playing", 14.0f, 212.0f, 0.42f, kBottomMuted, AlignLeft,
             false, 0.0f, 0.7f);
        C2D_DrawCircleSolid(294.0f, 220.0f, 0.7f, 14.0f, RGBA(224, 224, 229, 255));
        draw_play_icon(294.0f, 220.0f, 0.72f, RGBA(160, 160, 166, 255), 0.7f);
    }
}

void ui_handle_keys(AppState& st, u32 keysDown, u32 keysRepeat) {
    if (!st.songs.empty()) {
        if (keysRepeat & KEY_DOWN) {
            if (st.selected + 1 < (int)st.songs.size()) ++st.selected;
            st.scrollVel = 0.0f;
            ensure_selected_visible(st);
        }
        if (keysRepeat & KEY_UP) {
            if (st.selected > 0) --st.selected;
            st.scrollVel = 0.0f;
            ensure_selected_visible(st);
        }
    }

    if (keysDown & KEY_A) {
        if (st.selected != st.current)
            app_play(st, st.selected);
        else
            app_toggle(st);
    }
    if (keysDown & KEY_B) app_prev(st);
    if (keysDown & KEY_X) app_next(st);
    if (keysDown & KEY_Y) app_shuffle(st);
    if (keysDown & KEY_L) app_prev(st);
    if (keysDown & KEY_R) app_next(st);
    if (keysDown & KEY_LEFT) app_seek_relative(st, -5.0);
    if (keysDown & KEY_RIGHT) app_seek_relative(st, 5.0);
}

void ui_handle_touch(AppState& st, const touchPosition& touch, bool down, bool held, bool up) {
    if (down) {
        st.touchActive = true;
        st.touchMoved = false;
        st.touchStartMs = osGetTime();
        st.dragStartY = touch.py;
        st.dragStartScroll = st.scrollY;
        st.lastScroll = st.scrollY;
        st.lastTouchX = touch.px;
        st.lastTouchY = touch.py;

        if (touch.py >= 200.0f) {
            if (touch.py <= 205.0f && st.current >= 0 &&
                st.current < (int)st.songs.size()) {
                double duration = audio_duration();
                if (duration <= 0.0) duration = st.songs[st.current].duration;
                if (duration > 0.0) {
                    double target = duration * (double)touch.px / 320.0;
                    app_seek(st, target);
                }
            } else {
                app_toggle(st);
            }
        } else if (touch.py >= kListTop && touch.py < kListBottom) {
            st.dragging = true;
        } else {
            if (touch.px >= 228.0f && touch.px <= 308.0f && touch.py >= 10.0f &&
                touch.py <= 44.0f)
                app_shuffle(st);
        }
    } else if (held && st.touchActive) {
        st.lastTouchX = touch.px;
        st.lastTouchY = touch.py;
        if (st.dragging) {
            float delta = (float)(st.dragStartY - touch.py);
            if (std::fabs(delta) > 6.0f) st.touchMoved = true;

            float newScroll = st.dragStartScroll + delta;
            float m = max_scroll(st);
            if (newScroll < 0.0f) newScroll = 0.0f;
            if (newScroll > m) newScroll = m;

            st.scrollVel = (newScroll - st.lastScroll) * 0.9f;
            st.scrollY = newScroll;
            st.lastScroll = newScroll;
        }
    } else if (up && st.touchActive) {
        if (st.dragging) {
            st.dragging = false;
            uint64_t elapsed = osGetTime() - st.touchStartMs;
            float ty = (float)st.lastTouchY;
            if (!st.touchMoved && elapsed < 600 && ty >= kListTop && ty < kListBottom) {
                int row = (int)((ty - kListTop + st.scrollY) / kRowHeight);
                if (row >= 0 && row < (int)st.songs.size()) app_play(st, row);
            }
        }
        st.touchActive = false;
    }
}

void ui_update_scroll(AppState& st, float dt) {
    (void)dt;
    if (st.dragging || st.songs.empty()) return;

    float m = max_scroll(st);
    if (m <= 0.0f) {
        st.scrollY = 0.0f;
        st.scrollVel = 0.0f;
        return;
    }

    if (std::fabs(st.scrollVel) > 0.05f) {
        st.scrollY += st.scrollVel;
        st.scrollVel *= 0.86f;
        if (st.scrollY < 0.0f) {
            st.scrollY = 0.0f;
            st.scrollVel = 0.0f;
        }
        if (st.scrollY > m) {
            st.scrollY = m;
            st.scrollVel = 0.0f;
        }
    } else {
        st.scrollVel = 0.0f;
    }
}
