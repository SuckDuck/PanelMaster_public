// https://github.com/CodeDrome/svg-library-c

#ifndef SVG_H
#define SVG_H

#include<stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct SVG {
    FILE *file;
    int height;
    int width;
    bool finalized;
} SVG;

typedef struct {
    float x;
    float y;
} SVG_Point;

// --------------------------------------------------------
// FUNCTION PROTOTYPES
// --------------------------------------------------------
SVG* SVG_create(FILE *file, float width, float height);
void SVG_finalize(SVG* psvg);
void SVG_free(SVG* psvg);

void SVG_line(SVG* psvg, uint32_t color, float strokewidth, float x1, float y1, float x2, float y2);
void SVG_rectangle(SVG* psvg, float width, float height, float x, float y, uint32_t fillColor, uint32_t strokeColor, float strokewidth, float radiusx, float radiusy);
void SVG_polygon(SVG* psvg, SVG_Point* points, int pointCount, uint32_t fillColor, uint32_t strokeColor, float strokeWidth);
void SVG_circle(SVG* psvg, uint32_t strokeColor, float strokewidth, uint32_t fillColor, float r, float cx, float cy);
void SVG_text(SVG* psvg, float x, float y, char* fontfamily, float fontsize, char* fill, char* stroke, char* text);
void SVG_ellipse(SVG* psvg, float cx, float cy, float rx, float ry, char* fill, char* stroke, float strokewidth);

#endif