#include <stdint.h>
#include<stdlib.h>
#include<stdbool.h>
#include<stdio.h>
#include<string.h>

#include"svg.h"

static void appendstringtosvg(SVG* psvg, char* text){
    fprintf(psvg->file, "%s", text);
}

static void appendnumbertosvg(SVG* psvg, float n){
    char sn[16];
    sprintf(sn, "%f", n);
    appendstringtosvg(psvg, sn);
}

SVG* SVG_create(FILE *file, float width, float height){
    SVG* psvg = malloc(sizeof(SVG));

    if(psvg != NULL)
    {
        *psvg = (SVG){.file = file, .width = width, .height = height, .finalized = false};
        fprintf(psvg->file, "%s", "\0");
        appendstringtosvg(psvg, "<svg ");
        
        if (width > 0){
            appendstringtosvg(psvg, "width='");
            appendnumbertosvg(psvg, width);
            appendstringtosvg(psvg, "px' ");
        }

        if (height > 0){
            appendstringtosvg(psvg, "height='");
            appendnumbertosvg(psvg, height);
            appendstringtosvg(psvg, "px' "); 
        }

        appendstringtosvg(psvg, "xmlns='http://www.w3.org/2000/svg' version='1.1' xmlns:xlink='http://www.w3.org/1999/xlink'>\n");

        return psvg;
    }
    else
    {
        return NULL;
    }
}

void SVG_finalize(SVG* psvg){
    appendstringtosvg(psvg, "</svg>");

    psvg->finalized = true;
}

void SVG_free(SVG* psvg){
    free(psvg);
}

void SVG_line(SVG* psvg, uint32_t color, float strokewidth, float x1, float y1, float x2, float y2){
    uint8_t *c = (uint8_t *)&color;
    fprintf(psvg->file, "    <line stroke='#%02X%02X%02X' ", c[0], c[1], c[2]);
    appendstringtosvg(psvg, "vector-effect='non-scaling-stroke'");
    appendstringtosvg(psvg, " stroke-width='");
    appendnumbertosvg(psvg, strokewidth);
    appendstringtosvg(psvg, "px' y2='");
    appendnumbertosvg(psvg, y2);
    appendstringtosvg(psvg, "' x2='");
    appendnumbertosvg(psvg, x2);
    appendstringtosvg(psvg, "' y1='");
    appendnumbertosvg(psvg, y1);
    appendstringtosvg(psvg, "' x1='");
    appendnumbertosvg(psvg, x1);
    appendstringtosvg(psvg, "' />\n");
}

void SVG_rectangle(SVG* psvg, float width, float height, float x, float y, uint32_t fillColor, uint32_t strokeColor, float strokewidth, float radiusx, float radiusy){
    uint8_t *fill = (uint8_t *)&fillColor;
    fprintf(psvg->file, "    <rect fill='#%02X%02X%02X' ", fill[0], fill[1], fill[2]);
    
    uint8_t *stroke = (uint8_t *)&strokeColor;
    fprintf(psvg->file, "stroke='#%02X%02X%02X' ", stroke[0], stroke[1], stroke[2]);
    
    appendstringtosvg(psvg, "stroke-width='");
    appendnumbertosvg(psvg, strokewidth);
    appendstringtosvg(psvg, "px' width='");
    appendnumbertosvg(psvg, width);
    appendstringtosvg(psvg, "' height='");
    appendnumbertosvg(psvg, height);
    appendstringtosvg(psvg, "' y='");
    appendnumbertosvg(psvg, y);
    appendstringtosvg(psvg, "' x='");
    appendnumbertosvg(psvg, x);
    appendstringtosvg(psvg, "' ry='");
    appendnumbertosvg(psvg, radiusy);
    appendstringtosvg(psvg, "' rx='");
    appendnumbertosvg(psvg, radiusx);
    appendstringtosvg(psvg, "' />\n");
}

void SVG_polygon(SVG* psvg, SVG_Point* points, int pointCount, uint32_t fillColor, uint32_t strokeColor, float strokeWidth){
    uint8_t *fill = (uint8_t *)&fillColor;
    fprintf(psvg->file, "    <polygon fill='#%02X%02X%02X' ",fill[0], fill[1], fill[2]);

    uint8_t *stroke = (uint8_t *)&strokeColor;
    fprintf(psvg->file, "stroke='#%02X%02X%02X' ",stroke[0], stroke[1], stroke[2]);

    appendstringtosvg(psvg, "stroke-width='");
    appendnumbertosvg(psvg, strokeWidth);
    appendstringtosvg(psvg, "px' points='");

    for (int i = 0; i < pointCount; i++) {
        appendnumbertosvg(psvg, points[i].x);
        appendstringtosvg(psvg, ",");
        appendnumbertosvg(psvg, points[i].y);

        if (i < pointCount - 1)
            appendstringtosvg(psvg, " ");
    }

    appendstringtosvg(psvg, "' />\n");
}

void SVG_circle(SVG* psvg, uint32_t strokeColor, float strokewidth, uint32_t fillColor, float r, float cx, float cy){
    uint8_t *stroke = (uint8_t *)&strokeColor;
    fprintf(psvg->file, "    <circle stroke='#%02X%02X%02X' ", stroke[0], stroke[1], stroke[2]);

    appendstringtosvg(psvg, "stroke-width='");
    appendnumbertosvg(psvg, strokewidth);
    
    uint8_t *fill = (uint8_t *)&fillColor;
    fprintf(psvg->file, "px' fill='#%02X%02X%02X' ", fill[0], fill[1], fill[2]);
    
    appendstringtosvg(psvg, "r='");
    appendnumbertosvg(psvg, r);
    appendstringtosvg(psvg, "' cy='");
    appendnumbertosvg(psvg, cy);
    appendstringtosvg(psvg, "' cx='");
    appendnumbertosvg(psvg, cx);
    appendstringtosvg(psvg, "' />\n");
}

void SVG_text(SVG* psvg, float x, float y, char* fontfamily, float fontsize, char* fill, char* stroke, char* text){
    appendstringtosvg(psvg, "    <text x='");
    appendnumbertosvg(psvg, x);
    appendstringtosvg(psvg, "' y = '");
    appendnumbertosvg(psvg, y);
    appendstringtosvg(psvg, "' font-family='");
    appendstringtosvg(psvg, fontfamily);
    appendstringtosvg(psvg, "' stroke='");
    appendstringtosvg(psvg, stroke);
    appendstringtosvg(psvg, "' fill='");
    appendstringtosvg(psvg, fill);
    appendstringtosvg(psvg, "' font-size='");
    appendnumbertosvg(psvg, fontsize);
    appendstringtosvg(psvg, "px'>");
    appendstringtosvg(psvg, text);
    appendstringtosvg(psvg, "</text>\n");
}

void SVG_ellipse(SVG* psvg, float cx, float cy, float rx, float ry, char* fill, char* stroke, float strokewidth){
    appendstringtosvg(psvg, "    <ellipse cx='");
    appendnumbertosvg(psvg, cx);
    appendstringtosvg(psvg, "' cy='");
    appendnumbertosvg(psvg, cy);
    appendstringtosvg(psvg, "' rx='");
    appendnumbertosvg(psvg, rx);
    appendstringtosvg(psvg, "' ry='");
    appendnumbertosvg(psvg, ry);
    appendstringtosvg(psvg, "' fill='");
    appendstringtosvg(psvg, fill);
    appendstringtosvg(psvg, "' stroke='");
    appendstringtosvg(psvg, stroke);
    appendstringtosvg(psvg, "' stroke-width='");
    appendnumbertosvg(psvg, strokewidth);
    appendstringtosvg(psvg, "' />\n");
}