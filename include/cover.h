#pragma once

#include "types.h"

#include <citro2d.h>

// Decodes embedded cover art into a 256x256 RGBA texture.
bool cover_load(const Song& song, C3D_Tex& tex, Tex3DS_SubTexture& sub, C2D_Image& image);
void cover_free(C3D_Tex& tex);
