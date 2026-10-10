// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef MAIN_H
#define MAIN_H

#include "titleblock.h"
#include <raylib.h>

#ifndef PROJECT_TITLE
#define PROJECT_TITLE ""
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION ""
#endif

#ifndef ROOT
#define ROOT ".."
#endif

#define HIGHLIGHT_C  CLITERAL(Color){ 000,121,241,255 }
#define HIGHLIGHT_C1 CLITERAL(Color){ 120,170,255,200 }
#define HIGHLIGHT_C2 CLITERAL(Color){ 000,200,200,200 }
#define TREELISTS_Q 32
#define TITLEBLOCKS_Q 32
#define HATCH_SPACE 3.0f

#include "surfaces.h"
#include "documentation.h"
#include "caddraw.h"
#include "componentslib.h"

extern int titleBlocksQ;
extern TitleBlock *titleBlocks[];
extern TitleBlock *activeTitleBlock;

extern int treeListQ;
extern SurfaceTree *treeList[];
extern SurfaceTree *activeTree;
extern SurfaceTree *ghostTree;
extern bool ghostTreeFlipX;
extern bool ghostTreeFlipY;

extern int lodsQ;
extern float lods[];

extern Document mainDocument;

extern char templateSourcePath[];
extern char currentAssemblyPath[];

extern CadFont *cadFont;

extern ComponentsLib componentsLib;

#endif