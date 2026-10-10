// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include "origami.h"
#include "cJSON.h"

static float ProcessComponentNode(OG_Viewport *v, cJSON *node, Vector2 *offset, Vector2 (*perNode)(OG_Viewport*, cJSON*, Vector2, void*), void *data){
    Vector2 advance = perNode(v, node, *offset, data);
    offset->y += advance.y;
    return advance.y;
}

static float ProcessComponentsFolder(OG_Viewport *v, cJSON *node, Vector2 *offset, Vector2 (*perFolder)(OG_Viewport*, cJSON*, Vector2, bool, void*), Vector2 (*perNode)(OG_Viewport*, cJSON*, Vector2, void*), void *data){
    cJSON *expanded = cJSON_GetObjectItem(node, "expanded");

    Vector2 advance = perFolder(v, node, *offset, expanded->valueint, data);
    offset->y += advance.y;
    offset->x += advance.x;
    float contentH = advance.y;
    
    if (expanded->valueint){
        cJSON *child;
        cJSON *content =  cJSON_GetObjectItem(node, "content");
        cJSON_ArrayForEach(child, content){
            bool isFolder = cJSON_GetObjectItem(child, "content") != NULL;
            if (isFolder){
                contentH += ProcessComponentsFolder(v, child, offset, perFolder, perNode, data);
                continue;
            }

            contentH += ProcessComponentNode(v, child, offset, perNode, data);
        }
    }

    offset->x -= advance.x;
    return contentH;
}

float ProcessComponentsLib(OG_Viewport *v, cJSON *tree, Vector2 defaultOffset, Vector2 (*perFolder)(OG_Viewport*, cJSON*, Vector2, bool, void*), Vector2 (*perNode)(OG_Viewport*, cJSON*, Vector2, void*), void *data){
    Vector2 offset = defaultOffset;
    float contentH = 0;

    cJSON *i;
    cJSON_ArrayForEach(i, tree){
        bool isFolder = cJSON_GetObjectItem(i, "content") != NULL;
        if (isFolder){
            contentH += ProcessComponentsFolder( v, i, &offset, perFolder, perNode, data);
            continue;
        }

        contentH += ProcessComponentNode(v, i, &offset, perNode, data);
        
    }

    return contentH;
}