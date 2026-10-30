// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef LOADER_H
#define LOADER_H

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

typedef enum LoaderErrorV0 {
    LDRV0_OK                     =  0,
    LDRV0_FILE_MISSING           = -1,
    LDRV0_INVALID_FILE_EXTENSION = -2,
    LDRV0_IO_ERROR               = -3,
    LDRV0_CORRUPTED_FILE         = -4,
    LDRV0_INVALID_VILE           = -5,
    LDRV0_ERR_CODE               = -6
} LoaderErrorV0;

typedef enum LoaderErrorCodesV0 {
    LDRV0_ERR_FILE          = '0',
    LDRV0_ERR_SURFACE       = '1',
    LDRV0_ERR_SKIN          = '2',
    LDRV0_ERR_ASM           = '3',
    LDRV0_ERR_TEMPLATE      = '4',
    LDRV0_ERR_TREE          = '5',
    LDRV0_ERR_ANCHOR        = '6',
    LDRV0_ERR_IO            = '7',
    LDRV0_ERR_MISSING       = '8',
    LDRV0_ERR_INVALID       = '9',
    LDRV0_ERR_CORRUPTED     = 'A',
    LDRV0_ERR_UNKNOWN       = 'B',
    LDRV0_ERR_VERSION       = 'C',
    LDRV0_ERR_SECTION       = 'D',
    LDRV0_ERR_ID            = 'E',
    LDRV0_ERR_NAME          = 'F',
    LDRV0_ERR_TYPE          = 'G',
    LDRV0_ERR_A             = 'H',
    LDRV0_ERR_B             = 'I',
    LDRV0_ERR_X             = 'J',
    LDRV0_ERR_Y             = 'K',
    LDRV0_ERR_R             = 'L',
    LDRV0_ERR_G             = 'M',
    LDRV0_ERR_WIDTH         = 'N',
    LDRV0_ERR_HEIGHT        = 'O',
    LDRV0_ERR_TINT          = 'P',
    LDRV0_ERR_POINTS        = 'Q',
    LDRV0_ERR_LOD           = 'R',
    LDRV0_ERR_ZOOM          = 'S',
    LDRV0_ERR_LINE          = 'T',
    LDRV0_ERR_LEN           = 'U',
    LDRV0_ERR_CONSTRAINT    = 'V',
    LDRV0_ERR_ANNOTATION    = 'W',
    LDRV0_ERR_OPTS          = 'X',
    LDRV0_ERR_VAL           = 'Y',
    LDRV0_ERR_POS           = 'Z',
    LDRV0_ERR_ROT           = 'a',
    LDRV0_ERR_ALIGN         = 'b',
    LDRV0_ERR_LCUT          = 'c',
    LDRV0_ERR_RCUT          = 'd',
    LDRV0_ERR_PARENT        = 'e',
    LDRV0_ERR_MISMATCH      = 'f',
    LDRV0_ERR_TITLEBLOCK    = 'g',
    LDRV0_ERR_SIZE          = 'h',
    LDRV0_ERR_T             = 'i',
    LDRV0_ERR_RECT          = 'j',
    LDRV0_ERR_TEXT          = 'k',
    LDRV0_ERR_LABEL         = 'l',
    LDRV0_ERR_EXTERNAL      = 'm',
    LDRV0_ERR_NO            = 'n',
    LDRV0_ERR_DOCUMENT      = 'o',
    LDRV0_ERR_PAGE          = 'p',
    LDRV0_ERR_DRAW          = 'q',
    LDRV0_ERR_FRAME         = 'r',
    LDRV0_ERR_TOP           = 's',
    LDRV0_ERR_BOTTOM        = 't',
    LDRV0_ERR_LEFT          = 'u',
    LDRV0_ERR_RIGHT         = 'v',
    LDRV0_ERR_ENTITY        = 'w',
    LDRV0_ERR_PROP          = 'x',
    LDRV0_ERR_CUTOUT        = 'y',
    LDRV0_ERR_FLAGS         = 'z',
    LDRV0_ERR_BOUNDARIES    = '?',
    LDRV0_ERR_EXTENSION     = '_',
    LDRV0_ERR_LIBRARY       = '!',
    LDRV0_ERR_PREVIEW       = '-',
    LDRV0_ERR_PATH          = '@',
    LDRV0_ERR_NODE          = '|',
    LDRV0_ERR_INDEX         = '/',
    LDRV0_ERR_EXTENSIBLE    = '\\',
    LDRV0_ERR_ORG           = '<',
} LoaderErrorCodesV0;

// LDRV0_ERR_MISSING LDRV0_ERR_ASM LDRV0_ERR_DOCUMENT LDRV0_ERR_ENTITY LDRV0_ERR_FLAGS

typedef struct FileGeneric {
    cJSON *root;
    int version;
} FileGeneric;

bool LoadSticker(char *path, void *param);
bool LoadTitleBlock(char *path, void *param);
bool LoadPanelTemplate(char *path, void *param);
bool ReloadPanelTemplate(char *path, void *param);
bool LoadSurface(char *path, void *param);
bool LoadAssembly(char *path, void *param);
bool LoadComponentLibrary(char *path, void *param);

#endif