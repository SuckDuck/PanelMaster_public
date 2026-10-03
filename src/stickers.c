// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <stdlib.h>
#include "stickers.h"

Sticker *stickers;
Sticker *lastAddedSticker;

void RemoveSticker(Sticker *s){
    if (!s) return;

    for (Sticker *ss = stickers; ss != NULL; ss = ss->next){
        if (ss->next == s){
            ss->next = s->next;
            break;
        }
    }

    if (stickers == s)
        stickers = s->next;

    if (s->srcPath)
        free(s->srcPath);
    UnloadImage(s->img);
    UnloadTexture(s->tex);
    free(s);
}

void CleanStickers(){
    while (stickers != NULL)
        RemoveSticker(stickers);
}