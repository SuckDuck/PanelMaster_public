// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "caddraw.h"
#include "loader.h"
#include "origami.h"
#include "surfaces.h"
#include "titleblock.h"
#include "viewports.h"
#include "documentation.h"
#include "main.h"
#include "componentslib.h"
#include "IBMPlexMono-Medium.h"
#include "Icon.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

extern void Solver();
extern float gridSpacing;

// Super globals ?
CadFont *cadFont;

int treeListQ = 0;
SurfaceTree *treeList[TREELISTS_Q];
SurfaceTree *activeTree = NULL;
SurfaceTree *ghostTree = NULL;
bool ghostTreeFlipX = false;
bool ghostTreeFlipY = false;

int lodsQ = 3;
float lods[] = {
    0.0f,
    0.9f,
    1.7f
};

char templateSourcePath[OG_PATH_MAX];
char currentAssemblyPath[OG_PATH_MAX];

Document mainDocument = {
    .width = DEFAULT_PAGE_W, 
    .height = DEFAULT_PAGE_H
};

int titleBlocksQ = 0;
TitleBlock *titleBlocks[TITLEBLOCKS_Q];
TitleBlock *activeTitleBlock;

ComponentsLib componentsLib;

static Texture2D bgTex;

static void RenderBackground(){
    DrawTextureEx(
        bgTex, 
        (Vector2){
            (int)(GetScreenWidth()/2.0f - bgTex.width/2.0f),
            (int)(GetScreenHeight()/2.0f - bgTex.height/2.0f)
        }, 
        0.0f, 1.0f, 
        WHITE
    );
}

int main(int argc, char **argv){
    MenuBar();
    Workspace();
    DocumentEditor();
    TitleBlockEditor();
    SchematicEditor();

    OG_Init(PROJECT_TITLE, 60);
    
    // Set window icon
    Image icon = LoadImageFromMemory(".png", Icon_png, Icon_png_len);
    SetWindowIcon(icon);
    
    // Set background Image
    OG.RenderBackground = &RenderBackground;
    bgTex = LoadTextureFromImage(icon);
    UnloadImage(icon);
    SetTextureFilter(bgTex, TEXTURE_FILTER_POINT);

    cadFont = LoadCadFont(
        IBMPlexMono_Medium_ttf,
        IBMPlexMono_Medium_ttf_len,
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        " !\"#$%&'()*+,-./:;<=>∧⋀⌃?@[\\]^_`{|}~θ°"
        "0123456789 "
    );

    // Load Components libraries
    char componentsLibrariePath[OG_PATH_MAX];
    sprintf(componentsLibrariePath, "%s%s/data/component_library.mwl", GetApplicationDirectory(), ROOT);
    LoadComponentLibrary(componentsLibrariePath, NULL);

    #ifdef __EMSCRIPTEN__
        ChangeDirectory("panelmaster");
    #endif

    while (true) {
        
        #ifdef __EMSCRIPTEN__
            double width, height;
            emscripten_get_element_css_size("#canvas", &width, &height);
            if (GetScreenWidth() != (int)width || GetScreenHeight() != (int)height){
                SetWindowSize((int)width, (int)height);
            }
        #endif
        
        if (OG_UpdateFrame()) break;
        Solver();
        if (OG_RenderFrame()) break;
    }

    OG_Free();
    FreeCadFont(cadFont);
    UnloadTexture(bgTex);
    return 0;
}