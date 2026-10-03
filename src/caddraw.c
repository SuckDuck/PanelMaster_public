// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <float.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "caddraw.h"
#include "dxfout.h"
#include "pdfgen.h"
#include "stickers.h"
#include "svg.h"
#include "ttf2mesh.h"
#include "utils.h"

#define PT_TO_MM(pt) ((pt) * 25.4f / 72.0f)

typedef struct PdfOperation PdfOperation;
struct PdfOperation {
    struct pdf_path_operation op;
    struct pdf_doc *pdf; 
    struct pdf_object *page;
    float width;
    uint32_t color;
    size_t opsQ;
    PdfOperation *next;
    PdfOperation *tail;
};

static PdfOperation *pdfOperations;
static bool pathOpen = false;
static bool dxfFirstPointDone = false;

static CadOutputType output;
static struct pdf_doc *pdf;
static DXF *dxf;
static SVG *svg;
static CadBounds bounds;

struct {
    Vector2 *points;
    int pointsQ;
} mask = {NULL, 0};

static void AddLine2Operations(PdfOperation **op, struct pdf_doc *pdf, struct pdf_object *page, float x1, float y1, float x2, float y2, float width, uint32_t colour){
    PdfOperation *moveOP = calloc(1, sizeof(PdfOperation));
    moveOP->op.op = 'm';
    moveOP->op.x1 = x1;
    moveOP->op.y1 = y1;

    PdfOperation *lineOP = calloc(1, sizeof(PdfOperation));
    lineOP->op.op = 'l';
    lineOP->op.x1 = x2;
    lineOP->op.y1 = y2;

    moveOP->next = lineOP;
    if (*op == NULL){
        *op = moveOP;
        moveOP->pdf = pdf;
        moveOP->page = page;
        moveOP->width = width;
        moveOP->color = colour;
        moveOP->opsQ = 2;
        moveOP->tail = lineOP;
        return;
    }

    (*op)->pdf = pdf;
    (*op)->page = page;
    (*op)->width = width;
    (*op)->color = colour;
    (*op)->opsQ += 2;
    (*op)->tail->next = moveOP;
    (*op)->tail = lineOP;

}

void SetCadOutput(CadOutputType o, void *param){
    switch (o){
        case CAD_OUTPUT_PDF:{
            if (!param) return;
            pdf = param;
            dxf = NULL;
            svg = NULL;
        } break;

        case CAD_OUTPUT_DXF:{
            if (!param) return;
            dxf = param;
            pdf = NULL;
            svg = NULL;
        } break;

        case CAD_OUTPUT_SVG:{
            if (!param) return;
            svg = param;
            dxf = NULL;
            pdf = NULL;
        } break;

        case CAD_OUTPUT_DEFAULT:{
            dxf = NULL;
            pdf = NULL;
            svg = NULL;
        } break;

        case CAD_OUTPUT_MEASURE:{
            bounds.min = Vector2Zero();
            bounds.max = Vector2Zero();
            dxf = NULL;
            pdf = NULL;
            svg = NULL;
        }
    }

    output = o;
}

CadFont *LoadCadFont(const uint8_t *data, size_t size, char *chars){
    uint8_t *dataCpy = calloc(size, 1);
    memcpy(dataCpy, data, size);
    
    ttf_t *font;
    int rs = ttf_load_from_mem(dataCpy, size, &font, false);
    if (rs != TTF_DONE)
        return NULL;
    
    CadFont *cadFont = calloc(1, sizeof(CadFont));

    int maxIndex = 0;
    for (int i=0; chars[i] != '\0'; i++){
        unsigned char c = (unsigned char) chars[i];
        if (c > maxIndex)
            maxIndex = c;
    }

    cadFont->charsQ = maxIndex+1;
    cadFont->chars = calloc(cadFont->charsQ, sizeof(CadChar));

    ttf_mesh_t **glyphMeshes = calloc(cadFont->charsQ, sizeof(ttf_mesh_t*));
    for (int i=0; chars[i] != '\0'; i++){
        unsigned char c = (unsigned char) chars[i];
        if (glyphMeshes[c] != NULL)
            continue;

        int gi = ttf_find_glyph(font, chars[i]);
        if (gi < 0) continue;

        ttf_glyph2mesh(
            &font->glyphs[gi], 
            &glyphMeshes[c], 
            TTF_QUALITY_LOW, 
            TTF_FEATURES_DFLT
        );
    }

    for (int i=0; i<cadFont->charsQ; i++){
        ttf_mesh_t *m = glyphMeshes[i];
        if (m == NULL) continue;

        cadFont->chars[i].advance = font->glyphs[ttf_find_glyph(font, i)].advance;
        cadFont->chars[i].height = font->glyphs[ttf_find_glyph(font, i)].ybounds[1] - font->glyphs[ttf_find_glyph(font, i)].ybounds[0];
    
        cadFont->chars[i].verticesCount = m->nfaces * 3;
        cadFont->chars[i].vertices = calloc(
            cadFont->chars[i].verticesCount, 
            sizeof(Vector2)
        );

        // for each face
        for (int fi=0; fi<m->nfaces; fi++){
            cadFont->chars[i].vertices[fi*3] = (Vector2){
                m->vert[m->faces[fi].v1].x ,
                m->vert[m->faces[fi].v1].y 
            };

            cadFont->chars[i].vertices[fi*3+1] = (Vector2){
                m->vert[m->faces[fi].v2].x ,
                m->vert[m->faces[fi].v2].y 
            };

            cadFont->chars[i].vertices[fi*3+2] = (Vector2){
                m->vert[m->faces[fi].v3].x ,
                m->vert[m->faces[fi].v3].y 
            };
        }
    }

    for (int i=0; i<cadFont->charsQ; i++){
        if (glyphMeshes[i])
            ttf_free_mesh(glyphMeshes[i]);
    }

    free(glyphMeshes);
    ttf_free(font);
    free(dataCpy);
    cadFont->ogData = data;
    cadFont->ogDataSize = size;
    return cadFont;
}

void FreeCadFont(CadFont *font){    
    for (int i=0; i<font->charsQ; i++)
        free(font->chars[i].vertices);
    free(font->chars);
    free(font);
}

Vector2 MeasureCadText(CadFont *font, const char *text, int textLen, float fontSize, float xspacing, float yspacing){
    float maxWidth = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    for (int i = 0; text[i] != '\0'; i++){
        if (textLen > 0 && i >= textLen) break;
        unsigned char c = (unsigned char)text[i];

        if (c == ' '){
            width += font->chars['m'].advance * fontSize;
            if (text[i + 1] != '\0')
                width += xspacing;

            continue;
        }

        if (c == '\n'){
            if (width > maxWidth)
                maxWidth = width;
            height += yspacing;

            if (textLen == -1) break;
            width = 0.0f;
            continue;
        }

        CadChar *cc = &font->chars[c];
        if (!cc->vertices) continue;

        if (cc->height > height)
            height = cc->height;

        width += cc->advance * fontSize;
        if (text[i + 1] != '\0')
            width += xspacing;
    }

    if (width > maxWidth) maxWidth = width;
    return (Vector2){maxWidth, height * fontSize};
}

/* Used only for grouping lines in the PDF and DXF*/
void OpenCadPath(){
    if (output != CAD_OUTPUT_PDF || output != CAD_OUTPUT_DXF) return;
    pathOpen = true;

    if (output == CAD_OUTPUT_DXF){
        DXF_OpenPolylineBegin(dxf->f, dxf->layer, dxf->color);
    }
}

void CloseCadPath(){
    if (!pathOpen) return;

    switch (output){
        case CAD_OUTPUT_PDF:{
            if (pdfOperations == NULL) break;
            
            struct pdf_doc *pdf     = pdfOperations->pdf; 
            struct pdf_object *page = pdfOperations->page;
            float width             = pdfOperations->width;
            uint32_t color          = pdfOperations->color;
            size_t opsQ             = pdfOperations->opsQ;
            
            struct pdf_path_operation *linearOps = calloc(
                opsQ, sizeof(struct pdf_path_operation)
            );

            PdfOperation *op = pdfOperations;
            for (size_t i=0; op != NULL; i++){
                linearOps[i] = op->op;
                PdfOperation *toDel = op;
                op = toDel->next;
                free(toDel);
            }
            pdfOperations = NULL;

            pdf_add_custom_path(pdf, page, linearOps, opsQ, width, color, color);
            free(linearOps);

        } break;

        case CAD_OUTPUT_DXF:{
            DXF_OpenPolylineEnd(dxf->f);
            dxfFirstPointDone = false;
        } break;

        case CAD_OUTPUT_SVG:
        case CAD_OUTPUT_DEFAULT:break;
        case CAD_OUTPUT_MEASURE:break;
    }

    pathOpen = false;
}

static void DrawLineHelper(Vector2 A, Vector2 B, float thick, Color color){
    switch (output){
        case CAD_OUTPUT_DEFAULT:
            DrawLineEx(A, B, thick, color);
        break;

        case CAD_OUTPUT_MEASURE:{
            if (A.x < bounds.min.x) bounds.min.x = A.x;
            if (A.y < bounds.min.y) bounds.min.y = A.y;
            if (A.x > bounds.max.x) bounds.max.x = A.x;
            if (A.y > bounds.max.y) bounds.max.y = A.y;
            if (B.x < bounds.min.x) bounds.min.x = B.x;
            if (B.y < bounds.min.y) bounds.min.y = B.y;
            if (B.x > bounds.max.x) bounds.max.x = B.x;
            if (B.y > bounds.max.y) bounds.max.y = B.y;

        } break;

        case CAD_OUTPUT_PDF:{
            float height = PT_TO_MM(pdf->height);
            float x1 = PDF_MM_TO_POINT(A.x);
            float y1 = PDF_MM_TO_POINT(height - A.y);
            float x2 = PDF_MM_TO_POINT(B.x);
            float y2 = PDF_MM_TO_POINT(height - B.y);
            uint32_t c = PDF_ARGB(0, color.r, color.g, color.b);

            if (pathOpen){
                AddLine2Operations(
                    &pdfOperations,
                    pdf, NULL,
                    x1, y1,
                    x2, y2,
                    0,
                    c
                );
            }
            
            else {
                pdf_add_line(
                    pdf, NULL,
                    x1, y1,
                    x2, y2,
                    0,
                    c
                );
            }
        } break;

        case CAD_OUTPUT_DXF:{
            if (pathOpen){
                if (!dxfFirstPointDone){
                    DXF_OpenPolylinePoint(dxf->f, A.x, A.y, 0.0f);
                    dxfFirstPointDone = true;
                }
                DXF_OpenPolylinePoint(dxf->f, B.x, B.y, 0.0f);
            }
            
            else {
                DXF_Line(
                    dxf->f, dxf->layer, dxf->color, 
                    A.x, A.y, 0.0f, 
                    B.x, B.y, 0.0f
                );
            }
        } break;

        case CAD_OUTPUT_SVG:{
            uint32_t c = *(uint32_t *) &color;
            SVG_line(svg, c, 1.0f, A.x, A.y, B.x, B.y);
        } break;
    }
}

void DrawCadLine(Vector2 startPos, Vector2 endPos, float thick, Color color){
    
    // If there is no mask, just draw the line
    if (!mask.points || mask.pointsQ < 3){
        DrawLineHelper(startPos, endPos, thick, color);
        return;
    }

    // If the line is inside the mask, draw the line
    bool aInside = CheckCollisionPointPoly(startPos, mask.points, mask.pointsQ);
    bool bInside = CheckCollisionPointPoly(endPos, mask.points, mask.pointsQ);

    if (aInside && bInside){
        DrawLineHelper(startPos, endPos, thick, color);
        return;
    }

    // At this point, at least one point is outside the mask
    Vector2 intersections[2];
    int count = 0;

    for (int i = 0; i < mask.pointsQ; i++){
        Vector2 p1 = mask.points[i];
        Vector2 p2 = mask.points[(i + 1) % mask.pointsQ];
        Vector2 hit;

        if (CheckCollisionLines(startPos, endPos, p1, p2, &hit)){
            if (count < 2){
                intersections[count++] = hit;
            }
        }
    }

    if (count <= 0) return;
    if (count >= 2){
        DrawLineHelper(intersections[0], intersections[1], thick, color);
        return;
    }

    if (aInside){
        DrawLineHelper(startPos, intersections[0], thick, color);
        return;
    }

    DrawLineHelper(intersections[0], endPos, thick, color);
}

void DrawCadBG(Texture2D *texture, Vector2 *points, int pointCount, Color tint){
    switch (output){
        case CAD_OUTPUT_DEFAULT:
            DrawTextureFromScreenSpace(texture, points, pointCount, tint);
        break;

        case CAD_OUTPUT_MEASURE:{
            for (int i=0; i<pointCount; i++){
                if (points[i].x < bounds.min.x) bounds.min.x = points[i].x;
                if (points[i].y < bounds.min.y) bounds.min.y = points[i].y;
                if (points[i].x > bounds.max.x) bounds.max.x = points[i].x;
                if (points[i].y > bounds.max.y) bounds.max.y = points[i].y;
            }
        } break;

        case CAD_OUTPUT_PDF:{
            float height = PT_TO_MM(pdf->height);

            float xs[pointCount-1];
            float ys[pointCount-1];
            for (int i=0; i<pointCount-1; i++){
                xs[i] = PDF_MM_TO_POINT(points[i].x);
                ys[i] = PDF_MM_TO_POINT(height - points[i].y);
            }

            pdf_add_filled_polygon(
                pdf, NULL, 
                xs, ys, 
                pointCount-1, 
                0,
                PDF_WHITE
            );
        } break;

        // There are no solid figures in dxf :(
        case CAD_OUTPUT_DXF:break;

        case CAD_OUTPUT_SVG:{
            uint32_t c = *(uint32_t *) &tint;
            SVG_polygon(svg, (SVG_Point*) points, pointCount, c, c, 0.0f);
        } break;
        
    }
}

static void DrawTextGPU(CadFont *font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float xspacing, float yspacing, CadTextAlign align, Color tint){
    Vector2 advance = Vector2Zero();
    for (const char *c = text; *c != '\0'; c++){
        if (*c == '\n') advance.y -= yspacing;
    }
    
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0.0f);
    rlRotatef(rotation, 0, 0, 1);
    rlTranslatef(-origin.x, -origin.y, 0.0f);
    rlScalef(fontSize, fontSize, 1.0f);

    rlBegin(RL_TRIANGLES);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    
    float maxW = 0.0f;
    float w = 0.0f;
    for (int i=0; text[i] != '\0'; i++){
        unsigned char c = text[i];
        switch (c){
            case ' ':{
                w += font->chars['m'].advance;
                continue;
            } break;

            case '\n':{
                if (w > maxW) maxW = w;
                w = 0;
                continue;
            } break;
        }

        CadChar *cc = &font->chars[c];
        if (!cc->vertices) continue;
        w += cc->advance;
    }
    if (w > maxW) maxW = w;

    float lineW = 0.0f;
    bool newLine = true;
    for (int i=0; text[i] != '\0'; i++){
        unsigned char c = text[i];

        // Getting the line width
        if (newLine){
            lineW = 0;
            for (int o=i; text[o] != '\n' && text[o] != '\0'; o++){
                unsigned char c = text[o];
                if (c == ' '){
                    lineW += font->chars['m'].advance;
                    continue;
                }

                CadChar *cc = &font->chars[c];
                if (!cc->vertices) continue;
                lineW += cc->advance; 
            }
            newLine = false;
        }

        // SPECIAL CHARS
        switch (c){
            case ' ':{
                advance = Vector2Add(
                    advance, 
                    (Vector2){
                        font->chars['m'].advance, 
                        0
                    }
                );
                
                continue;
            } break;

            case '\n':{
                advance = (Vector2){0, advance.y + yspacing};
                newLine = true;
                continue;
            } break;
        }
            
        CadChar *cc = &font->chars[c];
        if (!cc->vertices) continue;

        rlPushMatrix();
        float xOffset = 0.0f;
        switch (align){
            case CAD_ALIGN_LEFT: break;
            case CAD_ALIGN_CENTER: xOffset += (maxW - lineW)*0.5f; break;
            case CAD_ALIGN_RIGHT:  xOffset += (maxW - lineW);      break;
        }

        rlTranslatef(advance.x + xOffset, advance.y, 0);
        for (int f=0; f<cc->verticesCount/3; f++){
            Vector2 p0 = (Vector2){cc->vertices[f*3].x, 0-cc->vertices[f*3].y};
            Vector2 p1 = (Vector2){cc->vertices[f*3+1].x, 0-cc->vertices[f*3+1].y};
            Vector2 p2 = (Vector2){cc->vertices[f*3+2].x, 0-cc->vertices[f*3+2].y};

            rlVertex2f(p0.x, p0.y);
            rlVertex2f(p1.x, p1.y);
            rlVertex2f(p2.x, p2.y);

        }
        rlPopMatrix();

        advance = Vector2Add(
            advance, 
            (Vector2){
                cc->advance, 
                0
            }
        );
    
    }

    rlEnd();
    rlPopMatrix();

}

static void DrawTextPDF(struct pdf_doc *pdf, CadFont *font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float xspacing, float yspacing, CadTextAlign align, Color tint){
    float height = PT_TO_MM(pdf->height);
    Vector2 tpos = Vector2Subtract(position, Vector2RotateDegrees(origin, rotation));

    pdf_set_font_ttf_mem(pdf, font->ogData, font->ogDataSize);

    Vector2 lineAdvance = Vector2RotateDegrees(
        (Vector2){ 0.0f, yspacing * fontSize },
        rotation
    );

    int lines = 1;
    for (const char *c = text; *c != '\0'; c++) {
        if (*c == '\n') lines++;
    }

    tpos = Vector2Subtract(
        tpos,
        Vector2Scale(lineAdvance, (float)(lines - 1))
    );


    // Get max line width
    float maxW = 0.0f;
    float w = 0.0f;

    for (const char *c = text; *c != '\0'; c++){
        if (*c == '\n'){
            if (w > maxW) maxW = w;
            w = 0;
            continue;
        }

        if (*c == ' '){
            w += font->chars['m'].advance;
            continue;
        }

        CadChar *cc = &font->chars[(unsigned char)*c];
        if (!cc->vertices) continue;

        w += cc->advance;
    }

    if (w > maxW) maxW = w;

    const char *start = text;
    const char *end = text;
    int line = 0;

    while (1) {
        while (*end && *end != '\n') end++;

        int len = (int)(end - start);

        char buffer[len + 1];
        memcpy(buffer, start, len);
        buffer[len] = '\0';

        // Current line width
        float lineW = 0.0f;
        for (int i = 0; i < len; i++){
            unsigned char c = buffer[i];

            if (c == ' '){
                lineW += font->chars['m'].advance;
                continue;
            }

            CadChar *cc = &font->chars[c];
            if (!cc->vertices) continue;

            lineW += cc->advance;
        }

        Vector2 pos = Vector2Add(
            tpos,
            Vector2Scale(lineAdvance, (float)line)
        );

        float xOffset = 0.0f;

        switch (align){
            case CAD_ALIGN_LEFT: break;
            case CAD_ALIGN_CENTER: xOffset = (maxW - lineW) * 0.5f; break;
            case CAD_ALIGN_RIGHT:  xOffset = (maxW - lineW);        break;
        }

        pos.x += xOffset * fontSize;
        pdf_add_text_rotate(
            pdf,
            NULL,
            buffer,
            PDF_MM_TO_POINT(fontSize),
            PDF_MM_TO_POINT(pos.x),
            PDF_MM_TO_POINT(height - pos.y),
            -rotation * DEG2RAD,
            PDF_RGB(tint.r, tint.g, tint.b)
        );

        if (*end == '\0') break;
        start = ++end;
        line++;
    }
}

static void DrawTextCPU(void *out, CadFont *font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float xspacing, float yspacing, CadTextAlign align, Color tint){
    Vector2 advance = Vector2Zero();

    for (const char *c = text; *c != '\0'; c++){
        if (*c == '\n')
            advance.y -= yspacing;
    }

    Matrix transform =
        MatrixMultiply(
            MatrixScale(fontSize, fontSize, 1.0f),
            MatrixMultiply(
                MatrixTranslate(-origin.x, -origin.y, 0.0f),
                MatrixMultiply(
                    MatrixRotateZ(rotation * DEG2RAD),
                    MatrixTranslate(position.x, position.y, 0.0f)
                )
            )
        );

    // Get max line width
    float maxW = 0.0f;
    float w = 0.0f;

    for (int i = 0; text[i] != '\0'; i++){
        unsigned char c = text[i];

        switch (c){
            case ' ':{
                w += font->chars['m'].advance;
                continue;
            }

            case '\n':{
                if (w > maxW)
                    maxW = w;

                w = 0;
                continue;
            }
        }

        CadChar *cc = &font->chars[c];

        if (!cc->vertices)
            continue;

        w += cc->advance;
    }

    if (w > maxW)
        maxW = w;

    // Draw Characters
    float lineW = 0.0f;
    bool newLine = true;

    for (int i = 0; text[i] != '\0'; i++){
        unsigned char c = text[i];

        // Getting the line width
        if (newLine){
            lineW = 0;

            for (int o = i; text[o] != '\n' && text[o] != '\0'; o++){
                unsigned char c = text[o];

                if (c == ' '){
                    lineW += font->chars['m'].advance;
                    continue;
                }

                CadChar *cc = &font->chars[c];

                if (!cc->vertices)
                    continue;

                lineW += cc->advance;
            }

            newLine = false;
        }

        // SPECIAL CHARS
        switch (c){
            case ' ':{
                advance = Vector2Add(
                    advance,
                    (Vector2){
                        font->chars['m'].advance,
                        0
                    }
                );

                continue;
            }

            case '\n':{
                advance = (Vector2){
                    0,
                    advance.y + yspacing
                };

                newLine = true;
                continue;
            }
        }

        CadChar *cc = &font->chars[c];

        if (!cc->vertices)
            continue;

        float xOffset = 0.0f;

        switch (align){
            case CAD_ALIGN_LEFT:
                break;

            case CAD_ALIGN_CENTER:
                xOffset += (maxW - lineW) * 0.5f;
                break;

            case CAD_ALIGN_RIGHT:
                xOffset += (maxW - lineW);
                break;
        }

        Matrix charTransform =
            MatrixMultiply(
                MatrixTranslate(
                    advance.x + xOffset,
                    advance.y,
                    0.0f
                ),
                transform
            );

        for (int f = 0; f < cc->verticesCount / 3; f++){

            Vector2 p0 = { cc->vertices[f * 3].x, -cc->vertices[f * 3].y };
            Vector2 p1 = { cc->vertices[f * 3 + 1].x, -cc->vertices[f * 3 + 1].y };
            Vector2 p2 = { cc->vertices[f * 3 + 2].x, -cc->vertices[f * 3 + 2].y };
            Vector3 v0 = Vector3Transform( (Vector3){ p0.x, p0.y, 0 }, charTransform );
            Vector3 v1 = Vector3Transform( (Vector3){ p1.x, p1.y, 0 }, charTransform );
            Vector3 v2 = Vector3Transform( (Vector3){ p2.x, p2.y, 0 }, charTransform );
            uint32_t c = *(uint32_t *) &tint;

            // ------------------------------------ DRAWING TIME
            switch (output){
                case CAD_OUTPUT_SVG: {
                    SVG *psvg = out;
                    SVG_polygon(
                        psvg, 
                        (SVG_Point[]){
                            {v0.x, v0.y}, 
                            {v1.x, v1.y}, 
                            {v2.x, v2.y}
                        }, 
                        3, 
                        c, c, 
                        0.1f
                    );
                } break;

                case CAD_OUTPUT_MEASURE: {
                    // v0
                    if (v0.x < bounds.min.x) bounds.min.x = v0.x;
                    if (v0.y < bounds.min.y) bounds.min.y = v0.y;
                    if (v0.x > bounds.max.x) bounds.max.x = v0.x;
                    if (v0.y > bounds.max.y) bounds.max.y = v0.y;
                    
                    // v1
                    if (v1.x < bounds.min.x) bounds.min.x = v1.x;
                    if (v1.y < bounds.min.y) bounds.min.y = v1.y;
                    if (v1.x > bounds.max.x) bounds.max.x = v1.x;
                    if (v1.y > bounds.max.y) bounds.max.y = v1.y;

                    // v2
                    if (v2.x < bounds.min.x) bounds.min.x = v2.x;
                    if (v2.y < bounds.min.y) bounds.min.y = v2.y;
                    if (v2.x > bounds.max.x) bounds.max.x = v2.x;
                    if (v2.y > bounds.max.y) bounds.max.y = v2.y;

                } break;

                // to be done
                case CAD_OUTPUT_DXF: break;
                
                case CAD_OUTPUT_DEFAULT: break; // why would you do this? use the gpu version
                case CAD_OUTPUT_PDF: break; // use the pdf version, dah
            }
            
        }

        advance = Vector2Add(
            advance,
            (Vector2){
                cc->advance,
                0
            }
        );
    }

}

void DrawCadText(CadFont *font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float xspacing, float yspacing, CadTextAlign align, Color tint){
    switch (output){
        case CAD_OUTPUT_DEFAULT: DrawTextGPU(font, text, position, origin, rotation, fontSize, xspacing, yspacing, align, tint); break;
        case CAD_OUTPUT_MEASURE: DrawTextCPU(NULL, font, text, position, origin, rotation, fontSize, xspacing, yspacing, align, tint); break;
        case CAD_OUTPUT_PDF: DrawTextPDF(pdf, font, text, position, origin, rotation, fontSize, xspacing, yspacing, align, tint); break;
        case CAD_OUTPUT_SVG: DrawTextCPU(svg, font, text, position, origin, rotation, fontSize, xspacing, yspacing, align, tint); break;;
        case CAD_OUTPUT_DXF: break;
    }
}

void SetCadScissors(Rectangle rect){
    switch (output){
        case CAD_OUTPUT_DEFAULT:{
            BeginScissorMode(rect.x, rect.y, rect.width, rect.height);
        } break;

        case CAD_OUTPUT_PDF:{
            float height = PT_TO_MM(pdf->height);
            pdf_begin_clip_rect(
                pdf, NULL, 
                PDF_MM_TO_POINT(rect.x), 
                PDF_MM_TO_POINT(height - rect.y - rect.height), 
                PDF_MM_TO_POINT(rect.width), 
                PDF_MM_TO_POINT(rect.height)
            );
        } break;

        // There is no scissors in dxf format...
        // But it could use a mask?
        case CAD_OUTPUT_DXF: break;
        case CAD_OUTPUT_SVG: break;

        // no need to do anything
        case CAD_OUTPUT_MEASURE: break;
    }
}

void UnsetCadScissors(){
    switch (output){
        case CAD_OUTPUT_DEFAULT: {
            EndScissorMode();
        } break;

        case CAD_OUTPUT_PDF: {
            pdf_end_clip(pdf, NULL);
        } break;

        // There is no scissors in dxf format...
        // But it could use a mask?
        case CAD_OUTPUT_DXF: break;
        case CAD_OUTPUT_SVG: break;

        // no need to do anything
        case CAD_OUTPUT_MEASURE: break;
    }
}

void SetCadMask(Vector2 *points, int pointC){
    if (!points) return;
    mask.pointsQ = pointC;
    mask.points = calloc(pointC, sizeof(Vector2));
    for (int i=0; i<pointC; i++)
        mask.points[i] = points[i];
}

void UnsetCadMask(){
    if (mask.points){
        free(mask.points);
        mask.points = NULL;
        mask.pointsQ = 0;
    }
}

void DrawCadSticker(Sticker *s, Vector2 pos, float scale){
    switch (output){
        case CAD_OUTPUT_DEFAULT:{
            DrawTextureEx(
                s->tex, 
                (Vector2){
                    pos.x - (s->tex.width*scale)/2.0f,
                    pos.y - (s->tex.height*scale)/2.0f
                }, 
                0.0f, 
                scale, 
                WHITE
            );
        } break;

        case CAD_OUTPUT_PDF:{
            float height = PT_TO_MM(pdf->height);

            pdf_add_rgb24(
                pdf, NULL, 
                PDF_MM_TO_POINT(pos.x - (s->img.width*scale)/2.0f), 
                PDF_MM_TO_POINT(height - (pos.y + (s->img.height*scale)/2.0f)), 
                PDF_MM_TO_POINT(s->img.width * scale), 
                PDF_MM_TO_POINT(s->img.height * scale), 
                s->img.data, 
                s->img.width, 
                s->img.height
            );
        } break;

        // There are no images in dxf nor svg...
        case CAD_OUTPUT_DXF: break;
        case CAD_OUTPUT_SVG: break;

        // no need to do anything
        case CAD_OUTPUT_MEASURE: break;
    }
}

CadBounds GetCadBounds(){
    return bounds;
}