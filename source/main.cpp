#include "app.h"
#include "ui.h"

#include <3ds.h>
#include <citro2d.h>

#include <cstdio>

namespace {

u64 g_lastRepeatMs = 0;
int g_repeatDir = 0;

u32 repeat_mask(u32 down, u32 held) {
    u32 repeat = 0;

    int dir = 0;
    if (held & KEY_DOWN) dir = 1;
    else if (held & KEY_UP) dir = -1;

    if (down & (KEY_DOWN | KEY_UP)) {
        g_lastRepeatMs = osGetTime();
        g_repeatDir = dir;
        repeat = down & (KEY_DOWN | KEY_UP);
    } else if (dir != 0 && dir == g_repeatDir) {
        u64 now = osGetTime();
        u64 wait = now - g_lastRepeatMs;
        if (wait > 420) {
            repeat = (dir > 0) ? KEY_DOWN : KEY_UP;
            // Keep a steady repeat rate after the initial delay.
            g_lastRepeatMs = now - 320;
        }
    } else if (dir == 0) {
        g_repeatDir = 0;
    }

    return repeat;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    AppState st;
    app_init(st);

    ui_draw_loading(top, bottom, st.textBuf, "Scanning sdmc:/Music ...");
    size_t found = app_scan(st);

    if (found == 0)
        app_toast(st, "No MP3s found - copy music to sdmc:/Music");

    u64 lastTick = osGetTime();

    while (aptMainLoop()) {
        hidScanInput();

        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();
        u32 up = hidKeysUp();
        u32 repeat = down | repeat_mask(down, held);

        if (down & KEY_START) break;
        if (down & KEY_SELECT) {
            ui_draw_loading(top, bottom, st.textBuf, "Rescanning sdmc:/Music ...");
            size_t count = app_scan(st);
            char msg[64];
            std::snprintf(msg, sizeof(msg), "%d songs found", (int)count);
            app_toast(st, msg);
        }

        touchPosition touch;
        hidTouchRead(&touch);
        ui_handle_touch(st, touch, (down & KEY_TOUCH) != 0,
                        (held & KEY_TOUCH) != 0, (up & KEY_TOUCH) != 0);

        ui_handle_keys(st, down, repeat);

        app_update(st);

        u64 now = osGetTime();
        float dt = (float)(now - lastTick) / 1000.0f;
        if (dt > 0.25f) dt = 0.0f;
        lastTick = now;
        ui_update_scroll(st, dt);

        C2D_TextBufClear(st.textBuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        ui_draw_top(st, top);
        ui_draw_bottom(st, bottom);
        C3D_FrameEnd(0);
    }

    app_fini(st);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
