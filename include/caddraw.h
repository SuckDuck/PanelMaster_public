// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef CADDRAW_H
#define CADDRAW_H

#include <raylib.h>
#include <stddef.h>
#include <stdint.h>
#include "stickers.h"

typedef enum CadOutputType {
    CAD_OUTPUT_DEFAULT,
    CAD_OUTPUT_MEASURE,
    CAD_OUTPUT_PDF,
    CAD_OUTPUT_DXF,
    CAD_OUTPUT_SVG
} CadOutputType;

typedef enum CadTextAlign {
    CAD_ALIGN_LEFT, 
    CAD_ALIGN_CENTER, 
    CAD_ALIGN_RIGHT,
} CadTextAlign;

typedef struct CadChar {
    Vector2 *vertices;
    int verticesCount;
    float advance;
    float height;
} CadChar;

typedef struct CadFont {
    const uint8_t *ogData;
    size_t ogDataSize;
    CadChar *chars;
    int charsQ;
} CadFont;

typedef struct CadBounds {
    Vector2 min;
    Vector2 max;
} CadBounds;

void SetCadOutput(CadOutputType o, void *param);
CadFont *LoadCadFont(const uint8_t *data, size_t size, char *chars);
void FreeCadFont(CadFont *font);
Vector2 MeasureCadText(CadFont *font, const char *text, int textLen, float fontSize, float xspacing, float yspacing);
void OpenCadPath();
void CloseCadPath();
void DrawCadLine(Vector2 startPos, Vector2 endPos, float thick, Color color);
void EraseCadArea(Vector2 *points, int pointCount);
void DrawCadText(CadFont *font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float xspacing, float yspacing, CadTextAlign align, Color tint);
void SetCadScissors(Rectangle rect);
void UnsetCadScissors();
void SetCadMask(Vector2 *points, int pointC);
void UnsetCadMask();
void DrawCadSticker(Sticker *s, Vector2 pos, float scale);
CadBounds GetCadBounds();

#endif