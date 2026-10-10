// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef TITLEBLOCK_H
#define TITLEBLOCK_H

#include <raylib.h>
#include "origami.h"

#define DEFAULT_TITLE_BLOCK_W 180
#define DEFAULT_TITLE_BLOCK_H 48

typedef struct TitleBlockLine TitleBlockLine;
typedef struct TitleBlockRect TitleBlockRect;
typedef struct TitleBlock TitleBlock;
typedef struct PageEntityTitleBlock PageEntityTitleBlock;

typedef enum TitleBlockRectTextPosition {
    TBRTP_BOTTOM,
    TBRTP_TOP
} TitleBlockRectTextPosition;

struct TitleBlockLine {
    TitleBlock *titleBlock;
    float t;
    int type;

    TitleBlockLine *next;
};

struct TitleBlockRect {
    TitleBlock *titleBlock;
    struct {
        TitleBlockLine *top;
        TitleBlockLine *bottom;
        TitleBlockLine *left;
        TitleBlockLine *right;
    } lines;

    float textSize;
    char *label;
    char *content;
    int externalContent;
    int noLabel;
    TitleBlockRectTextPosition textPos;
    TitleBlockRect *next;
};

struct TitleBlock {
    Vector2 size;
    TitleBlockLine *lines;
    TitleBlockRect *rects; 
    TitleBlockRect *selectedRect;
    char *sourcePath;
};

TitleBlockLine *AddTitleBlockLine(TitleBlock *tb, int type, float t);
TitleBlockRect *AddTitleBlockRect(TitleBlock *tb, TitleBlockLine *left, TitleBlockLine*right, TitleBlockLine *top, TitleBlockLine *bottom );
TitleBlock *NewTitleBlock(Vector2 *size);
void FreeTitleBlock(TitleBlock *tb);
void AddTitleBlockDivisions(TitleBlockRect *r, int type, int q, TitleBlockRect **out);
void CollapseTitleBlockLines(TitleBlockLine *A, TitleBlockLine *B);
float GetTitleBlockLinePos(TitleBlockLine *l);
void GetNeighboringLines(TitleBlockLine *l, TitleBlockRect *r, TitleBlockLine **out);
bool IsPInsideTitleBlockRect(Vector2 p, TitleBlockRect *r);
bool IsPInsideTitleBlockLine(Vector2 p, TitleBlockLine *l);
bool SetTitleBlockLabel(TitleBlockRect *r, char *label);
Rectangle DrawTitleBlockRect(TitleBlockRect *tbr, OG_Viewport *viewport, Vector2 offset, Color c);
void DrawTitleBlock(TitleBlock *tb, OG_Viewport *viewport, Vector2 offset, Color c, bool noRepeat, PageEntityTitleBlock *entity);

#endif