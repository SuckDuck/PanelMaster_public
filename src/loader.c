// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cJSON.h"
#include "caddraw.h"
#include "documentation.h"
#include "origami.h"
#include "surfaces.h"
#include "loader.h"
#include "main.h"
#include "titleblock.h"
#include "stickers.h"

extern void Solver();
static char invalidSurfaceFilename[OG_PATH_MAX];
static char errCode[64];
static FileGeneric *loadedAssemblyFile = NULL;

/*
VerificateConstraintV0() does not check circular dependencies.
When loading assembly, anchor constraints with type > 0 should have proper references in A and/or B
*/

/* <== Generics ================================================> */

#define HANDLE_GENERAL_ERRORS do { \
    if (f.version < 0){ \
        if (ErrorIs(LDRV0_ERR_FILE, LDRV0_ERR_MISSING, 0)) { \
            OG_PushLog("File %s not found", path); \
            return false; \
        } \
\
        if (ErrorIs(LDRV0_ERR_FILE, LDRV0_ERR_INVALID, LDRV0_ERR_TYPE, 0)) { \
            OG_PushLog("File %s has an invalid ext", path); \
            return false; \
        } \
\
        if (ErrorIs(LDRV0_ERR_FILE, LDRV0_ERR_IO, 0)) { \
            OG_PushLog("Failed to load file %s", path); \
            return false; \
        } \
\
        if (ErrorIs(LDRV0_ERR_FILE, LDRV0_ERR_CORRUPTED, 0) || \
            ErrorIs(LDRV0_ERR_FILE, LDRV0_ERR_INVALID, 0)) { \
            OG_PushLog("File %s is invalid or corrupted", path); \
            return false; \
        } \
    } \
} while (0)

static void SetError(int first, ...){
    va_list args;
    va_start(args, first);

    int i = 0;
    int c = first;

    while (c != 0 && i < sizeof(errCode) - 1) {
        errCode[i++] = (char)c;
        c = va_arg(args, int);
    }

    errCode[i] = '\0';

    va_end(args);
}

static void ErrorAppend(int first, ...){
    va_list args;
    va_start(args, first);

    size_t i = strlen(errCode);
    int c = first;

    while (c != 0 && i < sizeof(errCode) - 1){
        errCode[i++] = (char)c;
        c = va_arg(args, int);
    }

    errCode[i] = '\0';

    va_end(args);
}

static void ErrorPrepend(int first, ...){
    char temp[64];

    va_list args;
    va_start(args, first);
    size_t i = 0;
    int c = first;

    while (c != 0 && i < sizeof(temp) - 1){
        temp[i++] = (char)c;
        c = va_arg(args, int);
    }

    va_end(args);

    temp[i] = '\0';
    size_t oldLen = strlen(errCode);
    if (i + oldLen >= sizeof(errCode))
        return;

    memmove(errCode + i, errCode, oldLen + 1);
    memcpy(errCode, temp, i);
}

bool ErrorIs(int first, ...){
    char temp[32];
    va_list args;
    va_start(args, first);

    int i = 0;
    int c = first;
    while (c != 0 && i < sizeof(temp) - 1) {
        temp[i++] = (char)c;
        c = va_arg(args, int);
    }

    temp[i] = '\0';
    va_end(args);
    return strcmp(errCode, temp) == 0;
}

bool ErrorHas(int first, ...){
    va_list args;
    va_start(args, first);
    int wanted = first;

    while (wanted != 0){
        bool found = false;

        for (int i = 0; errCode[i] != '\0'; i++){
            if (errCode[i] == wanted){
                found = true;
                break;
            }
        }

        if (!found){
            va_end(args);
            return false;
        }

        wanted = va_arg(args, int);
    }

    va_end(args);
    return true;
}

static int JsonRequire( cJSON *j, const char *key, cJSON_bool (*type)(const cJSON *), cJSON **out, int first, ...){
    *out = cJSON_GetObjectItem(j, key);

    if (!*out || !type(*out)) {
        char temp[64];
        int i = 0;

        temp[i++] = !*out
            ? LDRV0_ERR_MISSING
            : LDRV0_ERR_INVALID;

        va_list args;
        va_start(args, first);
        int c = first;

        while (c != 0 && i < sizeof(temp)-1) {
            temp[i++] = (char)c;
            c = va_arg(args, int);
        }

        temp[i] = '\0';
        va_end(args);
        strcpy(errCode, temp);
        return LDRV0_ERR_CODE;
    }

    return LDRV0_OK;
}

#define JSON_REQUIRE_RET(var, parent, type, ...) \
    cJSON *var; \
    rs = JsonRequire(parent, #var, type, &var, __VA_ARGS__, 0); \
    if (rs < 0) \
        return rs

#define JSON_REQUIRE_GOTO(var, parent, type, ...) \
    cJSON *var; \
    rs = JsonRequire(parent, #var, type, &var, __VA_ARGS__, 0); \
    if (rs < 0) \
        goto error

static FileGeneric LoadFileGeneric(char *path, char *ext){
    FileGeneric fileError = (FileGeneric){NULL, -1};
    FileGeneric out = {0};
    int rs = 0;

    if (!FileExists(path)){
        SetError(LDRV0_ERR_FILE, LDRV0_ERR_MISSING, 0);
        return fileError;
    }

    if (!IsFileExtension(path, ext)){
        SetError(LDRV0_ERR_FILE, LDRV0_ERR_INVALID, LDRV0_ERR_TYPE, 0);
        return fileError;
    }

    char *t = LoadFileText(path);
    if (t == NULL){
        SetError(LDRV0_ERR_FILE, LDRV0_ERR_IO, 0);
        return fileError;
    }

    out.root = cJSON_Parse(t);
    UnloadFileText(t);
    
    if (out.root == NULL){
        SetError(LDRV0_ERR_FILE, LDRV0_ERR_CORRUPTED, 0);
        return fileError;
    }

    if (!cJSON_IsObject(out.root)){
        SetError(LDRV0_ERR_FILE, LDRV0_ERR_INVALID, 0);
        goto error;
    }

    JSON_REQUIRE_GOTO(version, out.root, cJSON_IsNumber, LDRV0_ERR_FILE, LDRV0_ERR_VERSION);
    
    out.version = version->valueint;
    return out;

    error:
    cJSON_Delete(out.root);
    fileError.version = rs;
    return fileError;
}

static void FreeFileGeneric(FileGeneric f){
    cJSON_Delete(f.root);
}

/* <== Documentation ===========================================> */

static int VerificateDocumentV0(cJSON *j){
    int rs = LDRV0_OK;
    
    JSON_REQUIRE_RET(width, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_WIDTH);
    JSON_REQUIRE_RET(height, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_HEIGHT);
    JSON_REQUIRE_RET(pages, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_SECTION);
    
    cJSON *page;
    cJSON_ArrayForEach(page, pages){
        if (!cJSON_IsObject(page)){
            rs = LDRV0_ERR_CODE;
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_INVALID);
            return rs;
        }

        JSON_REQUIRE_RET(name, page, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_NAME);
        JSON_REQUIRE_RET(draw_frame, page, cJSON_IsBool, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_DRAW, LDRV0_ERR_FRAME);
        
        JSON_REQUIRE_RET(frame, page, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_FRAME, LDRV0_ERR_SECTION);
        JSON_REQUIRE_RET(top, frame, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_FRAME, LDRV0_ERR_TOP);
        JSON_REQUIRE_RET(bottom, frame, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_FRAME, LDRV0_ERR_BOTTOM);
        JSON_REQUIRE_RET(left, frame, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_FRAME, LDRV0_ERR_LEFT);
        JSON_REQUIRE_RET(right, frame, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_FRAME, LDRV0_ERR_RIGHT);

        JSON_REQUIRE_RET(entities, page, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_PAGE, LDRV0_ERR_ENTITY, LDRV0_ERR_SECTION);
        cJSON *entity;
        cJSON_ArrayForEach(entity, entities){
            if (!cJSON_IsObject(entity)){
                rs = LDRV0_ERR_CODE;
                SetError(LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_INVALID);
                return rs;
            }

            JSON_REQUIRE_RET(type, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TYPE);
            JSON_REQUIRE_RET(name, entity, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_NAME);
            JSON_REQUIRE_RET(pos, entity, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_POS);
            JSON_REQUIRE_RET(x, pos, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_X);
            JSON_REQUIRE_RET(y, pos, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_Y);

            switch (type->valueint){
                case PAGE_ENTITY_SURFACE:{
                    JSON_REQUIRE_RET(surface, entity, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_SURFACE);
                    JSON_REQUIRE_RET(scale, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_SIZE);
                    JSON_REQUIRE_RET(flags, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_FLAGS);
                    JSON_REQUIRE_RET(annotationsFontSize, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_ANNOTATION, LDRV0_ERR_SIZE);
                    JSON_REQUIRE_RET(annotationsArrowSize, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_ANNOTATION, LDRV0_ERR_SIZE, '2');
                    JSON_REQUIRE_RET(lod, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, '_', 'L', 'O', 'D');
                } break;

                case PAGE_ENTITY_TEXT:{
                    JSON_REQUIRE_RET(text, entity, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TEXT);
                    JSON_REQUIRE_RET(fontSize, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TEXT, LDRV0_ERR_SIZE);
                    
                    JSON_REQUIRE_RET(color, entity, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TINT);
                    JSON_REQUIRE_RET(r, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TINT, LDRV0_ERR_R);
                    JSON_REQUIRE_RET(g, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TINT, LDRV0_ERR_G);
                    JSON_REQUIRE_RET(b, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TINT, LDRV0_ERR_B);

                    JSON_REQUIRE_RET(alignment, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_ALIGN);

                } break;
    
                case PAGE_ENTITY_TITLEBLOCK:{
                    JSON_REQUIRE_RET(block, entity, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TITLEBLOCK);
                    JSON_REQUIRE_RET(properties, entity, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_PROP, LDRV0_ERR_SECTION);
                    cJSON *prop;
                    cJSON_ArrayForEach(prop, properties){
                        if (!cJSON_IsObject(prop)){
                            rs = LDRV0_ERR_CODE;
                            SetError(LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_PROP, LDRV0_ERR_INVALID);
                            return rs;
                        }

                        JSON_REQUIRE_RET(rect, prop, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_PROP, LDRV0_ERR_RECT);
                        JSON_REQUIRE_RET(content, prop, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_TEXT);
                    }

                } break;

                case PAGE_ENTITY_STICKER:{
                    JSON_REQUIRE_RET(srcPath, entity, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_FILE, LDRV0_ERR_NAME);
                    JSON_REQUIRE_RET(scale, entity, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_SIZE);
                } break;
                
                default:
                    rs = LDRV0_ERR_CODE;
                    SetError(LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_ENTITY, LDRV0_ERR_INVALID, LDRV0_ERR_TYPE);
                    return rs;
                break;
            }
        }

    }

    return LDRV0_OK;
}

static int LoadDocumentV0(cJSON *j){
    int rs = LDRV0_OK;

    int width = cJSON_GetObjectItem(j, "width")->valueint;
    int height = cJSON_GetObjectItem(j, "height")->valueint;
    
    mainDocument.width = width;
    mainDocument.height = height;

    cJSON *page;
    cJSON_ArrayForEach(page, cJSON_GetObjectItem(j, "pages")){
        char *name = cJSON_GetObjectItem(page, "name")->valuestring;
        bool drawFrame = cJSON_GetObjectItem(page, "draw_frame")->valueint>0 ? true:false;
        float top = cJSON_GetObjectItem(cJSON_GetObjectItem(page, "frame"), "top")->valuedouble;
        float bottom = cJSON_GetObjectItem(cJSON_GetObjectItem(page, "frame"), "bottom")->valuedouble;
        float left = cJSON_GetObjectItem(cJSON_GetObjectItem(page, "frame"), "left")->valuedouble;
        float right = cJSON_GetObjectItem(cJSON_GetObjectItem(page, "frame"), "right")->valuedouble;
        
        Page *p = NewPage(&mainDocument, name);
        p->drawFrame = drawFrame;
        p->frame.top = top;
        p->frame.bottom = bottom;
        p->frame.left = left;
        p->frame.right = right;

        cJSON *entity;
        cJSON_ArrayForEach(entity, cJSON_GetObjectItem(page, "entities")){
            PageEntityType type = cJSON_GetObjectItem(entity, "type")->valueint;
            char *eName = cJSON_GetObjectItem(entity, "name")->valuestring;
            Vector2 pos = {
                cJSON_GetObjectItem(cJSON_GetObjectItem(entity, "pos"), "x")->valuedouble,
                cJSON_GetObjectItem(cJSON_GetObjectItem(entity, "pos"), "y")->valuedouble
            };

            switch (type){
                case PAGE_ENTITY_SURFACE: {
                    Surface *s = NULL;
                    for (int i=0; i<TREELISTS_Q; i++){
                        if (strcmp(treeList[i]->baseSurface.name, cJSON_GetObjectItem(entity, "surface")->valuestring) == 0){
                            s = &treeList[i]->baseSurface;
                            break;
                        }
                    }

                    float scale = cJSON_GetObjectItem(entity, "scale")->valuedouble;
                    int flags = cJSON_GetObjectItem(entity, "flags")->valueint;
                    float annotationsFontSize = cJSON_GetObjectItem(entity, "annotationsFontSize")->valuedouble;
                    float annotationsArrowSize = cJSON_GetObjectItem(entity, "annotationsArrowSize")->valuedouble;
                    int lod = cJSON_GetObjectItem(entity, "lod")->valueint;

                    PageEntitySurface *pes = AddSurfaceEntity(p, s);
                    free(pes->entity->name);
                    pes->entity->name = strdup(eName);
                    pes->entity->pos = pos;

                    pes->scale = scale;
                    pes->flags = flags;
                    pes->annotationsFontSize = annotationsFontSize;
                    pes->annotationsArrowSize = annotationsArrowSize;
                    pes->lod = lod;
                } break;
                
                case PAGE_ENTITY_TEXT: {
                    char *text = cJSON_GetObjectItem(entity, "text")->valuestring;
                    float fontSize = cJSON_GetObjectItem(entity, "fontSize")->valuedouble;
                    Color color = {
                        cJSON_GetObjectItem(cJSON_GetObjectItem(entity, "color"), "r")->valuedouble,
                        cJSON_GetObjectItem(cJSON_GetObjectItem(entity, "color"), "g")->valuedouble,
                        cJSON_GetObjectItem(cJSON_GetObjectItem(entity, "color"), "b")->valuedouble,
                        255
                    };

                    CadTextAlign alignment = cJSON_GetObjectItem(entity, "alignment")->valueint;
                    
                    PageEntityText *pet = AddTextEntity(p, text);
                    free(pet->entity->name);
                    pet->entity->name = strdup(eName);
                    pet->entity->pos = pos;
                    
                    if (pet->text) 
                        free(pet->text);
                    pet->text = strdup(text);
                    pet->fontSize = fontSize;
                    pet->color = color;
                    pet->alignment = alignment;

                } break;
                
                case PAGE_ENTITY_TITLEBLOCK: {
                    char *blockName = cJSON_GetObjectItem(entity, "block")->valuestring;
                    TitleBlock *tb = NULL;
                    for (int i=0; i<TITLEBLOCKS_Q; i++){
                        if (strcmp(GetFileName(titleBlocks[i]->sourcePath), blockName) == 0){
                            tb = titleBlocks[i];
                            break;
                        }
                    }

                    PageEntityTitleBlock *petb = AddTitleBlockEntity(p, tb);
                    free(petb->entity->name);
                    petb->entity->name = strdup(eName);
                    petb->entity->pos = pos;

                    cJSON *properties = cJSON_GetObjectItem(entity, "properties");
                    cJSON *prop;
                    cJSON_ArrayForEach(prop, properties){
                        char *rect = cJSON_GetObjectItem(prop, "rect")->valuestring;
                        char *content = cJSON_GetObjectItem(prop, "content")->valuestring;
                        
                        for (PageEntityTitleBlockProp *p = petb->properties; p != NULL; p = p->next){
                            if (strcmp(p->r->label, rect) == 0){
                                if (p->content)
                                    free(p->content);
                                p->content = strdup(content);
                                break;
                            }
                        }
                    }

                } break;

                case PAGE_ENTITY_STICKER: {
                    char *stickerPath = cJSON_GetObjectItem(entity, "srcPath")->valuestring;
                    float scale = cJSON_GetObjectItem(entity, "scale")->valuedouble;
                    LoadSticker(stickerPath, NULL);
                    
                    PageEntitySticker *pes = AddStickerEntity(p, lastAddedSticker);
                    pes->entity->pos = pos;
                    pes->scale = scale;

                    free(pes->entity->name);
                    pes->entity->name = strdup(eName);
                    
                } break;
            }

        }
    }

    return rs;
}

/* <== TitleBlocks =============================================> */

static int VerificateTitleBlockV0(cJSON *j){
    int rs = LDRV0_OK;

    // ============= COMPROBATIONS SECTION    
    // Size
    JSON_REQUIRE_RET(size, j, cJSON_IsObject, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_SIZE);
    JSON_REQUIRE_RET(w, size, cJSON_IsNumber,LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_SIZE, LDRV0_ERR_WIDTH);
    JSON_REQUIRE_RET(h, size, cJSON_IsNumber,LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_SIZE, LDRV0_ERR_HEIGHT);

    // Lines
    JSON_REQUIRE_RET(lines, j, cJSON_IsArray, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_LINE, LDRV0_ERR_SECTION);
    cJSON *line;
    size_t linesQ = 0;
    cJSON_ArrayForEach(line, lines){
        if (!cJSON_IsObject(line)){
            SetError(LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_LINE, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(t, line, cJSON_IsNumber, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_LINE, LDRV0_ERR_T);
        JSON_REQUIRE_RET(type, line, cJSON_IsNumber, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_LINE, LDRV0_ERR_TYPE);
        linesQ++;
    }

    // Rects
    JSON_REQUIRE_RET(rects, j, cJSON_IsArray, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_SECTION);
    cJSON *rect;
    cJSON_ArrayForEach(rect, rects){
        if (!cJSON_IsObject(rect)){
            SetError(LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(lines, rect, cJSON_IsArray, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_LINE, LDRV0_ERR_SECTION);
        cJSON *line;
        cJSON_ArrayForEach(line, lines){
            if (!cJSON_IsNumber(line)){
                SetError(LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_LINE);
                return LDRV0_ERR_CODE;
            }   
        }

        JSON_REQUIRE_RET(textSize, rect, cJSON_IsNumber, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_TEXT, LDRV0_ERR_SIZE);
        JSON_REQUIRE_RET(textPos, rect, cJSON_IsNumber, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_TEXT, LDRV0_ERR_POS);
        JSON_REQUIRE_RET(label, rect, cJSON_IsString, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_LABEL);
        JSON_REQUIRE_RET(content, rect, cJSON_IsString, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_TEXT);
        JSON_REQUIRE_RET(externalContent, rect, cJSON_IsBool, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_EXTERNAL, LDRV0_ERR_TEXT);
        JSON_REQUIRE_RET(noLabel, rect, cJSON_IsBool, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_RECT, LDRV0_ERR_NO, LDRV0_ERR_LABEL);
    }

    return LDRV0_OK;
}

static int LoadTitleBlockV0(cJSON *j){
    int rs = LDRV0_OK;

    rs = VerificateTitleBlockV0(j);
    if ( rs != LDRV0_OK )
        return rs;

    cJSON *size = cJSON_GetObjectItem(j, "size");
    cJSON *w = cJSON_GetObjectItem(size, "w");
    cJSON *h = cJSON_GetObjectItem(size, "h");

    TitleBlock *tb = calloc(1, sizeof(TitleBlock));
    tb->size = (Vector2){
        w->valuedouble, 
        h->valuedouble
    };
    
    cJSON *lines = cJSON_GetObjectItem(j, "lines");
    TitleBlockLine *titleBlockLines[cJSON_GetArraySize(lines)];
    int i=0;
    cJSON *line = NULL;
    cJSON_ArrayForEach(line, lines){
        cJSON *type = cJSON_GetObjectItem(line, "type");
        cJSON *t = cJSON_GetObjectItem(line, "t");

        titleBlockLines[i++] = AddTitleBlockLine(
            tb, 
            cJSON_GetObjectItem(line, "type")->valueint, 
            cJSON_GetObjectItem(line, "t")->valuedouble
        );
    }

    cJSON *rects = cJSON_GetObjectItem(j, "rects");
    cJSON *rect = NULL;
    cJSON_ArrayForEach(rect, rects){
        cJSON *lines = cJSON_GetObjectItem(rect, "lines");
        TitleBlockRect *r = AddTitleBlockRect(
            tb, // maintain same order as saver
            titleBlockLines[cJSON_GetArrayItem(lines, 2)->valueint], 
            titleBlockLines[cJSON_GetArrayItem(lines, 3)->valueint], 
            titleBlockLines[cJSON_GetArrayItem(lines, 0)->valueint], 
            titleBlockLines[cJSON_GetArrayItem(lines, 1)->valueint]
        );

        r->textSize = cJSON_GetObjectItem(rect, "textSize")->valuedouble;
        r->textPos = cJSON_GetObjectItem(rect, "textPos")->valueint;
        
        if (r->label) free(r->label);
        if (r->content) free(r->content);

        char *label = cJSON_GetObjectItem(rect, "label")->valuestring;
        r->label = calloc(strlen(label)+1, sizeof(char));
        strcpy(r->label, label);

        char *content = cJSON_GetObjectItem(rect, "content")->valuestring;
        r->content = calloc(strlen(content)+1, sizeof(char));
        strcpy(r->content, content);

        r->externalContent = cJSON_GetObjectItem(rect, "externalContent")->valueint;
        r->noLabel = cJSON_GetObjectItem(rect, "noLabel")->valueint;
    }

    titleBlocks[titleBlocksQ] = tb;
    activeTitleBlock = titleBlocks[titleBlocksQ++];
    
    return rs;
}

/* <== Surfaces ================================================> */

static int LoadSkinV0(cJSON *j, Skin *skin, bool comprobationsOnly){
    int rs = 0;
    *skin = (Skin){NULL, NULL, 0, NULL, 0, 0.0, WHITE};    
    cJSON *jSkin = cJSON_GetObjectItem(j, "skin");
    cJSON *type = cJSON_GetObjectItem(j, "type");

    if (jSkin){
        if (cJSON_IsObject(jSkin)){
            
            // ----------- TINT ------------ //
            cJSON *tint = cJSON_GetObjectItem(jSkin, "tint");
            if (tint){
                if (!cJSON_IsObject(tint)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_TINT, LDRV0_ERR_SECTION, 0);
                    return LDRV0_ERR_CODE;
                }

                // RED
                cJSON *R = cJSON_GetObjectItem(tint, "R");
                if (!R){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_MISSING, LDRV0_ERR_R, 0);
                    return LDRV0_ERR_CODE;
                }

                if (!cJSON_IsNumber(R)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_R, 0);
                    return LDRV0_ERR_CODE;
                }

                if (R->valueint > 255 || R->valueint < 0){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_R, 0);
                    return LDRV0_ERR_CODE;
                }

                // GREEN
                cJSON *G = cJSON_GetObjectItem(tint, "G");
                if (!G){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_MISSING, LDRV0_ERR_G, 0);
                    return LDRV0_ERR_CODE;
                }

                if (!cJSON_IsNumber(G)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_G, 0);
                    return LDRV0_ERR_CODE;
                }

                if (G->valueint > 255 || G->valueint < 0){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_G, 0);
                    return LDRV0_ERR_CODE;
                }

                // BLUE
                cJSON *B = cJSON_GetObjectItem(tint, "B");
                if (!B){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_MISSING, LDRV0_ERR_B, 0);
                    return LDRV0_ERR_CODE;
                }

                if (!cJSON_IsNumber(B)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_B, 0);
                    return LDRV0_ERR_CODE;
                }

                if (B->valueint > 255 || B->valueint < 0){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_TINT, LDRV0_ERR_INVALID, LDRV0_ERR_B, 0);
                    return LDRV0_ERR_CODE;
                }

                skin->tint = (Color){
                    R->valueint,
                    G->valueint,
                    B->valueint,
                    255
                };
            }
            
            // ------------ LEN ------------ //
            // this is used just for extensibles, an extensible without
            // len will crash the software :/
            cJSON *len = cJSON_GetObjectItem(jSkin, "len");
            if (type && type->valueint == SURFACE_EXTENSIBLE && !len){
                SetError(LDRV0_ERR_SKIN, LDRV0_ERR_MISSING, LDRV0_ERR_LEN, 0);
                return LDRV0_ERR_CODE;
            }

            if (len){
                if (!cJSON_IsNumber(len)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_LEN, 0);
                    return LDRV0_ERR_CODE;
                }
                else skin->len = len->valuedouble;
            }
            
            // ------------ POINTS ------------ //
            JSON_REQUIRE_RET(points, jSkin, cJSON_IsArray, LDRV0_ERR_SKIN, LDRV0_ERR_POINTS);
            
            // first round is comprobation
            cJSON *point;
            int pointsQ = 0;
            cJSON_ArrayForEach(point, points){
                if (!cJSON_IsObject(point)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_POINTS, 0);
                    return LDRV0_ERR_CODE;
                }
                    
                JSON_REQUIRE_RET(x, point, cJSON_IsNumber, LDRV0_ERR_SKIN, LDRV0_ERR_POINTS, LDRV0_ERR_X);
                JSON_REQUIRE_RET(y, point, cJSON_IsNumber, LDRV0_ERR_SKIN, LDRV0_ERR_POINTS, LDRV0_ERR_X);
                pointsQ++;
            }

            // ------------ LODS ------------- //
            JSON_REQUIRE_RET(LOD, jSkin, cJSON_IsArray, LDRV0_ERR_SKIN, LDRV0_ERR_LOD);
            
            // first round is comprobation (again)
            cJSON *l;
            bool hasZoom0 = false;
            int lodsQ = 0;
            int linesQs[256]; //kinda sucks, but lods are never gonna be more than 256 anyway
            cJSON_ArrayForEach(l, LOD){
                if (!cJSON_IsObject(l)){
                    SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_LOD, 0);
                    return LDRV0_ERR_CODE;
                }
                    
                JSON_REQUIRE_RET(zoom, l, cJSON_IsNumber, LDRV0_ERR_SKIN, LDRV0_ERR_ZOOM);
                if (zoom->valuedouble == 0.0)
                    hasZoom0 = true;
                
                JSON_REQUIRE_RET(lines, l, cJSON_IsArray, LDRV0_ERR_SKIN, LDRV0_ERR_LINE, LDRV0_ERR_SECTION);

                // ------------ LINES ------------- //
                cJSON *line;
                int linesQ = 0;
                cJSON_ArrayForEach(line, lines){
                    if (!cJSON_IsObject(line)){
                        SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_LINE, 0);
                        return LDRV0_ERR_CODE;
                    }

                    JSON_REQUIRE_RET(A, line, cJSON_IsNumber, LDRV0_ERR_SKIN, LDRV0_ERR_LINE, LDRV0_ERR_A);
                    JSON_REQUIRE_RET(B, line, cJSON_IsNumber, LDRV0_ERR_SKIN, LDRV0_ERR_LINE, LDRV0_ERR_B);

                    if (A->valueint >= pointsQ){
                        SetError(LDRV0_ERR_SKIN, LDRV0_ERR_LINE, LDRV0_ERR_INVALID, LDRV0_ERR_A, 0);
                        return LDRV0_ERR_CODE;
                    }
                    
                    if (B->valueint >= pointsQ){
                        SetError(LDRV0_ERR_SKIN, LDRV0_ERR_LINE, LDRV0_ERR_INVALID, LDRV0_ERR_B, 0);
                        return LDRV0_ERR_CODE;
                    }

                    linesQ++;
                }
                
                linesQs[lodsQ] = linesQ;
                lodsQ++;
            }

            if (!hasZoom0){
                SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, LDRV0_ERR_LOD, LDRV0_ERR_SECTION, 0);
                return LDRV0_ERR_CODE;
            }
            
            if (comprobationsOnly)
                return LDRV0_OK;

            // ok comprobations done, now it actually load the skin
            skin->pointsQ = pointsQ;
            skin->lodsQ = lodsQ;
            
            int i=0;
            skin->points = calloc(pointsQ, sizeof(Vector2));
            cJSON_ArrayForEach(point, points){
                skin->points[i].x = cJSON_GetObjectItem(point, "x")->valuedouble;
                skin->points[i].y = cJSON_GetObjectItem(point, "y")->valuedouble;
                i++;
            }

            i = 0;
            skin->lods = calloc(lodsQ, sizeof(SkinLod));
            cJSON_ArrayForEach(l, LOD){
                skin->lods[i].zoom = cJSON_GetObjectItem(l, "zoom")->valuedouble;
                skin->lods[i].lines = calloc(linesQs[i], sizeof(SkinLine));
                skin->lods[i].linesQ = linesQs[i];
                
                int o=0;
                cJSON *line;
                cJSON_ArrayForEach(line, cJSON_GetObjectItem(l, "lines")){
                    skin->lods[i].lines[o].A = cJSON_GetObjectItem(line, "A")->valueint;
                    skin->lods[i].lines[o].B = cJSON_GetObjectItem(line, "B")->valueint;
                    o++;
                }

                i++;
            }
        }

        else {
            SetError(LDRV0_ERR_SKIN, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;
        }
    }

    return LDRV0_OK;
}

static int LoadCutOutV0(cJSON *j, Vector2 **points, int *len){
    int rs = 0;
    
    if (points) *points = NULL;
    if (len) *len = -1;
    cJSON *jCutOut = cJSON_GetObjectItem(j, "cutout");
    if (!jCutOut) return LDRV0_OK;
    
    if (!cJSON_IsArray(jCutOut)){
        SetError(LDRV0_ERR_CUTOUT, LDRV0_ERR_INVALID, LDRV0_ERR_SECTION, 0);
        return LDRV0_ERR_CODE;
    }

    int vCount = 0;
    cJSON *vertex = NULL;
    cJSON_ArrayForEach(vertex, jCutOut){
        if (!cJSON_IsObject(vertex)){
            SetError(LDRV0_ERR_CUTOUT, LDRV0_ERR_POINTS, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(x, vertex, cJSON_IsNumber, LDRV0_ERR_CUTOUT, LDRV0_ERR_POINTS, LDRV0_ERR_X);
        JSON_REQUIRE_RET(y, vertex, cJSON_IsNumber, LDRV0_ERR_CUTOUT, LDRV0_ERR_POINTS, LDRV0_ERR_Y);
        vCount++;
    }

    if (!points || !len)
        return LDRV0_OK;

    *len = vCount;
    Vector2 *p = calloc(*len, sizeof(Vector2));
    *points = p;
    
    int i=0;
    cJSON_ArrayForEach(vertex, jCutOut){
        p[i].x = cJSON_GetObjectItem(vertex, "x")->valuedouble;
        p[i].y = cJSON_GetObjectItem(vertex, "y")->valuedouble;
        i++;
    }

    return LDRV0_OK;
}

static int VerificateSurfaceExtensibleV0(cJSON *j){
    int rs = LDRV0_OK;
    JSON_REQUIRE_RET(id, j, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_ID);
    JSON_REQUIRE_RET(width, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_WIDTH);
    Skin tmp;
    rs = LoadSkinV0(j, &tmp, true); // skin is in the stack, so it doesn't load anything really
    return rs;
}

static int VerificateSurfaceComponentV0(cJSON *j){
    int rs = LDRV0_OK;
    JSON_REQUIRE_RET(id, j, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_ID);
    JSON_REQUIRE_RET(width, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_WIDTH);
    JSON_REQUIRE_RET(height, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_HEIGHT);
    Skin tmp;
    rs = LoadSkinV0(j, &tmp, true); // skin is in the stack, so it doesn't load anything really
    if (rs != LDRV0_OK)
        return rs;

    rs = LoadCutOutV0(j, NULL, NULL);
    return rs;
}

static int VerificateSurfaceV0(cJSON *j){
    int rs = 0;
    JSON_REQUIRE_RET(type, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_TYPE);
    switch (type->valueint){
        case SURFACE_COMPONENT:  return VerificateSurfaceComponentV0(j);
        case SURFACE_EXTENSIBLE: return VerificateSurfaceExtensibleV0(j);
        default: 
            SetError(LDRV0_ERR_SURFACE, LDRV0_ERR_UNKNOWN, LDRV0_ERR_TYPE, 0);
            return LDRV0_ERR_CODE;
    }
}

static int LoadSurfaceComponentV0(cJSON *j){
    int rs = LDRV0_OK;

    JSON_REQUIRE_RET(id, j, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_ID);
    JSON_REQUIRE_RET(width, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_WIDTH);
    JSON_REQUIRE_RET(height, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_HEIGHT);

    // retrieve skin
    Skin skin;
    rs = LoadSkinV0(j, &skin, false);

    // retrieve CutOut
    Vector2 *cutout = NULL;
    int cutoutLen = 0;
    LoadCutOutV0(j, &cutout, &cutoutLen);

    char *name = ResolveSurfaceName(id->valuestring, activeTree);
    Surface *s = AddSurface( activeTree->surfaceSelection[0], id->valuestring, name, SURFACE_COMPONENT );
    free(name);

    // no error code here, what a boring thing to do...
    if (s->parent->type == SURFACE_EXTENSIBLE) s->opts |= SURFACE_OPT_FIXED_Y;
    if (s->parent->opts & SURFACE_OPT_FIXED_Y) s->opts |= SURFACE_OPT_FIXED_Y;
    if (s->parent->opts & SURFACE_OPT_FIXED_X) s->opts |= SURFACE_OPT_FIXED_X;
    s->origin = (Vector2){0,0};

    cJSON *origin = cJSON_GetObjectItem(j, "origin");
    if (origin && cJSON_IsObject(origin)){
        cJSON *x = cJSON_GetObjectItem(origin, "x");
        cJSON *y = cJSON_GetObjectItem(origin, "y");
        if (x && y && cJSON_IsNumber(x) && cJSON_IsNumber(y)){
            s->origin.x = x->valuedouble;
            s->origin.y = y->valuedouble;
        }
    }

    s->size = (Vector2){width->valuedouble, height->valuedouble};
    s->ogSize = s->size;
    if (skin.pointsQ > 0){
        skin.surface = s;
        s->skin = skin;
    }

    if (cutout){
        s->cutout.len = cutoutLen;
        s->cutout.points = cutout;
    }

    // if parent is not a base surface
    if (!s->parent->parent || s->parent->type == SURFACE_EXTENSIBLE)
        s->pos = Vector2Zero();
    else s->pos = (Vector2){s->parent->size.x/2 + s->size.x/2,0};

    s->skin.label.offset.y = s->size.y/2 *-1 -10;
        
    return rs;
}

static int LoadSurfaceExtensibleV0(cJSON *j, void *param){
    int rs = CanAddExtensible(activeTree, param);
    
    switch (rs){
        case -1: 
            SetError(LDRV0_ERR_ANCHOR, LDRV0_ERR_MISSING, 0);
            return LDRV0_ERR_CODE;
        
        case -2: 
        case -3: 
            SetError(LDRV0_ERR_ANCHOR, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;

    }

    JSON_REQUIRE_RET(id, j, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_ID);
    JSON_REQUIRE_RET(width, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_WIDTH);

    // Add multiple extensibles
    // buch of magical nums, kinda sucks...
    if (activeTree->anchorSelection[3]){
        
        // Sort the anchor pairs (manually...)
        Anchor *sortedAnchors[4];
        for (int i = 0; i < 4; i++) sortedAnchors[i] = activeTree->anchorSelection[i];
        if (sortedAnchors[0]->type != sortedAnchors[1]->type){
            Anchor *tmp = sortedAnchors[2];
            sortedAnchors[2] = sortedAnchors[1];
            sortedAnchors[1] = tmp;
            if (sortedAnchors[0]->type != sortedAnchors[1]->type){
                tmp = sortedAnchors[3];
                sortedAnchors[3] = sortedAnchors[1];
                sortedAnchors[1] = tmp;
            }
        }

        Anchor *tmp[4];
        for (int i = 0; i < 4; i++) tmp[i] = sortedAnchors[i];
        sortedAnchors[0] = GetAnchorAtLeft(tmp[0], tmp[1]);
        sortedAnchors[1] = sortedAnchors[0] == tmp[0] ? tmp[1] : tmp[0];
        sortedAnchors[2] = GetAnchorAtLeft(tmp[2], tmp[3]);
        sortedAnchors[3] = sortedAnchors[2] == tmp[2] ? tmp[3] : tmp[2];
        
        if (sortedAnchors[0]->type == ANCHOR_VERTICAL){
            for (int i = 0; i < 4; i++) tmp[i] = sortedAnchors[i];
            sortedAnchors[0] = tmp[2];
            sortedAnchors[1] = tmp[3];
            sortedAnchors[2] = tmp[0];
            sortedAnchors[3] = tmp[1];
        }
        
        // Now add the extensibles
        activeTree->anchorSelection[3] = NULL;
    
        activeTree->anchorSelection[0] = sortedAnchors[0];
        activeTree->anchorSelection[1] = sortedAnchors[2];
        activeTree->anchorSelection[2] = sortedAnchors[3];
        LoadSurfaceExtensibleV0(j, param);
        activeTree->lastInsertedSurface->anchorAlign = ANCHOR_ALIGN_MAX;
        activeTree->lastInsertedSurface->leftCut = width->valuedouble;
        activeTree->lastInsertedSurface->rightCut = width->valuedouble;
        CreateSurfaceBoundarieAnchors(activeTree->lastInsertedSurface);

        activeTree->anchorSelection[0] = sortedAnchors[3];
        activeTree->anchorSelection[1] = sortedAnchors[0];
        activeTree->anchorSelection[2] = sortedAnchors[1];
        LoadSurfaceExtensibleV0(j, param);
        activeTree->lastInsertedSurface->anchorAlign = ANCHOR_ALIGN_MIN;
        activeTree->lastInsertedSurface->leftCut = width->valuedouble;
        activeTree->lastInsertedSurface->rightCut = width->valuedouble;;
        CreateSurfaceBoundarieAnchors(activeTree->lastInsertedSurface);

        activeTree->anchorSelection[0] = sortedAnchors[1];
        activeTree->anchorSelection[1] = sortedAnchors[2];
        activeTree->anchorSelection[2] = sortedAnchors[3];
        LoadSurfaceExtensibleV0(j, param);
        activeTree->lastInsertedSurface->anchorAlign = ANCHOR_ALIGN_MIN;
        activeTree->lastInsertedSurface->leftCut = width->valuedouble*-1;
        activeTree->lastInsertedSurface->rightCut = width->valuedouble*-1;
        CreateSurfaceBoundarieAnchors(activeTree->lastInsertedSurface);

        activeTree->anchorSelection[0] = sortedAnchors[2];
        activeTree->anchorSelection[1] = sortedAnchors[0];
        activeTree->anchorSelection[2] = sortedAnchors[1];
        LoadSurfaceExtensibleV0(j, param);
        activeTree->lastInsertedSurface->anchorAlign = ANCHOR_ALIGN_MAX;
        activeTree->lastInsertedSurface->leftCut = width->valuedouble*-1;
        activeTree->lastInsertedSurface->rightCut = width->valuedouble*-1;
        CreateSurfaceBoundarieAnchors(activeTree->lastInsertedSurface);


        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++)
            activeTree->anchorSelection[i] = NULL;
    }

    // Add a single extensible
    else {

        Sort3Anchors(activeTree->anchorSelection);
        Skin skin;
        rs = LoadSkinV0(j, &skin, false);

        char *name = ResolveSurfaceName(id->valuestring, activeTree);
        Surface *s = AddSurface(activeTree->surfaceSelection[0], id->valuestring, name, SURFACE_EXTENSIBLE);
        free(name);

        s->size = (Vector2){0, width->valuedouble};
        s->anchors[0] = activeTree->anchorSelection[0];
        s->anchors[1] = activeTree->anchorSelection[1];
        s->anchors[2] = activeTree->anchorSelection[2];

        if (activeTree->anchorSelection[0]->type == ANCHOR_VERTICAL)
            s->rotation += 90;

        skin.surface = s;
        s->skin = skin;
    }
    
    return rs;
}

static int LoadSurfaceV0(cJSON *j, void *param){
    int rs = LDRV0_OK;
    JSON_REQUIRE_RET(type, j, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_TYPE);
    
    switch (type->valueint){
        case SURFACE_COMPONENT:  return LoadSurfaceComponentV0(j);
        case SURFACE_EXTENSIBLE: return LoadSurfaceExtensibleV0(j, param);
        default: 
            SetError(LDRV0_ERR_SURFACE, LDRV0_ERR_UNKNOWN, LDRV0_ERR_TYPE, 0);
            return LDRV0_ERR_CODE;
    }
}

/* <== Templates ===============================================> */

static bool TemplatesMatch(cJSON *j){
    cJSON *surfaces = cJSON_GetObjectItem(j, "surfaces");
    
    for (int i=0; i<treeListQ; i++){    
        SurfaceTree *tree = treeList[i];

        // every base surface on the original template
        // must exists on the new one
        cJSON *surface = NULL;
        cJSON_ArrayForEach(surface, surfaces){
            cJSON *name = cJSON_GetObjectItem(surface, "name");
            if (strcmp(tree->baseSurface.id, name->valuestring) == 0)
                goto out0;
        }

        return false;
        out0:

        // every fixed anchor on the original template
        // must exists on the new one
        for (Anchor *a = tree->anchors; a != NULL; a=a->next){
            if (!(a->opts & ANCHOR_OPT_FIXED_BY_ENGINE)) continue;
            if (IsAnchorBase(a)) continue;
            
            cJSON *anchors = cJSON_GetObjectItem(surface, "anchors");
            if (!anchors) return false;
            
            cJSON *anchor = NULL;
            bool ok = false;
            cJSON_ArrayForEach(anchor, anchors){
                cJSON *anchorName = cJSON_GetObjectItem(anchor, "name");
                if (strcmp(a->name, anchorName->valuestring) == 0){
                    ok = true;
                    break;
                }
                    
            }
            
            if (!ok) return false;
        }
    }

    return true;
}

static int VerificateTemplate(cJSON *j){
    int rs = LDRV0_OK;
    
    JSON_REQUIRE_RET(surfaces, j, cJSON_IsArray, LDRV0_ERR_SURFACE, LDRV0_ERR_SECTION);

    cJSON *surface = NULL;
    cJSON_ArrayForEach(surface, surfaces){
        if (!cJSON_IsObject(surface)) {
            SetError(LDRV0_ERR_SURFACE, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;
        }
        
        JSON_REQUIRE_RET(name, surface, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_NAME);
        JSON_REQUIRE_RET(width, surface, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_WIDTH);
        JSON_REQUIRE_RET(height, surface, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_HEIGHT);

        cJSON *anchors = cJSON_GetObjectItem(surface, "anchors");
        if (anchors){
            if (!cJSON_IsArray(anchors)){
                SetError(LDRV0_ERR_SURFACE, LDRV0_ERR_INVALID, LDRV0_ERR_ANCHOR, LDRV0_ERR_SECTION, 0);
                return LDRV0_ERR_CODE;
            }

            cJSON *anchor;
            cJSON_ArrayForEach(anchor, anchors){
                if (!cJSON_IsObject(anchor)){
                    SetError(LDRV0_ERR_SURFACE, LDRV0_ERR_INVALID, LDRV0_ERR_ANCHOR, 0);
                    return LDRV0_ERR_CODE;
                }
         
                JSON_REQUIRE_RET(name, anchor, cJSON_IsString, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_NAME);
                JSON_REQUIRE_RET(type, anchor, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_TYPE);
                JSON_REQUIRE_RET(val, anchor, cJSON_IsNumber, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_VAL);
         
            }
        }
    }

    return LDRV0_OK;
}

static int LoadTemplateV0(cJSON *j){
    int rs = VerificateTemplate(j);
    if (rs != LDRV0_OK) 
        return rs;

    SurfaceTree *newTreeList[TREELISTS_Q];
    int newTreeListQ = 0;

    cJSON *surfaces = cJSON_GetObjectItem(j, "surfaces");
    cJSON *surface = NULL;
    int errorLoadingSkin = -1;
    cJSON_ArrayForEach(surface, surfaces){
        cJSON *name = cJSON_GetObjectItem(surface, "name");
        cJSON *width = cJSON_GetObjectItem(surface, "width");
        cJSON *height = cJSON_GetObjectItem(surface, "height");
        cJSON *anchors = cJSON_GetObjectItem(surface, "anchors");
        
        newTreeList[newTreeListQ] = calloc(1, sizeof(SurfaceTree));
        newTreeList[newTreeListQ]->baseSurface.tree = newTreeList[newTreeListQ];
        newTreeList[newTreeListQ]->surfacesQ = 1;
        newTreeList[newTreeListQ]->baseSurface.size = (Vector2){width->valuedouble, height->valuedouble};
        newTreeList[newTreeListQ]->baseSurface.ogSize = newTreeList[newTreeListQ]->baseSurface.size;
        newTreeList[newTreeListQ]->baseSurface.id = calloc(strlen(name->valuestring) + 1, sizeof(char));
        newTreeList[newTreeListQ]->baseSurface.name = calloc(strlen(name->valuestring) + 1, sizeof(char));
        newTreeList[newTreeListQ]->baseSurface.opts = 0;
        strcpy(newTreeList[newTreeListQ]->baseSurface.id, name->valuestring);
        strcpy(newTreeList[newTreeListQ]->baseSurface.name, name->valuestring);
        
        if ( errorLoadingSkin == -1){
            Skin skin;
            rs = LoadSkinV0(surface, &skin, false);
            if (rs == LDRV0_OK){
                skin.surface = &newTreeList[newTreeListQ]->baseSurface;
                newTreeList[newTreeListQ]->baseSurface.skin = skin;
            }

            else errorLoadingSkin = newTreeListQ;    
        }
        
        UpdateSurfaceTree(newTreeList[newTreeListQ]);
        
        // create base anchors
        Anchor *topMost = AddAnchor(newTreeList[newTreeListQ], "TopMost", ANCHOR_HORIZONTAL, 0);
        topMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

        Anchor *bottomMost = AddAnchor(newTreeList[newTreeListQ], "BottomMost", ANCHOR_HORIZONTAL, newTreeList[newTreeListQ]->baseSurface.size.y);
        bottomMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

        Anchor *leftMost = AddAnchor(newTreeList[newTreeListQ], "LeftMost", ANCHOR_VERTICAL, 0);
        leftMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

        Anchor *rightMost = AddAnchor(newTreeList[newTreeListQ], "RightMost", ANCHOR_VERTICAL, newTreeList[newTreeListQ]->baseSurface.size.x);
        rightMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

        if (anchors){
            cJSON *anchor;
            cJSON_ArrayForEach(anchor, anchors){
                Anchor *newAnchor = AddAnchor(
                    newTreeList[newTreeListQ], 
                    cJSON_GetObjectItem(anchor, "name")->valuestring,
                    cJSON_GetObjectItem(anchor, "type")->valueint,
                    cJSON_GetObjectItem(anchor, "val")->valuedouble
                );

                newAnchor->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;
            }
        }

        newTreeListQ++;
    }

    // Exit
    if (treeListQ > 0){
        for (int i=0; i<treeListQ; i++){
            
            // Clear every anchor
            Anchor *a = treeList[i]->anchors;
            while (a != NULL){
                Anchor *next = a->next;
                DeleteAnchorForced(a);
                a = next;
            }
            
            // ClearSurfaceTree clean's childs
            // but baseSurface is not dinamically allocated
            ClearSurfaceTree(treeList[i]);

            ClearSurfaceSkin(&treeList[i]->baseSurface.skin);
            free(treeList[i]->baseSurface.id);
            if (treeList[i]->baseSurface.name)
                free(treeList[i]->baseSurface.name);
            free(treeList[i]->linearSurfaceTree);
            free(treeList[i]);
        }
    }

    for (int i=0; i<TREELISTS_Q; i++)
        treeList[i] = NULL;

    treeListQ = newTreeListQ;
    for (int i=0; i<treeListQ; i++)
        treeList[i] = newTreeList[i];
    
    activeTree = treeList[0];
    
    if (errorLoadingSkin > -1) {
        ErrorAppend('_', 0);
        char str[16];
        sprintf(str, "%d", errorLoadingSkin);
        for (int i = 0; str[i] != '\0'; i++)
            ErrorAppend(str[i], 0);

        return LDRV0_ERR_CODE;
    }
        
    return LDRV0_OK;
}

static int ReloadTemplateV0(cJSON *j){
    int rs = VerificateTemplate(j);
    if (rs != LDRV0_OK) 
        return rs;

    cJSON *surfaces = cJSON_GetObjectItem(j, "surfaces");
    cJSON *surface = NULL;

    for (int i = 0; i < treeListQ; i++){
        SurfaceTree *tree = treeList[i];
        int rs = ClearSurfaceSkin(&tree->baseSurface.skin);
    }

    // Deletes non matching trees
    for (int i=0; i<TREELISTS_Q; i++){
        SurfaceTree *t = treeList[i];
        if (t == NULL || t->baseSurface.id == NULL)
            continue;

        bool doesMatch = false;
        cJSON_ArrayForEach(surface, surfaces){
            cJSON *name = cJSON_GetObjectItem(surface, "name");
            if (name == NULL || name->valuestring == NULL)
                continue;
            
            if (strcmp(name->valuestring, t->baseSurface.id) == 0)
                doesMatch = true;
        }

        if (doesMatch) continue;

        // First free every projection constraint
        for (int ii=0; ii<TREELISTS_Q; ii++){
            SurfaceTree *tt = treeList[ii];
            if (tt == NULL) continue;
            if (t == tt) continue;

            for (Anchor *a = tt->anchors; a != NULL; a = a->next){
                if (a->refA && a->refA->tree == t) AnchorFreeConstraint(a);
                if (a->refB && a->refB->tree == t) AnchorFreeConstraint(a);
            }
        }

        // Ok, delete time :D
        if (ghostTree == t) ghostTree = NULL;
        if (activeTree == t) activeTree = NULL;

        // Clear every anchor
        Anchor *a = t->anchors;
        while (a != NULL){
            Anchor *next = a->next;
            DeleteAnchorForced(a);
            a = next;
        }
        
        // ClearSurfaceTree clean's childs
        // but baseSurface is not dinamically allocated
        ClearSurfaceTree(treeList[i]);

        ClearSurfaceSkin(&t->baseSurface.skin);
        free(t->baseSurface.id);
        if (t->baseSurface.name)
            free(t->baseSurface.name);
        free(t->linearSurfaceTree);
        free(treeList[i]);
        treeList[i] = NULL;
    }

    // Compact tree list by filling NULL slots
    struct {
        int len;
        int cursor;
        int indexes[TREELISTS_Q];
    } nullIndexes = {0};

    treeListQ = 0;
    for (int i=0; i<TREELISTS_Q; i++){
        if (treeList[i] != NULL){
            if (nullIndexes.len > 0){
                treeList[nullIndexes.indexes[nullIndexes.cursor++]] = treeList[i];
                treeList[i] = NULL;
                nullIndexes.len--;
            }
            treeListQ++;
            continue;
        }

        nullIndexes.indexes[nullIndexes.len++] = i;
    }

    int i=0;
    int errorLoadingSkin = -1;
    cJSON_ArrayForEach(surface, surfaces){
        cJSON *name = cJSON_GetObjectItem(surface, "name");
        cJSON *width = cJSON_GetObjectItem(surface, "width");
        cJSON *height = cJSON_GetObjectItem(surface, "height");
        cJSON *anchors = cJSON_GetObjectItem(surface, "anchors");
    
        Skin skin;
        if ( errorLoadingSkin == -1){
            rs = LoadSkinV0(surface, &skin, false);
            if (rs != LDRV0_OK){
                errorLoadingSkin = i;
            }
        }

        SurfaceTree *matchingTree = NULL;
        for (int i = 0; i < treeListQ; i++){
            SurfaceTree *tree = treeList[i];
            if (strcmp(tree->baseSurface.id, name->valuestring) == 0){
                matchingTree = tree;
                break;
            }
        }

        if (matchingTree){
            matchingTree->baseSurface.size.x = width->valuedouble;
            matchingTree->baseSurface.size.y = height->valuedouble;
            matchingTree->baseSurface.ogSize = matchingTree->baseSurface.size;
            if (skin.points){
                skin.surface = &matchingTree->baseSurface;
                matchingTree->baseSurface.skin = skin;
            }
            
            if (anchors){
                cJSON *a;
                cJSON_ArrayForEach(a, anchors){
                    Anchor *anchor = GetAnchorByName(matchingTree, cJSON_GetObjectItem(a, "name")->valuestring);
                    anchor->type = cJSON_GetObjectItem(a, "type")->valueint;
                    anchor->val = cJSON_GetObjectItem(a, "val")->valuedouble;
                }
            }
        }


        else {
            SurfaceTree *newTree = calloc(1, sizeof(SurfaceTree));
            newTree->baseSurface.tree = newTree;
            newTree->surfacesQ = 1;

            newTree->baseSurface.size = (Vector2){ width->valuedouble, height->valuedouble};
            newTree->baseSurface.ogSize = newTree->baseSurface.size;
            newTree->baseSurface.id = calloc(strlen(name->valuestring) + 1, sizeof(char));
            newTree->baseSurface.opts = 0;
            strcpy(newTree->baseSurface.id, name->valuestring);
            newTree->baseSurface.name = strdup(newTree->baseSurface.id);

            UpdateSurfaceTree(newTree);

            // create base anchors
            Anchor *topMost = AddAnchor(newTree, "TopMost", ANCHOR_HORIZONTAL, 0);
            topMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

            Anchor *bottomMost = AddAnchor(newTree, "BottomMost", ANCHOR_HORIZONTAL, newTree->baseSurface.size.y);
            bottomMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

            Anchor *leftMost = AddAnchor(newTree, "LeftMost", ANCHOR_VERTICAL, 0);
            leftMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

            Anchor *rightMost = AddAnchor(newTree, "RightMost", ANCHOR_VERTICAL, newTree->baseSurface.size.x);
            rightMost->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;

            if (anchors){
                cJSON *anchor;
                cJSON_ArrayForEach(anchor, anchors){
                    Anchor *newAnchor = AddAnchor(
                        newTree, 
                        cJSON_GetObjectItem(anchor, "name")->valuestring,
                        cJSON_GetObjectItem(anchor, "type")->valueint,
                        cJSON_GetObjectItem(anchor, "val")->valuedouble
                    );

                    newAnchor->opts |= ANCHOR_OPT_FIXED_BY_ENGINE;
                }
            }

            treeList[treeListQ] = newTree;

            if (skin.points){
                skin.surface = &treeList[treeListQ]->baseSurface;
                treeList[treeListQ]->baseSurface.skin = skin;
            }

            treeListQ++;
        }

        i++;
    }
    
    if (errorLoadingSkin > -1) {
        ErrorAppend('_', 0);
        char str[16];
        sprintf(str, "%d", errorLoadingSkin);
        for (int i = 0; str[i] != '\0'; i++)
            ErrorAppend(str[i], 0);

        return LDRV0_ERR_CODE;
    }
    return LDRV0_OK;
}

/* <== Assemblies ==============================================> */

static int VerificateAnchorV0(cJSON *j){
    int rs = LDRV0_OK;
    
    JSON_REQUIRE_RET(name, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_NAME);
    JSON_REQUIRE_RET(type, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_TYPE);
    switch (type->valueint){
        case ANCHOR_VERTICAL:
        case ANCHOR_HORIZONTAL:
            break;

        default: 
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_INVALID, LDRV0_ERR_TYPE, 0);
            return LDRV0_ERR_CODE;
    }

    JSON_REQUIRE_RET(opts, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_OPTS);
    JSON_REQUIRE_RET(val, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_VAL);

    return rs;
}

static int VerificateConstraintV0(cJSON *j){
    int rs = LDRV0_OK;
    JSON_REQUIRE_RET(anchor, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_ANCHOR);
    JSON_REQUIRE_RET(type, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_TYPE);
    
    if (type->valueint == ANCHOR_CONSTRAINT_PROJECTION || type->valueint == ANCHOR_CONSTRAINT_INVERSED_PROJECTION){
        JSON_REQUIRE_RET(A, j, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_A);
        JSON_REQUIRE_RET(surface_tree, A, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_A, LDRV0_ERR_SURFACE);
        JSON_REQUIRE_RET(name, A, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_A, LDRV0_ERR_NAME);

        cJSON *tree; // trees sections's existence has already been verified by this point
        cJSON_ArrayForEach(tree, cJSON_GetObjectItem(loadedAssemblyFile->root, "surface_trees")){
            if (cJSON_IsObject(tree)){
                cJSON *name0 = cJSON_GetObjectItem(tree, "name");
                if (name0 && cJSON_IsString(name0)){
                    if (strcmp(name0->valuestring, surface_tree->valuestring) == 0){
                        // tree exists
                        cJSON *anchors = cJSON_GetObjectItem(tree, "anchors");
                        if (anchors && cJSON_IsArray(anchors)){
                            cJSON *anchor;
                            cJSON_ArrayForEach(anchor, anchors){
                                if (cJSON_IsObject(anchor)){
                                    cJSON *anchorName = cJSON_GetObjectItem(anchor, "name");
                                    if (anchorName && cJSON_IsString(anchorName)){
                                        if (strcmp(anchorName->valuestring, name->valuestring) == 0){
                                            goto out_spaguetti;
                                        }
                                    }
                                }
                            }
                        }
                        
                        
                    }
                }
            }
        }

        SetError(LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_A, LDRV0_ERR_INVALID, 0);
        return LDRV0_ERR_CODE;
        out_spaguetti:{}

    }
    else {
        JSON_REQUIRE_RET(A, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_A);
    }
    
    JSON_REQUIRE_RET(B, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_B);
    
    //ref surface
    cJSON *refSurface = cJSON_GetObjectItem(j, "refSurface");
    if (refSurface){
        if (!cJSON_IsObject(refSurface)){
            JSON_REQUIRE_RET(surface, refSurface, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_SURFACE);
            JSON_REQUIRE_RET(type, refSurface, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_SURFACE);

            bool ok = false;
            if (strcmp(surface->valuestring, "bTop") == 0) ok = true;
            else if (strcmp(surface->valuestring, "bBottom") == 0) ok = true;
            else if (strcmp(surface->valuestring, "bLeft") == 0) ok = true;
            else if (strcmp(surface->valuestring, "bRight") == 0) ok = true;

            else if (strcmp(surface->valuestring, "coTop") == 0) ok = true;
            else if (strcmp(surface->valuestring, "coBottom") == 0) ok = true;
            else if (strcmp(surface->valuestring, "coLeft") == 0) ok = true;
            else if (strcmp(surface->valuestring, "coRight") == 0) ok = true;
            
            if (!ok){
                SetError(LDRV0_ERR_ASM, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_SURFACE, LDRV0_ERR_TYPE);
                return LDRV0_ERR_CODE;
            }
        }
    }
    return rs;
}

static int VerificateAnnotationV0(cJSON *j){
    int rs = LDRV0_OK;
    JSON_REQUIRE_RET(A, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_A);
    JSON_REQUIRE_RET(B, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_B);
    JSON_REQUIRE_RET(offset, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_VAL);
    JSON_REQUIRE_RET(extension, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_EXTENSION);
    JSON_REQUIRE_RET(opts, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_OPTS);
    return rs;
}

static int VerificateAsmSurfaceV1(cJSON *j){
    int rs = LDRV0_OK;

    JSON_REQUIRE_RET(id, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ID);
    JSON_REQUIRE_RET(type, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TYPE);
    switch (type->valueint){
        case SURFACE_COMPONENT:
        case SURFACE_EXTENSIBLE: 
        break;
        
        default: 
        SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TYPE, LDRV0_ERR_INVALID, 0);
        return LDRV0_ERR_CODE;
    }

    // Position
    JSON_REQUIRE_RET(pos, j, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_POS);
    JSON_REQUIRE_RET(x, pos, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_POS, LDRV0_ERR_X);
    JSON_REQUIRE_RET(y, pos, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_POS, LDRV0_ERR_Y);

    // Size
    cJSON *size = cJSON_GetObjectItem(j, "size");
    if (size){
        if (!cJSON_IsObject(size)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_SIZE, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(w, size, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_SIZE, LDRV0_ERR_WIDTH);
        JSON_REQUIRE_RET(h, size, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_SIZE, LDRV0_ERR_HEIGHT);
    }

    // Label
    cJSON *label = cJSON_GetObjectItem(j, "label");
    if (label){
        if (!cJSON_IsObject(label)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_LABEL, LDRV0_ERR_INVALID, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(text, label, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_LABEL, LDRV0_ERR_TEXT);
        JSON_REQUIRE_RET(offset_x, label, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_LABEL, LDRV0_ERR_X);
        JSON_REQUIRE_RET(offset_y, label, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_LABEL, LDRV0_ERR_Y);
        JSON_REQUIRE_RET(rotation, label, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_LABEL, LDRV0_ERR_ROT);
    }

    // Color
    cJSON *color = cJSON_GetObjectItem(j, "color");
    if (color){
        if (!cJSON_IsObject(color)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TINT, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(r, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TINT, LDRV0_ERR_R);
        JSON_REQUIRE_RET(g, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TINT, LDRV0_ERR_G);
        JSON_REQUIRE_RET(B, color, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TINT, LDRV0_ERR_B);
    }

    // Origin
    cJSON *origin = cJSON_GetObjectItem(j, "origin");
    if (origin){
        if (!cJSON_IsObject(origin)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ORG, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(x, origin, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ORG, LDRV0_ERR_X);
        JSON_REQUIRE_RET(y, origin, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ORG, LDRV0_ERR_Y);
    }

    cJSON *cutoutsRect = cJSON_GetObjectItem(j, "cutouts_rects");
    if (cutoutsRect){
        if (!cJSON_IsObject(cutoutsRect)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_CUTOUT, LDRV0_ERR_RECT, 0);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(w, cutoutsRect, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_CUTOUT, LDRV0_ERR_RECT, LDRV0_ERR_WIDTH);
        JSON_REQUIRE_RET(h, cutoutsRect, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_CUTOUT, LDRV0_ERR_RECT, LDRV0_ERR_HEIGHT);
    }

    // Rotation
    JSON_REQUIRE_RET(rotation, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ROT);
    if (!(rotation->valuedouble == 0.0 || rotation->valuedouble == 90.0 || rotation->valuedouble == 180.0 || rotation->valuedouble == 270.0)) {
        SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_INVALID, LDRV0_ERR_ROT, 0);
        return LDRV0_ERR_CODE;
    }
    
    JSON_REQUIRE_RET(opts, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_OPTS);

    // Anchors
    JSON_REQUIRE_RET(anchors, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_SECTION);
    cJSON *anchor;
    cJSON_ArrayForEach(anchor, anchors){
        if (!cJSON_IsString(anchor) && !cJSON_IsNull(anchor)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_INVALID, LDRV0_ERR_ANCHOR, 0);
            return LDRV0_ERR_CODE;
        }
    }

    JSON_REQUIRE_RET(anchor_align, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_ALIGN);
    JSON_REQUIRE_RET(left_cut, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_LCUT);
    JSON_REQUIRE_RET(right_cut, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_RCUT);
    JSON_REQUIRE_RET(parent, j, cJSON_IsNumber, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_ANCHOR, LDRV0_ERR_PARENT);

    return 0;
}

static int VerificateTreeV1(cJSON *j){
    int rs = LDRV0_OK;

    JSON_REQUIRE_RET(name, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_NAME);
    
    // Anchors
    JSON_REQUIRE_RET(anchors, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_SECTION);
    cJSON *anchor;
    cJSON_ArrayForEach(anchor, anchors){
        int rs = VerificateAnchorV0(anchor);
        if (rs != LDRV0_OK) return rs;
    }

    // Constraints
    JSON_REQUIRE_RET(anchors_constraints, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_ANCHOR, LDRV0_ERR_CONSTRAINT, LDRV0_ERR_SECTION);
    cJSON *constraint;
    cJSON_ArrayForEach(constraint, anchors_constraints){
        int rs = VerificateConstraintV0(constraint);
        if (rs != LDRV0_OK) return rs;
    }

    // Anotations
    JSON_REQUIRE_RET(annotations, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_ANNOTATION, LDRV0_ERR_SECTION);
    cJSON *annotation;
    cJSON_ArrayForEach(annotation, annotations){
        int rs = VerificateAnnotationV0(annotation);
        if (rs != LDRV0_OK) return rs;
    }

    // Surfaces
    JSON_REQUIRE_RET(surfaces, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_SECTION);
    cJSON *surface;
    cJSON_ArrayForEach(surface, surfaces){
        int rs = VerificateAsmSurfaceV1(surface);
        if (rs != LDRV0_OK) return rs;
    }
    
    return LDRV0_OK;
}

static int LoadAssemblyV1(cJSON *j){
    int rs = LDRV0_OK;
    invalidSurfaceFilename[0] = '\0';
    
    // verificate + retrieve template
    JSON_REQUIRE_RET(template, j, cJSON_IsString, LDRV0_ERR_ASM, LDRV0_ERR_TEMPLATE, LDRV0_ERR_NAME);
    
    // verificate + retrieve surface trees
    JSON_REQUIRE_RET(surface_trees, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_SURFACE, LDRV0_ERR_TREE );
    cJSON *tree;
    cJSON_ArrayForEach(tree, surface_trees){
        rs = VerificateTreeV1(tree);
        if (rs != LDRV0_OK) 
            return rs;
    }

    // verify file existence and correct format
    if (!IsFileExtension(template->valuestring, ".mwt")){
        SetError(LDRV0_ERR_ASM, LDRV0_ERR_INVALID, LDRV0_ERR_TEMPLATE, LDRV0_ERR_TYPE, 0);
        return LDRV0_ERR_CODE;
    }
        
    if (!FileExists(template->valuestring)){
        SetError(LDRV0_ERR_ASM, LDRV0_ERR_MISSING, LDRV0_ERR_TEMPLATE, LDRV0_ERR_FILE, 0);
        return LDRV0_ERR_CODE;
    }
    
    // verificate each surface
    cJSON_ArrayForEach(tree, surface_trees){
        cJSON *surface;
        char surfacePath[OG_PATH_MAX] = {0};
        cJSON_ArrayForEach(surface, cJSON_GetObjectItem(tree, "surfaces")){
            uint32_t opts = cJSON_GetObjectItem(surface, "opts")->valueint;
            if (opts & SURFACE_OPT_GENERIC) 
                continue;

            char *id = cJSON_GetObjectItem(surface, "id")->valuestring;
            
            if (strlen(id) >= OG_PATH_MAX){
                SetError(LDRV0_ERR_ASM, LDRV0_ERR_INVALID, LDRV0_ERR_SURFACE, LDRV0_ERR_NAME, 0);
                return LDRV0_ERR_CODE;
            }
                
            sprintf(surfacePath, "%s.mwc", id);
            if (!FileExists(surfacePath)){
                strcpy(invalidSurfaceFilename, surfacePath);
                SetError(LDRV0_ERR_ASM, LDRV0_ERR_MISSING, LDRV0_ERR_SURFACE, LDRV0_ERR_FILE, 0);
                return LDRV0_ERR_CODE;
            }

            // No validation on tmp because file extension and existence were verified earlier
            FileGeneric tmp = LoadFileGeneric(surfacePath, ".mwc");
            rs = VerificateSurfaceV0(tmp.root);
            if (rs != LDRV0_OK) 
                return rs;

            FreeFileGeneric(tmp);
            if (rs != LDRV0_OK){
                strcpy(invalidSurfaceFilename, surfacePath);
                ErrorPrepend(LDRV0_ERR_ASM);
                return LDRV0_ERR_CODE;
            }

        }
    }

    // cause LoadPanelTemplate deletes the titleblocks list
    TitleBlock *titleBlocksBackup[TITLEBLOCKS_Q];
    int titleBlocksQBackup = titleBlocksQBackup;
    titleBlocksQ = 0;
    for (int i=0; i<TITLEBLOCKS_Q; i++){
        titleBlocksBackup[i] = titleBlocks[i];
        titleBlocks[i] = NULL;
    }
    
    // verificate and load titleblocks
    JSON_REQUIRE_RET(title_blocks, j, cJSON_IsArray, LDRV0_ERR_ASM, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_SECTION);
    bool titleBlockError = false;
    cJSON *block = NULL;
    cJSON_ArrayForEach(block, title_blocks){
        if (!cJSON_IsString(block)){
            SetError(LDRV0_ERR_ASM, LDRV0_ERR_TITLEBLOCK);
            titleBlockError = true;
            goto titleBlockOut;
        }

        FileGeneric f = LoadFileGeneric(block->valuestring, ".mwi");
        if (f.version < 0){ 
            ErrorPrepend(LDRV0_ERR_ASM, LDRV0_ERR_TITLEBLOCK);
            titleBlockError = true;
            goto titleBlockOut;
        } 

        bool loadFlag = false;
        switch (f.version){
            case 0:{
                rs = LoadTitleBlockV0(f.root);
                if (rs == LDRV0_OK){
                    char fullPath[OG_PATH_MAX] = {0};
                    snprintf(fullPath, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), block->valuestring);
                    titleBlocks[titleBlocksQ-1]->sourcePath = calloc(strlen(fullPath)+1, sizeof(char));
                    strcpy(titleBlocks[titleBlocksQ-1]->sourcePath, fullPath);
                }

                else {
                    ErrorPrepend(LDRV0_ERR_ASM);
                    FreeFileGeneric(f);
                    titleBlockError = true;
                    goto titleBlockOut;
                }
            } break;

            default:{
                SetError(LDRV0_ERR_ASM, LDRV0_ERR_TITLEBLOCK, LDRV0_ERR_INVALID, LDRV0_ERR_FILE);
                FreeFileGeneric(f);
                titleBlockError = true;
                goto titleBlockOut;
            } break;
        }
        
        FreeFileGeneric(f);
    }

    // in case of error, free every titleBlock load attempt
    // and returns the originals to them place
    titleBlockOut:
    if (titleBlockError){
        for (int i=0; i<TITLEBLOCKS_Q; i++){
            if (titleBlocks[i]) 
                FreeTitleBlock(titleBlocks[i]);
            titleBlocks[i] = titleBlocksBackup[i];
        }
    
        titleBlocksQ = titleBlocksQBackup;
        return LDRV0_ERR_CODE;
    }

    // if everything was ok then copies the new titleblocks to
    // the backup and returns the originals so they can be freed 
    // on LoadPanelTemplate    
    titleBlocksQBackup = titleBlocksQ;
    for (int i=0; i<TITLEBLOCKS_Q; i++){
        TitleBlock *tmp = titleBlocksBackup[i];
        titleBlocksBackup[i] = titleBlocks[i];
        titleBlocks[i] = tmp;
    }

    //  Verificate the main document section
    JSON_REQUIRE_RET(main_document, j, cJSON_IsObject, LDRV0_ERR_ASM, LDRV0_ERR_DOCUMENT, LDRV0_ERR_SECTION);
    rs = VerificateDocumentV0(main_document);
    if (rs != LDRV0_OK){
        // free every titleBlock in backup
        activeTitleBlock = NULL;
        for (int i=0; i<TITLEBLOCKS_Q; i++){
            if (titleBlocksBackup[i]){
                FreeTitleBlock(titleBlocksBackup[i]);
            }
        }

        return rs;
    }
        
    // VERIFICATIONS DONE!
    // NOW LOADING...
    LoadPanelTemplate(cJSON_GetObjectItem(j, "template")->valuestring, NULL);
    
    // Now reloads the titleblocks backups into the main array
    titleBlocksQ = titleBlocksQBackup;
    for (int i=0; i<TITLEBLOCKS_Q; i++){
        titleBlocks[i] = titleBlocksBackup[i];
    }
    if (titleBlocks[0])
        activeTitleBlock = titleBlocks[0];

    cJSON_ArrayForEach(tree, surface_trees){
        SurfaceTree *st = NULL;
        for (int i=0; i<TREELISTS_Q; i++){
            SurfaceTree *t = treeList[i];
            if (!t) continue;
            if (strcmp(t->baseSurface.id, cJSON_GetObjectItem(tree, "name")->valuestring) == 0)
                st = t;
        }

        activeTree = st;
        
        // Load each anchor
        cJSON *anchor;
        cJSON_ArrayForEach(anchor, cJSON_GetObjectItem(tree, "anchors")){
            char *name = cJSON_GetObjectItem(anchor, "name")->valuestring;
            AnchorType type = cJSON_GetObjectItem(anchor, "type")->valueint;
            uint32_t opts = cJSON_GetObjectItem(anchor, "opts")->valueint;
            float val = cJSON_GetObjectItem(anchor, "val")->valuedouble;
            Anchor *a = AddAnchor(st, name, type, val);
            a->opts = opts;
        }

        // Load each anchor constraint (except those constrained to a surface or as projections)
        cJSON *aConstraint;
        cJSON_ArrayForEach(aConstraint, cJSON_GetObjectItem(tree, "anchors_constraints")){
            char *aName = cJSON_GetObjectItem(aConstraint, "anchor")->valuestring;
            AnchorConstraint aCType = cJSON_GetObjectItem(aConstraint, "type")->valueint;
            
            if (aCType != ANCHOR_CONSTRAINT_PROJECTION && aCType != ANCHOR_CONSTRAINT_INVERSED_PROJECTION && aCType != ANCHOR_CONSTRAINT_SURFACE){
                char *aAName = cJSON_GetObjectItem(aConstraint, "A")->valuestring;
                char *aBName = cJSON_GetObjectItem(aConstraint, "B")->valuestring;

                if (aCType != ANCHOR_CONSTRAINT_NONE){
                    Anchor *a = GetAnchorByName(st, aName);
                    a->constraint = aCType;                
                    a->refA = GetAnchorByName(st, aAName);
                    a->refB = GetAnchorByName(st, aBName);
                    
                    if (a->refA){
                        a->type = a->refA->type;
                        if (a->refB) a->refB->type = a->refA->type;
                    }
                }
            }
        }

        // Load each annotation
        cJSON *annotation;
        cJSON_ArrayForEach(annotation, cJSON_GetObjectItem(tree, "annotations")){
            char *AName = cJSON_GetObjectItem(annotation, "A")->valuestring;
            char *BName = cJSON_GetObjectItem(annotation, "B")->valuestring;
            float offset = cJSON_GetObjectItem(annotation, "offset")->valuedouble;
            float extension = cJSON_GetObjectItem(annotation, "extension")->valuedouble;
            uint32_t opts = cJSON_GetObjectItem(annotation, "opts")->valueint;

            Anchor *A = GetAnchorByName(st, AName);
            Anchor *B = GetAnchorByName(st, BName);
            Annotation *ann = AddAnnotation(st, A, B);
            ann->offset = offset;
            ann->extensionLine = extension;
            ann->opts = opts;
        }
        
        // Load each surface
        cJSON *surface;
        char surfacePath[OG_PATH_MAX] = {0};
        cJSON_ArrayForEach(surface, cJSON_GetObjectItem(tree, "surfaces")){
            char *id = cJSON_GetObjectItem(surface, "id")->valuestring;
            sprintf(surfacePath, "%s.mwc", id);

            SurfaceType type = cJSON_GetObjectItem(surface, "type")->valueint;
            
            cJSON *posJson = cJSON_GetObjectItem(surface, "pos");
            Vector2 pos = {
                cJSON_GetObjectItem(posJson, "x")->valuedouble,
                cJSON_GetObjectItem(posJson, "y")->valuedouble
            };

            Vector2 size;
            cJSON *sizeJson = cJSON_GetObjectItem(surface, "size");
            if (sizeJson){
                size.x = cJSON_GetObjectItem(sizeJson, "w")->valuedouble;
                size.y = cJSON_GetObjectItem(sizeJson, "h")->valuedouble;
            }

            float rotation = cJSON_GetObjectItem(surface, "rotation")->valuedouble;
            uint32_t opts = cJSON_GetObjectItem(surface, "opts")->valueint;
            AnchorAlign anchorAlign = cJSON_GetObjectItem(surface, "anchor_align")->valueint;
            float leftCut = cJSON_GetObjectItem(surface, "left_cut")->valuedouble;
            float rightCut = cJSON_GetObjectItem(surface, "right_cut")->valuedouble;
            int parentIndex = cJSON_GetObjectItem(surface, "parent")->valueint;

            int i=0;
            char *anchors[3] = {0};
            cJSON_ArrayForEach(anchor, cJSON_GetObjectItem(surface, "anchors")){
                if (i < 3 && cJSON_IsString(anchor)){
                    anchors[i] = anchor->valuestring;
                }
                i++;
            }

            SelectSurface(st->linearSurfaceTree[parentIndex+1], false, false);

            // Ensures anchors are selected perpendicular
            for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++)
                st->anchorSelection[i] = NULL;
            
            int order[6][3] = {
                {0,1,2},
                {0,2,1},
                {1,0,2},
                {1,2,0},
                {2,0,1},
                {2,1,0}
            };

            for (int p = 0; p < 6; p++) {
                Anchor *a0 = anchors[order[p][0]] ? GetAnchorByName(st, anchors[order[p][0]]) : NULL;
                Anchor *a1 = anchors[order[p][1]] ? GetAnchorByName(st, anchors[order[p][1]]) : NULL;
                Anchor *a2 = anchors[order[p][2]] ? GetAnchorByName(st, anchors[order[p][2]]) : NULL;
                
                if (a0 && a1 && a2 && a0->type != a1->type && a1->type == a2->type){
                    SelectAnchor(a0, true, false);
                    SelectAnchor(a1, true, false);
                    SelectAnchor(a2, true, false);
                    break;
                }
            }

            Surface *s = NULL;
            if (opts & SURFACE_OPT_GENERIC)
                s = AddSurface(activeTree->surfaceSelection[0], "generic", "doesnt matter hehe", SURFACE_COMPONENT);
            
            else {
                bool r = LoadSurface(surfacePath, NULL);
                s = st->lastInsertedSurface;
            }

            cJSON *name = cJSON_GetObjectItem(surface, "name");
            if (name && cJSON_IsString(name)){
                if (strcmp(name->valuestring, s->name) != 0){
                    char *newName = ResolveSurfaceName(name->valuestring, activeTree);
                    free(s->name);
                    s->name = newName;
                }
            }

            cJSON *label = cJSON_GetObjectItem(surface, "label");
            if (label){
                snprintf(s->skin.label.text, sizeof(s->skin.label.text), "%s", cJSON_GetObjectItem(label, "text")->valuestring);
                s->skin.label.offset.x = cJSON_GetObjectItem(label, "offset_x")->valuedouble;
                s->skin.label.offset.y = cJSON_GetObjectItem(label, "offset_y")->valuedouble;
                s->skin.label.rotation = cJSON_GetObjectItem(label, "rotation")->valuedouble;
            }

            cJSON *colorJson = cJSON_GetObjectItem(surface, "color");
            if (colorJson){
                s->skin.tint.r = cJSON_GetObjectItem(colorJson, "r")->valuedouble;
                s->skin.tint.g = cJSON_GetObjectItem(colorJson, "g")->valuedouble;
                s->skin.tint.b = cJSON_GetObjectItem(colorJson, "b")->valuedouble;
                s->skin.tint.a = 255;
            }

            cJSON *originJson = cJSON_GetObjectItem(surface, "origin");
            if (originJson){
                s->origin.x = cJSON_GetObjectItem(originJson, "x")->valuedouble;
                s->origin.y = cJSON_GetObjectItem(originJson, "y")->valuedouble;
            }

            cJSON *cutoutJson = cJSON_GetObjectItem(surface, "cutouts_rects");
            if (cutoutJson){
                s->cutout.rect.x = cJSON_GetObjectItem(cutoutJson, "w")->valuedouble;
                s->cutout.rect.y = cJSON_GetObjectItem(cutoutJson, "h")->valuedouble;
            }

            s->pos = pos;
            if (sizeJson) s->size = size;
            s->rotation = rotation;
            s->opts = opts;
            s->anchorAlign = anchorAlign;
            s->leftCut = leftCut;
            s->rightCut = rightCut;

            // not really fancy but anyway....
            Solver();
            
            for (int i=0; i<3; i++){
                if (anchors[i]){
                    s->anchors[i] = GetAnchorByName(st, anchors[i]);
                }
            }
        }

        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++)
            st->anchorSelection[i] = NULL;
        
        for (int i=0; i<SURFACE_SELECTION_BUF_LEN; i++)
            st->surfaceSelection[i] = NULL;

        // Load anchor constraints constrained to a surface
        cJSON_ArrayForEach(aConstraint, cJSON_GetObjectItem(tree, "anchors_constraints")){
            char *aName = cJSON_GetObjectItem(aConstraint, "anchor")->valuestring;
            AnchorConstraint aCType = cJSON_GetObjectItem(aConstraint, "type")->valueint;
            if (aCType == ANCHOR_CONSTRAINT_SURFACE){
                cJSON *refSurface = cJSON_GetObjectItem(aConstraint, "refSurface");
                char *surface = cJSON_GetObjectItem(refSurface, "surface")->valuestring;
                char *type = cJSON_GetObjectItem(refSurface, "type")->valuestring;

                Anchor *a = GetAnchorByName(st, aName);
                a->constraint = ANCHOR_CONSTRAINT_SURFACE;
                a->refA = NULL;
                a->refB = NULL;
                a->refSurface = GetSurfaceByName(st, surface);
                
                if (strcmp(type, "bTop") == 0)     a->refSurface->boundariesAnchors.top = a;
                if (strcmp(type, "bBottom") == 0)  a->refSurface->boundariesAnchors.bottom = a;
                if (strcmp(type, "bLeft") == 0)    a->refSurface->boundariesAnchors.left = a;
                if (strcmp(type, "bRight") == 0)   a->refSurface->boundariesAnchors.right = a;
                
                if (strcmp(type, "coTop") == 0)    a->refSurface->cutoutAnchors.top = a;
                if (strcmp(type, "coBottom") == 0) a->refSurface->cutoutAnchors.bottom = a;
                if (strcmp(type, "coLeft") == 0)   a->refSurface->cutoutAnchors.left = a;
                if (strcmp(type, "coRight") == 0)  a->refSurface->cutoutAnchors.right = a;

            }
        }

    }

    // Load projection constraints
    cJSON_ArrayForEach(tree, surface_trees){
        cJSON *aConstraint;
        cJSON_ArrayForEach(aConstraint, cJSON_GetObjectItem(tree, "anchors_constraints")){
            char *aName = cJSON_GetObjectItem(aConstraint, "anchor")->valuestring;
            AnchorConstraint aCType = cJSON_GetObjectItem(aConstraint, "type")->valueint;
            if (aCType == ANCHOR_CONSTRAINT_PROJECTION || aCType == ANCHOR_CONSTRAINT_INVERSED_PROJECTION){

                cJSON *A = cJSON_GetObjectItem(aConstraint, "A");
                cJSON *treeName = cJSON_GetObjectItem(A, "surface_tree");
                cJSON *name = cJSON_GetObjectItem(A, "name");

                SurfaceTree *st0 = NULL;
                SurfaceTree *st1 = NULL;
                for (int i=0; i<TREELISTS_Q; i++){
                    SurfaceTree *t = treeList[i];
                    if (!t) continue;
                    if (strcmp(t->baseSurface.id, cJSON_GetObjectItem(tree, "name")->valuestring) == 0) st0 = t;
                    if (strcmp(t->baseSurface.id, treeName->valuestring) == 0) st1 = t;
                }

                Anchor *a = GetAnchorByName(st0, aName);
                Anchor *b = GetAnchorByName(st1, name->valuestring);
                a->constraint = aCType;                
                a->refA = b;

            }
        }
    }

    // Load documentation
    rs = LoadDocumentV0(main_document);
    if (rs != LDRV0_OK)
        return rs;

    activeTree = treeList[0];
    return LDRV0_OK;
}

/* <== Libraries ===============================================> */

static int VerificateLibNodeV0(cJSON *j, bool noFolder){
    int rs = LDRV0_OK;
    
    if (!cJSON_IsObject(j)){
        SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_TREE, LDRV0_ERR_NODE);
        return LDRV0_ERR_CODE;
    }

    JSON_REQUIRE_RET(name, j, &cJSON_IsString, LDRV0_ERR_LIBRARY, LDRV0_ERR_TREE, LDRV0_ERR_NODE, LDRV0_ERR_NAME);

    // in case it is a folder
    cJSON *content = cJSON_GetObjectItem(j, "content");
    if (content){
        if (noFolder){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_EXTENSIBLE, LDRV0_ERR_NODE);
            return LDRV0_ERR_CODE;
        }

        if (!cJSON_IsArray(content)){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_TREE, LDRV0_ERR_NODE);
            return LDRV0_ERR_CODE;
        }

        JSON_REQUIRE_RET(expanded, j, &cJSON_IsBool, LDRV0_ERR_LIBRARY, LDRV0_ERR_TREE, LDRV0_ERR_NODE);

        cJSON *childNode;
        cJSON_ArrayForEach(childNode, content){
            int rss = VerificateLibNodeV0(childNode, false);
            if (rss != LDRV0_OK)
                return rss;
        }
    }

    // in case it is a component
    else{
        JSON_REQUIRE_RET(path, j, &cJSON_IsString, LDRV0_ERR_LIBRARY, LDRV0_ERR_TREE, LDRV0_ERR_NODE, LDRV0_ERR_PATH);
        JSON_REQUIRE_RET(preview, j, &cJSON_IsArray, LDRV0_ERR_LIBRARY, LDRV0_ERR_TREE, LDRV0_ERR_NODE, LDRV0_ERR_PREVIEW);
        if (cJSON_GetArraySize(preview) != 3){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_TREE, LDRV0_ERR_NODE, LDRV0_ERR_INDEX);
            return LDRV0_ERR_CODE;
        }

        for (int i=0; i<3; i++){
            if (!cJSON_IsNumber(cJSON_GetArrayItem(preview, i))){
                SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_TREE, LDRV0_ERR_NODE, LDRV0_ERR_INDEX);
                return LDRV0_ERR_CODE;
            }
        }
    }
    
    return rs;
}

static int LoadComponentLibraryV0(cJSON *j){
    int rs = LDRV0_OK;
    
    // Verificate the previews atlas
    JSON_REQUIRE_RET(previews, j, &cJSON_IsArray, LDRV0_ERR_LIBRARY, LDRV0_ERR_PREVIEW, LDRV0_ERR_SECTION);
    cJSON *preview;
    cJSON_ArrayForEach(preview, previews){
        if (!cJSON_IsString(preview)){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_PREVIEW);
            return LDRV0_ERR_CODE;
        }

        char atlasPath[OG_PATH_MAX];
        sprintf(atlasPath, "%s%s/data/%s", GetApplicationDirectory(), ROOT, preview->valuestring);
        
        if (!FileExists(atlasPath)){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_MISSING, LDRV0_ERR_PREVIEW, LDRV0_ERR_PATH);
            return LDRV0_ERR_CODE;
        }

        Image img = LoadImage(atlasPath);
        if (img.data == NULL){
            SetError(LDRV0_ERR_LIBRARY, LDRV0_ERR_INVALID, LDRV0_ERR_PREVIEW, LDRV0_ERR_PATH);
            return LDRV0_ERR_CODE;
        }

        UnloadImage(img);
    }

    // Verificate the extensibles
    JSON_REQUIRE_RET(extensibles, j, &cJSON_IsArray, LDRV0_ERR_LIBRARY, LDRV0_ERR_EXTENSIBLE, LDRV0_ERR_SECTION);
    
    cJSON *ext;
    cJSON_ArrayForEach(ext, extensibles){
        int rss = VerificateLibNodeV0(ext, true);
        if (rss != LDRV0_OK){
            return rss;
        }
    }

    // Verificate the component_tree
    JSON_REQUIRE_RET(component_tree, j, &cJSON_IsArray, LDRV0_ERR_LIBRARY, LDRV0_ERR_TREE, LDRV0_ERR_SECTION);

    cJSON *node;
    cJSON_ArrayForEach(node, component_tree){
        int rss = VerificateLibNodeV0(node, false);
        if (rss != LDRV0_OK){
            return rss;
        }
            
    }

    // Everything is ok, now is loading time
    int previewI = 0;
    cJSON_ArrayForEach(preview, previews){
        char atlasPath[OG_PATH_MAX];
        sprintf(atlasPath, "%s%s/data/%s", GetApplicationDirectory(), ROOT, preview->valuestring);
        Image img = LoadImage(atlasPath);
        componentsLib.atlas[previewI++] = LoadTextureFromImage(img);
    }

    componentsLib.extensibles = cJSON_Duplicate(extensibles, true);
    componentsLib.tree = cJSON_Duplicate(component_tree, true);
    return LDRV0_OK;
}

/* <== Public API ==============================================> */
/*  all these functions should be called from the file selection dialog
    so they all asume 'path' is relative to pwd */

bool LoadSticker(char *path, void *param){
    Image img = LoadImage(path);
    if (img.data == NULL){
        OG_PushLog("Could not load image from '%s'", path);
        return false;
    }

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    unsigned int hash = ComputeCRC32(
        (const unsigned char *) img.data, 
        img.width * img.height * 3
    );

    for (Sticker *s = stickers; s != NULL; s = s->next){
        if (s->hash == hash){
            UnloadImage(img);
            lastAddedSticker = s;
            return true;
        }
    }

    // Actually loading
    Sticker *newSticker = calloc(1, sizeof(Sticker));
    newSticker->hash = hash;
    newSticker->img = img;
    newSticker->tex = LoadTextureFromImage(img);

    char fullPath[OG_PATH_MAX] = {0};
    snprintf(fullPath, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), path);
    newSticker->srcPath = calloc(strlen(fullPath)+1, sizeof(char));
    strcpy(newSticker->srcPath, fullPath);
    
    for (Sticker *s = stickers; s != NULL; s = s->next){
        if (s->next == NULL){
            s->next = newSticker;
            break;
        }
    }
    
    if (stickers == NULL)
        stickers = newSticker;

    lastAddedSticker = newSticker;
    return true;
}

bool LoadTitleBlock(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;

    FileGeneric f = LoadFileGeneric(path, ".mwi");
    HANDLE_GENERAL_ERRORS;

    char fullPath[OG_PATH_MAX] = {0};
    snprintf(fullPath, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), path);
    for (int i=0; i<titleBlocksQ; i++){
        const char *s0 = GetFileName(titleBlocks[i]->sourcePath);;
        const char *s1 = GetFileName(fullPath);
        
        if (s0 && s1 && strcmp(s0, s1) == 0){
            OG_PushLog("A title block named '%s' already exists in the project", s0);
            FreeFileGeneric(f);
            return false;
        }
    }

    switch (f.version){
        case 0:{
            rs = LoadTitleBlockV0(f.root);
            if (rs == LDRV0_OK)
                loadFlag = true;

            else OG_PushLog("File %s is invalid or corrupted. (err code):%s", path, errCode);
        } break;

        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
        } break;
    }

    
    if (loadFlag){
        activeTitleBlock->sourcePath = calloc(strlen(fullPath)+1, sizeof(char));
        strcpy(activeTitleBlock->sourcePath, fullPath);
    }

    FreeFileGeneric(f);
    return loadFlag;
}

bool LoadPanelTemplate(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;

    FileGeneric f = LoadFileGeneric(path, ".mwt");
    HANDLE_GENERAL_ERRORS;
    
    switch (f.version){
        case 0:{ 
            rs = LoadTemplateV0(f.root);
            if (rs == LDRV0_OK)
                loadFlag = true;

            else if (ErrorHas(LDRV0_ERR_SKIN, 0)){
                OG_PushLog("WARNING: Error loading skin. (err code):%s", errCode);
                loadFlag = true; // it is a warning, not an error
            }

            else OG_PushLog("File %s is invalid or corrupted. (err code:%s)", path, errCode);
            break;
        }

        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
            break;
        }
    }
    
    // saves the original path if everything is ok
    if (loadFlag){
        snprintf(
            templateSourcePath, 
            OG_PATH_MAX, 
            "%s/%s", 
            GetWorkingDirectory(), 
            path
        );

        currentAssemblyPath[0] = '\0';
        ghostTree = NULL;
        ghostTreeFlipX = false;
        ghostTreeFlipY = false;

        // Deletes all documentation
        for (Page *p=mainDocument.pages; p != NULL;){
            Page *toDel = p;
            p = p->next;
            DeletePage(toDel);
        }

        // Delete title rects
        titleBlocksQ = 0;
        activeTitleBlock = NULL;
        for (int i=0; i<TITLEBLOCKS_Q; i++){
            if (titleBlocks[i]){
                FreeTitleBlock(titleBlocks[i]);
                titleBlocks[i] = NULL;
            }
        }
    }

    FreeFileGeneric(f);
    return loadFlag;
}

bool ReloadPanelTemplate(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;

    FileGeneric f = LoadFileGeneric(path, ".mwt");
    HANDLE_GENERAL_ERRORS;

    switch (f.version){
        case 0:{
            rs = ReloadTemplateV0(f.root);
            if (rs == LDRV0_OK)
                loadFlag = true;

            else if (ErrorHas(LDRV0_ERR_SKIN, 0)){
                OG_PushLog("WARNING: Error loading skin. (err code):%s", errCode);
                loadFlag = true; // it is a warning, not an error
            }

            else if (ErrorIs(LDRV0_ERR_TEMPLATE, LDRV0_ERR_MISMATCH, 0))
                OG_PushLog("Panel template %s is incompatible with the current panel", path);
            
            else OG_PushLog("File %s is invalid or corrupted. (err code:%s)", path, errCode);
            break;
        }

        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
            break;
        }
    }

    // saves the original path if everything is ok
    if (loadFlag){
        snprintf(
            templateSourcePath, 
            OG_PATH_MAX, 
            "%s/%s", 
            GetWorkingDirectory(), 
            path
        );

        currentAssemblyPath[0] = '\0';
    }

    FreeFileGeneric(f);
    return loadFlag;
}

bool LoadSurface(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;
    
    FileGeneric f = LoadFileGeneric(path, ".mwc");
    HANDLE_GENERAL_ERRORS;

    switch (f.version){
        case 0:{
            rs = LoadSurfaceV0(f.root, param);
            if (rs == LDRV0_OK)
                loadFlag = true;
            
            else {
                if (ErrorIs(LDRV0_ERR_ANCHOR, LDRV0_ERR_MISSING, 0))
                    OG_PushLog("At least 3 anchors required");
                else if (ErrorIs(LDRV0_ERR_ANCHOR, LDRV0_ERR_INVALID, 0))
                    OG_PushLog("Selected anchors have incompatible orientations");
                else if (ErrorHas(LDRV0_ERR_SKIN, 0)){
                    OG_PushLog("WARNING: File Skin section is invalid. (err code):%s", errCode);
                    loadFlag = true; // it is a warning, not an error
                }

                else OG_PushLog("File %s is invalid or corrupted. (err code):%s", path, errCode);
            }
            break;
        }

        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
            break;
        }
    }

    // saves the original path if everything is ok
    if (loadFlag){
        const char *cwd = GetWorkingDirectory();
        snprintf(
            activeTree->lastInsertedSurface->sourcePath, 
            OG_PATH_MAX, 
            "%s/%s", 
            cwd, 
            path
        );
    }
    
    FreeFileGeneric(f);
    return loadFlag;
}

bool LoadAssembly(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;

    FileGeneric f = LoadFileGeneric(path, ".mwa");
    HANDLE_GENERAL_ERRORS;
    loadedAssemblyFile = &f;
    
    switch (f.version){
        case 0:
        case 1:{ 
            rs = LoadAssemblyV1(f.root);
            if (invalidSurfaceFilename[0] != '\0'){
                OG_PushLog("Surface file is missing or invalid: %s", invalidSurfaceFilename);
                invalidSurfaceFilename[0] = '\0';
            }

            if (rs == LDRV0_OK) loadFlag = true;
            else OG_PushLog("Failed to load assembly %s: (err code):%s", path, errCode);

            break;
        }

        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
            break;
        }
    }

    if (loadFlag){
        snprintf(
            currentAssemblyPath, 
            OG_PATH_MAX, 
            "%s/%s", 
            GetWorkingDirectory(), 
            path
        );

        OG_PushLog("Panel assembly loaded successfully: %s", currentAssemblyPath);
    }
    
    loadedAssemblyFile = NULL;
    FreeFileGeneric(f);

    return loadFlag;
}

bool LoadComponentLibrary(char *path, void *param){
    int rs = 0;
    errCode[0] = '\0';
    bool loadFlag = false;
    
    FileGeneric f = LoadFileGeneric(path, ".mwl");
    HANDLE_GENERAL_ERRORS;

    switch (f.version){
        case 0:{
            loadFlag = LoadComponentLibraryV0(f.root);
            if (loadFlag != LDRV0_OK){
                OG_PushLog("Failed to load component lib %s: (err code):%s", path, errCode);
            }
        } break;
        
        default:{
            OG_PushLog("File %s is invalid or corrupted (version unknown)", path);
            break;
        }
    }

    FreeFileGeneric(f);
    return loadFlag;
}