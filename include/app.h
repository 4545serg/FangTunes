#pragma once

#include "types.h"

#include <citro2d.h>

#include <cstdint>
#include <string>
#include <vector>

struct AppState {
    std::vector<Song> songs;

    int selected = 0;
    int current = -1;

    bool audioReady = false;
    bool shuffle = false;
    bool playing = false;
    bool paused = true;

    // Bottom-screen scrolling / touch state.
    float scrollY = 0.0f;
    float scrollVel = 0.0f;
    bool dragging = false;
    bool touchActive = false;
    int dragStartY = 0;
    float dragStartScroll = 0.0f;
    float lastScroll = 0.0f;
    bool touchMoved = false;
    uint64_t touchStartMs = 0;
    int lastTouchX = 0;
    int lastTouchY = 0;

    C2D_TextBuf textBuf = nullptr;

    C3D_Tex coverTex = {};
    Tex3DS_SubTexture coverSub = {};
    C2D_Image cover = {};
    bool coverValid = false;
    int coverSong = -1;

    std::string toast;
    uint64_t toastUntil = 0;
};

void app_init(AppState& st);
void app_fini(AppState& st);

size_t app_scan(AppState& st);

void app_play(AppState& st, int index);
void app_toggle(AppState& st);
void app_next(AppState& st);
void app_prev(AppState& st);
void app_seek(AppState& st, double seconds);
void app_seek_relative(AppState& st, double delta);
void app_shuffle(AppState& st);
void app_update(AppState& st);

void app_load_cover(AppState& st, int index);
void app_toast(AppState& st, const std::string& message);
