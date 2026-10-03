#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "microui.h"
#include "origami.h"

static const char *commands[] = {
    NULL
};

static OG_Viewport *this;
static char buf[256];
static mu_Id input_id;
static void (*Ok)(char*);

static void Init(OG_Viewport *v){
    this = v;
}

static void Update(OG_Viewport *v){
    if (v->hidden) return;
    if (OG.viewports.tail != v) return;
    
    v->ctx.focus = input_id;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        if (!OG_MouseInViewport(v, false, false, false)){
            OG_CloseViewportByName("InputDialog");
        }
    }
}

static void UI(OG_Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 1, (const int[]){-1}, 15);

    // manual textbox
    input_id = mu_get_id(ctx, "BITCH", 5);
    mu_Rect r = mu_layout_next(ctx);
    if (mu_textbox_raw(ctx, buf, 256, input_id, r, 0) & MU_RES_SUBMIT){
        Ok(buf);
        OG_ToggleViewport(v);
    }

}

static const char **GetCmds(){
    return commands;
}

void OG_InputDialog(){
    OG_Viewport *v = OG_InitViewport(
        "InputDialog", 
        (Rectangle){0,0,250, 25}, 
        1.0f, 1.0f, 
        true, false, true, 
        &Init, 
        &Update, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        &UI,
        &GetCmds, 
        NULL
    );

    v->noTitleBar = true;
    v->updateAlways = true;
}

void OG_OpenInputDialog(void (*ok_callback)(char*), char *hint){
    OG_SetHeader(this, hint);
    this->noTitleBar = hint == NULL;
    
    memset(buf, 0, 256);
    Ok = ok_callback;
    Vector2 mousePos = GetMousePosition();
    this->pos = mousePos;
    OG_ToggleViewportByName("InputDialog");
}