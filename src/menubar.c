// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "documentation.h"
#include "origami.h"
#include "microui.h"
#include "main.h"
#include "loader.h"
#include "origami_tools.h"
#include "saver.h"
#include "titleblock.h"


static char helpUrl[OG_PATH_MAX];

static const char *commands[] = {
    NULL
};

static void DrawFrame(mu_Context *ctx, mu_Rect rect, int colorid){
    mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
    if (colorid == MU_COLOR_SCROLLBASE || colorid == MU_COLOR_SCROLLTHUMB || colorid == MU_COLOR_TITLEBG)
        return;
}

static void OpenWorkspace(bool s){ if (s) OG_OpenViewportByName("Workspace"); }
static void OpenTitleBlockEditor(bool s){ if (s) OG_OpenViewportByName("TitleBlockEditor"); }

static void FileContextMenu(char *opt, void *param){
    if (strcmp(opt, "New Panel") == 0){
        OG_OpenFileDialog(&LoadPanelTemplate, NULL, &OpenWorkspace, OG_FD_MODE_SELECT_FILE, ".mwt", "Select a panel template [.mwt]");
        return;
    }
        
    if (strcmp(opt, "Open Panel") == 0){
        OG_OpenFileDialog(&LoadAssembly, NULL, &OpenWorkspace, OG_FD_MODE_SELECT_FILE, ".mwa", "Select an assembly [.mwa]");
        return;
    }
        
    if (strcmp(opt, "Change Panel") == 0){
        OG_OpenFileDialog(&ReloadPanelTemplate, NULL, &OpenWorkspace, OG_FD_MODE_SELECT_FILE, ".mwt", "Select a panel template [.mwt]");
        return;
    }
        
    if (strcmp(opt, "Save Panel as") == 0){
        for (int i=0; i<titleBlocksQ; i++){
            if (!titleBlocks[i]->sourcePath){
                OG_PushLog("Cannot save assembly, one or more title blocks have not been saved");
                return ;
            }
        }

        OG_OpenFileDialog(&SavePanelAssembly, NULL, &OpenWorkspace, OG_FD_MODE_SELECT_FILE, NULL, "Save current assembly");
        return;
    }
    
    if (strcmp(opt, "Save Panel") == 0){
        for (int i=0; i<titleBlocksQ; i++){
            if (!titleBlocks[i]->sourcePath){
                OG_PushLog("Cannot save assembly, one or more title blocks have not been saved");
                return ;
            }
        }
        
        if (FileExists(currentAssemblyPath)){
            char cwd[OG_PATH_MAX];
            strcpy(cwd, GetWorkingDirectory());
            SavePanelAssembly(currentAssemblyPath, NULL);
            ChangeDirectory(cwd);
        }
            
        else OG_OpenFileDialog(&SavePanelAssembly, NULL, &OpenWorkspace, OG_FD_MODE_SELECT_FILE, NULL, "Save current assembly");    
        return;
    }

    if (strcmp(opt, "New Title Block") == 0){
        titleBlocks[titleBlocksQ] = NewTitleBlock(NULL);
        activeTitleBlock = titleBlocks[titleBlocksQ++];
        OpenTitleBlockEditor(true);
        return;
    }

    if (strcmp(opt, "Open Title Block") == 0){
        OG_OpenFileDialog(
            &LoadTitleBlock, 
            NULL, 
            &OpenTitleBlockEditor, 
            OG_FD_MODE_SELECT_FILE, 
            ".mwi", 
            "Select a title block file [.mwi]"
        );
        return;
    }

    if (strcmp(opt, "Save Title Block as") == 0){
        if (activeTitleBlock){
            OG_OpenFileDialog( &SaveTitleBlock, NULL, NULL, OG_FD_MODE_SELECT_FILE, NULL, "Save current title block" );
        }
        return;
    }

    if (strcmp(opt, "Save Document as") == 0){
        if (mainDocument.pagesQ <= 0){
            OG_PushLog("Cannot save document, document is empty");
            return;
        }

        OG_OpenFileDialog(
            &SaveMainDoc, 
            NULL, 
            NULL, 
            OG_FD_MODE_SELECT_FILE, 
            NULL, 
            "Save current open document [.pdf]"
        );

    }
}

static void ViewContextMenu(char *opt, void *param){    
    if (strcmp(opt, "Workspace") == 0){
        OG_OpenViewportByName("Workspace");
        return;
    }

    if (strcmp(opt, "Document Editor") == 0){
        OG_OpenViewportByName("DocumentEditor");
        return;
    }

    if (strcmp(opt, "TitleBlock Editor") == 0){
        OG_OpenViewportByName("TitleBlockEditor");
        return;
    }
}

static void Init(OG_Viewport *v){
    v->ctx.style->spacing = 0;
    v->ctx.style->padding = 0;
    v->ctx.draw_frame = &DrawFrame;

    sprintf(helpUrl, "file:///%shelp/index.html", GetApplicationDirectory());
}

static void Update(OG_Viewport *v){
    v->pos = (Vector2){0,-2};
    int sw = GetScreenWidth();
    if (v->size.width != sw)
        OG_ResizeViewport(v, sw, v->size.height*-1);
    
    if (v->hidden)
        OG_ToggleViewport(v);
    
    Vector2 mousePos = GetMousePosition();
    
    // For each viewport
    for (OG_Viewport *vv = OG.viewports.head; vv != NULL; vv = vv->next){
        if (vv == v) continue;
        if (vv->hidden) continue;
        if (vv->isModal) continue;

        if (vv->pos.y <= 18){
            OG.state = OG_STATE_IDLE;
            vv->pos.y = 19;
        }
    }

}

static void UI(OG_Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 3, (const int[]){50,50, 50}, 19);
    
    if (mu_button_ex(ctx, "File", 0, MU_OPT_ALIGNCENTER | (OG.viewports.tail == v ? 0:MU_OPT_NOINTERACT))){
        char *opts[] = {
            "New Panel",
            "Open Panel",
            activeTree ? "Change Panel":NULL,
            activeTree ? "Save Panel as":NULL,
            activeTree ? "Save Panel":NULL,
            "New Title Block",
            "Open Title Block",
            activeTitleBlock ? "Save Title Block as":NULL,
            mainDocument.pagesQ > 0 ? "Save Document as":NULL
        };

        OG_OpenContextMenuV3(&FileContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
        ctx->hover = 0;
        ctx->focus = 0;
    }
    
    if (mu_button_ex(ctx, "View", 0, MU_OPT_ALIGNCENTER | (OG.viewports.tail == v ? 0:MU_OPT_NOINTERACT))){
        char *opts[] = {
            "Workspace",
            "Document Editor",
            "TitleBlock Editor"
        };

        OG_OpenContextMenuV3(&ViewContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
        ctx->hover = 0;
        ctx->focus = 0;
    }

    if (mu_button_ex(ctx, "Help", 0, MU_OPT_ALIGNCENTER | (OG.viewports.tail == v ? 0:MU_OPT_NOINTERACT))){
        printf("%s\n", helpUrl);
        OpenURL(helpUrl);
    }
    
}

static void ExecCmd(OG_Viewport *v, int argc, char **argv){}
static const char **GetCmds(){
    return commands;
}

void MenuBar(){
    OG_Viewport *v = OG_InitViewport(
        "MenuBar", 
        (Rectangle){0, 0, 200, 20}, 
        1.0f, 
        1.0f, 
        true, false, false, 
        &Init, 
        &Update, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        &UI, 
        &GetCmds, 
        &ExecCmd
    );
    
    v->updateAlways = true;
    v->renderAlways = true;
    v->noTitleBar = true;
    v->noBorder = true;
}