#pragma once

#include "types.h"

// Reads ID3v2.2/2.3/2.4 tags and falls back to ID3v1.
// Cover art is returned as a byte range inside the file whenever possible.
bool id3_read_metadata(const char* path, Song& song);

// Reads the first MPEG frame header to get sample rate, channel count,
// bitrate and a duration estimate. Safe to call if the file is malformed.
void mp3_probe(const char* path, Song& song);
