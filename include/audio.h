#pragma once

#include "types.h"

#include <functional>

bool audio_init();
void audio_exit();

bool audio_load(const Song& song);
void audio_play();
void audio_pause();
void audio_toggle();
void audio_stop();
void audio_close();
void audio_seek(double seconds);
void audio_update();

bool audio_loaded();
bool audio_is_playing();
bool audio_is_paused();
double audio_position();
double audio_duration();

void audio_set_finished_callback(std::function<void()> callback);
