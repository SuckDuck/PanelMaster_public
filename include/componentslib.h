// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef COMPONENTS_LIB_H
#define COMPONENTS_LIB_H

#include "origami.h"
#include "cJSON.h"
#include <raylib.h>

typedef struct ComponentsLib {
    Texture2D atlas[232];
    cJSON *extensibles;
    cJSON *tree;
} ComponentsLib;

float ProcessComponentsLib(OG_Viewport *v, cJSON *tree, Vector2 defaultOffset, Vector2 (*perFolder)(OG_Viewport*, cJSON*, Vector2, bool, void*), Vector2 (*perNode)(OG_Viewport*, cJSON*, Vector2, void*), void *data);

#endif