#include "app.h"
#include "audio.h"
#include "cover.h"
#include "library.h"

#include <3ds.h>

#include <algorithm>
#include <cstdlib>

namespace {

int random_track(int count, int avoid) {
    if (count <= 1) return 0;
    int index = avoid;
    while (index == avoid) index = rand() % count;
    return index;
}

}  // namespace

void app_init(AppState& st) {
    st.textBuf = C2D_TextBufNew(32768);
    srand((unsigned)osGetTime());
    st.audioReady = audio_init();
    audio_set_finished_callback([&st]() { app_next(st); });
}

void app_fini(AppState& st) {
    audio_exit();
    if (st.coverValid) {
        cover_free(st.coverTex);
        st.coverValid = false;
    }
    if (st.textBuf) {
        C2D_TextBufDelete(st.textBuf);
        st.textBuf = nullptr;
    }
}

size_t app_scan(AppState& st) {
    std::string currentPath;
    if (st.current >= 0 && st.current < (int)st.songs.size())
        currentPath = st.songs[st.current].path;

    size_t count = library_scan(st.songs);

    // The library is re-sorted on every scan, so find the playing track again
    // by path instead of trusting the old index.
    int newCurrent = -1;
    if (!currentPath.empty()) {
        for (size_t i = 0; i < st.songs.size(); ++i) {
            if (st.songs[i].path == currentPath) {
                newCurrent = (int)i;
                break;
            }
        }
    }

    bool lostCurrent = st.current >= 0 && newCurrent < 0;
    st.current = newCurrent;

    if (lostCurrent) {
        audio_close();
        st.playing = false;
        st.paused = true;
    }

    if (st.songs.empty()) {
        st.selected = 0;
        st.current = -1;
        st.scrollY = 0.0f;
        st.scrollVel = 0.0f;
        if (st.coverValid) {
            cover_free(st.coverTex);
            st.coverValid = false;
        }
        st.coverSong = -1;
        audio_close();
        return 0;
    }

    if (st.selected < 0 || st.selected >= (int)st.songs.size()) st.selected = 0;

    // Metadata may have changed, and the current track may now be at a new
    // index, so drop the cached cover and load it again.
    st.coverSong = -1;
    if (st.coverValid) {
        cover_free(st.coverTex);
        st.coverValid = false;
    }
    if (st.current >= 0) app_load_cover(st, st.current);

    return count;
}

void app_play(AppState& st, int index) {
    if (index < 0 || index >= (int)st.songs.size()) return;

    st.selected = index;
    st.current = index;

    bool ok = st.audioReady && audio_load(st.songs[index]);
    if (ok) {
        audio_play();
        double duration = audio_duration();
        if (duration > 0.0) st.songs[index].duration = duration;
        st.playing = true;
        st.paused = false;
    } else {
        st.playing = false;
        st.paused = true;
        if (!st.audioReady)
            app_toast(st, "Audio unavailable - DSP firmware missing?");
        else
            app_toast(st, "Could not play this MP3");
    }

    app_load_cover(st, index);
}

void app_toggle(AppState& st) {
    if (st.songs.empty()) return;

    if (st.current < 0 || !audio_loaded()) {
        app_play(st, st.selected);
        return;
    }

    audio_toggle();
    st.playing = audio_is_playing();
    st.paused = audio_is_paused();
}

void app_next(AppState& st) {
    if (st.songs.empty()) return;

    int next = 0;
    if (st.shuffle) {
        next = random_track((int)st.songs.size(), st.current);
    } else if (st.current >= 0) {
        next = st.current + 1;
        if (next >= (int)st.songs.size()) next = 0;
    }

    app_play(st, next);
}

void app_prev(AppState& st) {
    if (st.songs.empty()) return;

    if (st.current >= 0 && audio_loaded() && audio_position() > 3.0) {
        audio_seek(0.0);
        return;
    }

    if (st.shuffle) {
        app_play(st, random_track((int)st.songs.size(), st.current));
        return;
    }

    int prev = (int)st.songs.size() - 1;
    if (st.current > 0) prev = st.current - 1;
    app_play(st, prev);
}

void app_seek(AppState& st, double seconds) {
    (void)st;
    if (audio_loaded()) audio_seek(seconds);
}

void app_seek_relative(AppState& st, double delta) {
    (void)st;
    if (!audio_loaded()) return;
    double target = audio_position() + delta;
    if (target < 0.0) target = 0.0;
    audio_seek(target);
}

void app_shuffle(AppState& st) {
    st.shuffle = !st.shuffle;
    app_toast(st, st.shuffle ? "Shuffle on" : "Shuffle off");
}

void app_update(AppState& st) {
    audio_update();
    st.playing = audio_is_playing();
    st.paused = audio_is_paused();

    if (st.current >= 0 && st.current < (int)st.songs.size() && audio_loaded()) {
        double duration = audio_duration();
        if (duration > 0.0) st.songs[st.current].duration = duration;
    }
}

void app_load_cover(AppState& st, int index) {
    if (index == st.coverSong && st.coverValid) return;

    if (st.coverValid) {
        cover_free(st.coverTex);
        st.coverValid = false;
    }

    st.coverSong = index;
    if (index < 0 || index >= (int)st.songs.size()) return;

    st.coverValid = cover_load(st.songs[index], st.coverTex, st.coverSub, st.cover);
}

void app_toast(AppState& st, const std::string& message) {
    st.toast = message;
    st.toastUntil = osGetTime() + 2600;
}
