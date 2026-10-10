// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef STICKERS_H
#define STICKERS_H

#include <raylib.h>
#include <stddef.h>

typedef struct Sticker Sticker;
extern Sticker *stickers;
extern Sticker *lastAddedSticker;

struct Sticker {
    size_t refQ;
    Image img;
    Texture tex;
    unsigned int hash;
    char *srcPath;
    Sticker *next;
};

void RemoveSticker(Sticker *s);
void CleanStickers();

#endif