#pragma once

#include "types.h"

#include <vector>

// Scans sdmc:/Music (recursively) and MP3 files directly in sdmc:/.
// Returns the number of tracks found.
size_t library_scan(std::vector<Song>& songs);
