#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include "microui.h"
#include "origami.h"
#include "origami_tools.h"

#define OPT_HEIGHT 20
#define MAX_HEIGHT 250

typedef struct ContextMenu {
    char** options;
    int optionsQ;
    void (*callback)(int, void*);
    void (*callbackStr)(char*, void*);
    void *callbackParam;
} ContextMenu;

static const char *commands[] = {
    NULL
};

static OG_Viewport *this;
static ContextMenu menus[32];
static int menusQ = 0;
static bool dontCloseFlag = false;

static void CloseContextMenu(){
    for (int i=0; i<menus[0].optionsQ; i++)
        free(menus[0].options[i]);
    free(menus[0].options);

    for (int i = 1; i < 32; i++) {
        menus[i - 1] = menus[i];
    }

    menusQ--;

    if (menusQ == 0)
        OG_CloseViewportByName("ContextMenu");
}

static void CloseAllContextMenus() {
    for (int m = 0; m < menusQ; m++) {
        for (int i = 0; i < menus[m].optionsQ; i++)
            free(menus[m].options[i]);
        free(menus[m].options);

        menus[m].options = NULL;
        menus[m].optionsQ = 0;
        menus[m].callback = NULL;
        menus[m].callbackStr = NULL;
    }

    menusQ = 0;
    OG_CloseViewportByName("ContextMenu");
}

static void Init(OG_Viewport *v){
    this = v;
    v->ctx.style->spacing = 0;
    v->ctx.style->padding = 0;
    v->ctx.style->colors[MU_COLOR_BORDER] = v->ctx.style->colors[MU_COLOR_BUTTON];
        
}

static void Update(OG_Viewport *v){
    if (IsKeyPressed(KEY_ESCAPE)){
        CloseAllContextMenus();
    }
    
    if (v->hidden){
        if (menusQ > 0)
            OG_OpenViewportByName("ContextMenu");    
        return;
    }

    if (OG.viewports.tail != v) return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !dontCloseFlag){
        if (!OG_MouseInViewport(v, false, false, false)){
            CloseAllContextMenus();
        }
    }

    dontCloseFlag = false;
}

static void UI(OG_Viewport *v, mu_Context *ctx){
    if (menusQ <= 0) return;
    
    if (menus[menusQ-1].options){
        mu_layout_row(ctx, 1, (const int[]){-1}, OPT_HEIGHT);
        for (int i=0; i<menus[menusQ-1].optionsQ; i++){
            if (mu_button_ex(ctx, menus[menusQ-1].options[i], 0, 0)){
                OG_ToggleViewport(v);
                if (menus[menusQ-1].callback) menus[menusQ-1].callback(i, menus[menusQ-1].callbackParam);
                else if (menus[menusQ-1].callbackStr) menus[menusQ-1].callbackStr(menus[menusQ-1].options[i], menus[menusQ-1].callbackParam);
                CloseContextMenu();
            }

            if (menusQ <= 0){
                CloseAllContextMenus();
                return;
            }
        
        }
    }

}

static void Render(OG_Viewport *v){}

static const char **GetCmds(){
    return commands;
}

void OG_ContextMenu(){
    OG_Viewport *v = OG_InitViewport(
        "ContextMenu", 
        (Rectangle){0,0,250, 250}, 
        1.0f, 1.0f,
        true, false, true, 
        &Init, 
        &Update, 
        NULL,
        &Render, 
        NULL, 
        NULL,
        NULL, 
        &UI,
        &GetCmds, 
        NULL
    );

    v->updateAlways = true;
}

static ContextMenu *OpenContextMenuHelper(void (*_callback)(int, void*), void *param, int optQ){
    Vector2 mousePos = GetMousePosition();
    if (menusQ == 0)
        this->pos = mousePos;

    menus[menusQ].options = calloc(optQ, sizeof(char*));
    menus[menusQ].optionsQ = optQ;

    menus[menusQ].callback = _callback;
    menus[menusQ].callbackStr = NULL;

    menus[menusQ].callbackParam = param;
    OG_ResizeViewport(
        this, this->size.width, 
        (optQ*OPT_HEIGHT+1) > MAX_HEIGHT?MAX_HEIGHT:(optQ*OPT_HEIGHT+1)
    );
    
    menusQ++;
    dontCloseFlag = true;
    return &menus[menusQ-1];
}

void OG_OpenContextMenu(void (*_callback)(int, void*), void *param, char *hint, int optQ, ...){
    OG_SetHeader(this, hint);
    this->noTitleBar = hint == NULL;
    
    ContextMenu *cm = OpenContextMenuHelper(_callback, param, optQ);

    va_list args;
    va_start(args, optQ);
    for (int i = 0; i < optQ; i++) {
        char *opt = va_arg(args, char*);
        cm->options[i] = calloc(strlen(opt)+1, sizeof(char));
        strcpy(cm->options[i], opt);
    }

    va_end(args);
}

void OG_OpenContextMenuV2(void (*_callback)(int, void*), void *param, char *hint, int optQ, char **opts){
    OG_SetHeader(this, hint);
    this->noTitleBar = hint == NULL;
    
    ContextMenu *cm = OpenContextMenuHelper(_callback, param, optQ);
    for (int i = 0; i < optQ; i++) {
        char *opt = opts[i];
        cm->options[i] = calloc(strlen(opt)+1, sizeof(char));
        strcpy(cm->options[i], opt);
    }
}

void OG_OpenContextMenuV3(void (*_callback)(char*, void*), void *param, char *hint, int optQ, char **opts){
    OG_SetHeader(this, hint);
    this->noTitleBar = hint == NULL;
    
    // discard empty options
    int Q=0;
    for (int i = 0; i < optQ; i++) {
        char *opt = opts[i];
        if (opt && strcmp(opt, "") != 0)
            Q++;
    }
    
    if (Q <= 0) return;

    ContextMenu *cm = OpenContextMenuHelper(NULL, param, Q);
    cm->callbackStr = _callback;
    cm->callback = NULL;

    int ii=0;
    for (int i = 0; i < optQ; i++) {
        char *opt = opts[i];
        if (opt && strcmp(opt, "") != 0){
            cm->options[ii] = calloc(strlen(opt)+1, sizeof(char));
            strcpy(cm->options[ii++], opt);
        }
    }
}