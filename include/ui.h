#pragma once

#include "app.h"

#include <3ds.h>
#include <citro2d.h>

void ui_draw_loading(C3D_RenderTarget* top, C3D_RenderTarget* bottom, C2D_TextBuf buf,
                     const char* message);
void ui_draw_top(AppState& st, C3D_RenderTarget* target);
void ui_draw_bottom(AppState& st, C3D_RenderTarget* target);

void ui_handle_touch(AppState& st, const touchPosition& touch, bool down, bool held, bool up);
void ui_handle_keys(AppState& st, u32 keysDown, u32 keysRepeat);
void ui_update_scroll(AppState& st, float dt);
