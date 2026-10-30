// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef SYMBOL_H
#define SYMBOL_H

#include <raylib.h>

typedef struct Schematic Schematic;
typedef struct Symbol Symbol;

struct Schematic {
    char *name;
    Symbol *symbols;

    Schematic *list;
    Schematic *next;
};

struct Symbol {
    Vector2 pos;
    Vector2 size;

    Symbol *next;   
};

extern Schematic *schematics;
extern Schematic *activeSchematic;

Schematic *AddSchematic(Schematic *list, char *name);
char *ResolveSchematicName(Schematic *list, char *name);
Symbol *AddSymbol(Schematic *schematic);
void DrawSymbol(Symbol *s);

#endif