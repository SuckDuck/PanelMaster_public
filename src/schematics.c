// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include "schematics.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Schematic *AddSchematic(Schematic *list, char *name){
    Schematic *new = calloc(1, sizeof(Schematic));
    if (list) new->list = list;
    new->name = ResolveSchematicName(list, name);

    for (Schematic *l = list; l != NULL; l = l->next){
        if (l->next == NULL) {
            l->next = new;
            break;
        }
    }

    return new;
}

char *ResolveSchematicName(Schematic *list, char *name){
    if (!list) return strdup(name);

    char *rsName = strdup(name);
    int suffix = 0;
    char *baseName = name;

    char *underscore = strrchr(name, '_');
    if (underscore && underscore[1] != '\0'){
        char *end;
        long n = strtol(underscore + 1, &end, 10);
        if (*end == '\0' && n >= 0){
            baseName = strndup(name, underscore - name);
            suffix = n + 1;
        }
    }

    Schematic *l = list;
    while (true){
        if (strcmp(rsName, l->name) == 0){
            free(rsName);

            char buffer[16];
            snprintf(buffer, sizeof(buffer), "_%d", suffix++);

            rsName = malloc(strlen(baseName) + strlen(buffer) + 1);
            sprintf(rsName, "%s%s", baseName, buffer);

            l = list;
            continue;
        }

        l = l->next;
        if (l == NULL)
            break;
    }

    if (baseName != name) free(baseName);
    return rsName;
}

Symbol *AddSymbol(Schematic *schematic){
    Symbol *new = calloc(1, sizeof(Symbol));
    
    if (schematic->symbols){
        for (Symbol *s = schematic->symbols; s != NULL; s = s->next){
            if (s->next == NULL){
                s->next = new;
                break;
            }
        }
    }

    else schematic->symbols = new;
    
    // momentaneo
    new->size = (Vector2){20, 20};
    new->pos = (Vector2){0,0};

    return new;
}

void DrawSymbol(Symbol *s){
    DrawRectangleLinesEx(
        (Rectangle){
            s->pos.x - s->size.x/2,
            s->pos.y - s->size.y/2,
            s->size.x,
            s->size.y
        }, 
        1.0f, 
        RED
    );
}