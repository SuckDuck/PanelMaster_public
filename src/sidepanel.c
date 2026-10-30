// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include "cJSON.h"
#include "loader.h"
#include "main.h"
#include "microui.h"
#include "origami.h"
#include "surfaces.h"
#include "utils.h"
#include "workspace.h"
#include <raymath.h>
#include <string.h>

#define ANCHOR_SNAP_DISTANCE 15.0f
#define EXTENSIBLES_INITIAL_OFFSET 30.0f

#define COMPONENTS_INITIAL_OFFSET 25.0f
#define COMPONENTS_FONT_SIZE 16.0f
#define COMPONENTS_INDENT_W 16.0f
#define COMPONENTS_PREVIEW_SIZE 50.0f
#define COMPONENTS_PREVIEW_MARGIN 10.0f
#define SCROLL_SPEED 12

extern void Solver();

typedef enum State{
    IDLE_STATE,
    DRAGGING_COMPONENT_STATE
} State;

static State currentState;
static int activeTab = 0;
static Tab tabs[] = {
    {"Inspector", 85},
    {"Components", 90},
    {"Extensibles", 90}
};

static OG_Viewport *workspace;
static cJSON *targetComponentNode;
static cJSON *selectedExtensible;
static int extensibleCreateAnchors = 0;

/* <== Utils ===================================================> */
static void RenderNoComponentsLabel(OG_Viewport *v){
    const char *noComponents = "No component library\n  currently loaded";
    Vector2 textSize = MeasureTextEx(
        OG.defaultFont, 
        noComponents, 
        16.0f, 
        2.0f
    );
    
    DrawTextEx(
        OG.defaultFont, 
        noComponents, 
        (Vector2){
            v->size.width/2.0f - textSize.x/2, 
            v->size.height/2.0f *-1 - textSize.y
        }, 
        16.0f,
        2.0f,
        WHITE
    );
}

static void RenderNoProjectCurrentlyLoaded(OG_Viewport *v){
    const char *noProject = "No project currently open";
    Vector2 textSize = MeasureTextEx(
        OG.defaultFont,
        noProject,
        16.0f,
        2.0f
    );

    DrawTextEx(
        OG.defaultFont,
        noProject,
        (Vector2){
            v->size.width/2.0f - textSize.x/2,
            v->size.height/2.0f *-1 - textSize.y
        },
        16.0f,
        2.0f,
        WHITE
    );
}

static void muLayout_1NumberWLabel_2Button(mu_Context *ctx, char *label){
    // implmentation taken from mu_equal_layout_l_auto

    mu_Container *cnt = mu_get_current_container(ctx);
    int spacing = ctx->style->spacing;
    int padding = ctx->style->padding;

    int inner_w = cnt->body.w - 2 * padding;
    int half_w = (inner_w - spacing) / 2;

    int space_w = ctx->text_width(ctx->style->font, " ", 1);
    int label_w = ctx->text_width(ctx->style->font, label, 2) + space_w;

    int textbox_w = half_w - label_w - spacing;
    int button_w = (half_w - spacing) / 2;

    int widths[4] = {
        label_w,
        textbox_w,
        button_w,
        half_w - spacing - button_w
    };

    mu_layout_row(ctx, 4, widths, 0);

}

/* <== Inspector Tab ===========================================> */

static void ChangeAlignContextMenu(int opt, void *param){
    activeTree->surfaceSelection[0]->anchorAlign = opt;
}

static void GeneralPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "General Properties", MU_OPT_EXPANDED)) return;
    
    mu_equal_layout_l_auto(ctx, (const char *[]){"GridStep:", 0}, 0, NULL, NULL);
    mu_label(ctx, "GridStep:");
    mu_number_ex(ctx, &inspectorSettings.gridSpacing, 0, "%.2fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "GridOn:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_GridOn", 16);
    mu_checkbox(ctx, "", &inspectorSettings.enableGrid);
    mu_pop_id(ctx);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "EnableAnchors:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_EnableAnchors", 23);
    mu_checkbox(ctx, "", &inspectorSettings.enableAnchors);
    mu_pop_id(ctx);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "EnableAnnotations:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_EnableAnnotations", 27);
    mu_checkbox(ctx, "", &inspectorSettings.enableAnnotations);
    mu_pop_id(ctx);
    
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "DrawSkins:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_DrawSkins", 19);
    mu_checkbox(ctx, "", &inspectorSettings.drawSkins);
    mu_pop_id(ctx);
    
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "DrawCutOuts:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_DrawCutOuts", 21);
    mu_checkbox(ctx, "", &inspectorSettings.drawCutOuts);
    mu_pop_id(ctx);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "DrawLabels:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_DrawLabels", 20);
    mu_checkbox(ctx, "", &inspectorSettings.drawLabels);
    mu_pop_id(ctx);
    
    mu_layout_row(ctx, 2, (const int[]){70, -1}, 0);

}

static void AnchorPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Anchor", MU_OPT_EXPANDED)) return;
    
    // Value
    {
        mu_Container *cnt = mu_get_current_container(ctx);
        int spacing = ctx->style->spacing;
        int padding = ctx->style->padding;

        int inner_w = cnt->body.w - 2 * padding;
        int half_w = (inner_w - spacing) / 2;

        int label_w = ctx->text_width(ctx->style->font, "Val:", 2)
            + ctx->text_width(ctx->style->font, " ", 1);

        int textbox_w = half_w - label_w - spacing;

        int widths[3] = {
            label_w,
            textbox_w,
            half_w
        };

        mu_layout_row(ctx, 3, widths, 0);
    }

    mu_label(ctx, "Val:");
    mu_number_custom(
        ctx, 
        activeTree ? (activeTree->anchorSelection[0] ? &activeTree->anchorSelection[0]->val : NULL):NULL, 
        0, "%.2fmm", 
        (
            MU_OPT_KEYBOARD_ONLY | 
            MU_OPT_ALIGNCENTER | 
            (CanAnchorValBeChanged(activeTree ? activeTree->anchorSelection[0]:NULL) ? 0:MU_OPT_NOINTERACT)
        )
    );
    
    
    // Rotate BTN
    if (!activeTree || !activeTree->anchorSelection[0])
        mu_button(ctx, "N/A");

    else if (mu_button(ctx, activeTree->anchorSelection[0]->type == ANCHOR_HORIZONTAL ? "[H] V ":" H [V]")){
        if (!CanAnchorValBeChanged(activeTree->anchorSelection[0]))
            OG_PushLog("Can't rotate a fixed anchor!");
        else if (IsAnchorReferencedByAnchor(activeTree->anchorSelection[0], treeList, treeListQ))
            OG_PushLog("Can't rotate anchor: it is referenced by another anchor");
        else if (IsAnchorReferencedBySurface(activeTree->anchorSelection[0]))
            OG_PushLog("Can't rotate anchor: it is referenced by a surface");
        else if (activeTree->anchorSelection[0]->constraint != ANCHOR_CONSTRAINT_NONE)
            OG_PushLog("Can't rotate anchor: it is constrained");

        else activeTree->anchorSelection[0]->type = activeTree->anchorSelection[0]->type == ANCHOR_HORIZONTAL ? ANCHOR_VERTICAL:ANCHOR_HORIZONTAL;
    }

    // Lock checkbox
    int locked = (int) IsAnchorLocked(activeTree ? activeTree->anchorSelection[0]:NULL);
    
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "Locked:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, &locked, sizeof(locked));
    if (mu_checkbox(ctx, "", &locked))
        AnchorToggleLock(activeTree ? activeTree->anchorSelection[0]:NULL);
    mu_pop_id(ctx);
        
}

static void GenericPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    Surface *s = NULL;
    if (activeTree && activeTree->surfaceSelection[0])
        s = activeTree->surfaceSelection[0];
    

    if (!(s && (s->opts & SURFACE_OPT_GENERIC)))
        return;

    mu_Container *cnt = mu_get_current_container(ctx);
	int padding = ctx->style->padding;
	int inner_w = cnt->body.w - 2 * padding;

    mu_push_id(ctx, "generic", 7);
    mu_layout_row(ctx, 2, (int[]){inner_w/2, -1}, 0);
    mu_label(ctx, "Boundaries:");
    mu_label(ctx, "Cutout:");

    // Boundaries Width
    mu_equal_layout_l_auto(ctx, (const char *[]){"W:", "W:", 0}, 0, NULL, NULL);
    mu_label(ctx, "W:");
    mu_number_custom( ctx, s ? &s->size.x:NULL, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );
    
    // Cutout Width
    mu_label(ctx, "W:");
    mu_number_custom( ctx, &s->cutout.rect.x, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );
    
    // Boundaries Height
    mu_label(ctx, "H:");
    mu_number_custom( ctx, s ? &s->size.y:NULL, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );
    
    // Cutout Height
    mu_label(ctx, "H:");
    mu_number_custom( ctx, &s->cutout.rect.y, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );

    mu_layout_row(ctx, 2, (int[]){inner_w/2, -1}, 0);
    mu_label(ctx, "Color:");
    mu_label(ctx, "Offset:");

    mu_equal_layout_l_auto(ctx, (const char *[]){"W:", "W:", 0}, 0, NULL, NULL);

    // Red Color
    mu_label(ctx, "R:");
    mu_Real r = s->skin.tint.r;
    mu_slider(ctx, &r, 0, 255);
    s->skin.tint.r = (unsigned char)r;

    // Offset X
    mu_label(ctx, "X:");
    mu_number_custom( ctx, &s->origin.x, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );

    // Green Color
    mu_label(ctx, "G:");
    mu_Real g = s->skin.tint.g;
    mu_slider(ctx, &g, 0, 255);
    s->skin.tint.g = (unsigned char)g;

    // Offset Y
    mu_label(ctx, "Y:");
    mu_number_custom( ctx, &s->origin.y, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER );

    // Blue Color
    mu_label(ctx, "B:");
    mu_Real b = s->skin.tint.b;
    mu_slider(ctx, &b, 0, 255);
    s->skin.tint.b = (unsigned char)b;
    mu_pop_id(ctx);

}

static void SurfacePropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Surface Properties", MU_OPT_EXPANDED)) return;

    Surface *s = NULL;
    if (activeTree && activeTree->surfaceSelection[0])
        s = activeTree->surfaceSelection[0];

    // Local position
    mu_equal_layout_l_auto(ctx, (const char *[]){" X:", " Y:", 0}, 0, NULL, NULL);
    mu_label(ctx, "X:");
    mu_number_custom(
        ctx, s ? &s->pos.x:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | (s == &activeTree->baseSurface ? MU_OPT_NOINTERACT:0)
    );

    mu_label(ctx, "Y:");
    mu_number_custom(
        ctx, s ? &s->pos.y:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | (s == &activeTree->baseSurface ? MU_OPT_NOINTERACT:0)
    );

    // Global position
    mu_equal_layout_l_auto(ctx, (const char *[]){"GX:", "GY:", 0}, 0, NULL, NULL);
    mu_label(ctx, "GX:");
    Vector2 globalPos = s ? s->globalPos:Vector2Zero();
    mu_number_custom(
        ctx, s ? &globalPos.x:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
    );

    mu_label(ctx, "GY:");
    mu_number_custom(
        ctx, s ? &globalPos.y:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
    );
    
    // Size
    if (s && !IsSurfaceBase(s) && s->type == SURFACE_COMPONENT && !(s->opts & SURFACE_OPT_GENERIC)){
        mu_Container *cnt = mu_get_current_container(ctx);
        int padding = ctx->style->padding;
        int inner_w = (cnt->body.w - 2 * padding)/3 * 2;

        mu_equal_layout_l_auto(ctx, (const char *[]){" W:", "H:", 0}, 0, &inner_w, (int[]){-1, 0});
        mu_label(ctx, "W:");
        mu_number_custom(
            ctx, s ? &s->size.x:NULL, 0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );

        mu_label(ctx, "H:");
        mu_number_custom(
            ctx, s ? &s->size.y:NULL, 0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );

        if (mu_button(ctx, "Reset") && activeTree && activeTree->surfaceSelection[0]){
            Surface *s = activeTree->surfaceSelection[0];
            s->size = s->ogSize;
        }
    }

    else {
        mu_equal_layout_l_auto(ctx, (const char *[]){" W:", " H:", 0}, 0, NULL, NULL);
        mu_label(ctx, "W:");
        mu_number_custom( ctx, s ? &s->size.x:NULL, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT );
        mu_label(ctx, "H:");
        mu_number_custom( ctx, s ? &s->size.y:NULL, 0, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT );
    }

    // Rotation
    muLayout_1NumberWLabel_2Button(ctx, " A:");
    mu_label(ctx, "A:");
    mu_number_custom(
        ctx, activeTree ? (activeTree->surfaceSelection[0] ? &activeTree->surfaceSelection[0]->rotation : NULL):NULL, 
        0, "%.2f°", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
    );

    if (mu_button(ctx,s ? ( s->type == SURFACE_COMPONENT ? "-90°":"-180°"):"-90°") && s){
        if (s != &activeTree->baseSurface){
            s->rotation -= s->type == SURFACE_COMPONENT ? 90:180;
            if (s->rotation < 0)
                s->rotation += 360;
        }

        else OG_PushLog("Base surface can't be rotated");
    }

    if (mu_button(ctx, s ? (s->type == SURFACE_COMPONENT ?  "+90°":"+180°"):"+90°") && s){
        if (s != &activeTree->baseSurface){
            s->rotation += s->type == SURFACE_COMPONENT ? 90:180;
            if (s->rotation >= 360)
                s->rotation -= 360;
        }

        else OG_PushLog("Base surface can't be rotated");
    }

    if (s){
        mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);

        // X fix checkbox
        int xFixed = s->opts & SURFACE_OPT_FIXED_X;
        mu_button_ex(ctx, "Fixed Local X:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
        mu_push_id(ctx, &xFixed, sizeof(xFixed));
        if (mu_checkbox(ctx, "", &xFixed))
            s->opts ^= SURFACE_OPT_FIXED_X;
        mu_pop_id(ctx);

        // Y fix checkbox
        int yFixed = s->opts & SURFACE_OPT_FIXED_Y;
        mu_button_ex(ctx, "Fixed Local Y:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
        mu_push_id(ctx, &yFixed, sizeof(yFixed));
        if (mu_checkbox(ctx, "", &yFixed))
            s->opts ^= SURFACE_OPT_FIXED_Y;
        mu_pop_id(ctx);

        // Show/Hide checkbox
        int hidden = s->opts & SURFACE_OPT_HIDDEN;
        mu_button_ex(ctx, "Hidden:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
        mu_push_id(ctx, &hidden, sizeof(hidden));
        if (mu_checkbox(ctx, "", &hidden))
            ShowHideSurface(s);
        mu_pop_id(ctx);

    }
    
    if (s && !IsSurfaceBase(s)) {
        mu_equal_layout_l_auto(ctx, (const char *[]){"Label:", 0}, 0, NULL, NULL);

        mu_label(ctx, "Label:");
        mu_textbox(ctx, s->skin.label.text, 64);
    
        mu_equal_layout_l_auto(ctx, (const char *[]){"LX:", "LY:", 0}, 0, NULL, NULL);

        mu_label(ctx, "LX:");
        mu_number_custom(
            ctx, s ? &s->skin.label.offset.x:NULL, 0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | (s == &activeTree->baseSurface ? MU_OPT_NOINTERACT:0)
        );

        mu_label(ctx, "LY:");
        mu_number_custom(
            ctx, s ? &s->skin.label.offset.y:NULL, 0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | (s == &activeTree->baseSurface ? MU_OPT_NOINTERACT:0)
        );

        muLayout_1NumberWLabel_2Button(ctx, "LA:");

        mu_label(ctx, "LA:");
        mu_number_custom(
            ctx, s ? &s->skin.label.rotation:NULL, 0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
        );

        if (mu_button(ctx, "-90") && s) {
            s->skin.label.rotation -= 90;
            if (s->skin.label.rotation < 0)
                s->skin.label.rotation += 360;
        }

        if (mu_button(ctx, "+90") && s){
            s->skin.label.rotation += 90;
            if (s->skin.label.rotation >= 360)
                s->skin.label.rotation -= 360;
        }
    }

    GenericPropertiesSection(v, ctx);
    
    // Anchor Setup SubSection
    if (s){
        mu_vertical_space(ctx, 5);
        mu_layout_row(ctx, 1, (const int[]){-1}, 15);
        mu_label(ctx, "Anchor Setup:");
        
        mu_layout_row(ctx, 1, (const int[]){-1}, 95);
        mu_begin_panel(ctx, "AnchorSetup");
        
        switch (s->type){
            case SURFACE_EXTENSIBLE:{
                mu_layout_row(ctx, 2, (const int[]){90, -1}, 0);
                mu_label(ctx, "AxisAlign:");
                const char *align = GetAnchorAlignStr(s);
                if (mu_button(ctx, align))
                    OG_OpenContextMenu(&ChangeAlignContextMenu, NULL, NULL, 3, "CENTER", "MIN", "MAX");
                
                mu_Container *container = mu_get_current_container(ctx);
                int label_width = 40;
                int spacing = ctx->style->spacing;
                int padding = ctx->style->padding;

                int available = container->body.w - padding * 2;
                int item_width = (available - label_width - spacing * 3) / 4;

                mu_layout_row(ctx, 5, (int[]){label_width, item_width, item_width, item_width, -1 }, 0);

                mu_label(ctx, "LCut:");
                mu_push_id(ctx, "LCUT_0", 6); if (mu_button(ctx, "0°")) s->leftCut = 0.0; mu_pop_id(ctx);
                mu_push_id(ctx, "LCUT_45", 7); if (mu_button(ctx, "45°")) s->leftCut = s->size.y; mu_pop_id(ctx);
                mu_push_id(ctx, "LCUT_m45", 8); if (mu_button(ctx, "-45°")) s->leftCut = -s->size.y; mu_pop_id(ctx);
                mu_number_custom(ctx, &s->leftCut, 0, "%.1fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER);

                mu_label(ctx, "RCut:");
                mu_push_id(ctx, "RCUT_0", 6); if (mu_button(ctx, "0°")) s->rightCut = 0.0; mu_pop_id(ctx);
                mu_push_id(ctx, "RCUT_45", 7); if (mu_button(ctx, "45°")) s->rightCut = s->size.y; mu_pop_id(ctx);
                mu_push_id(ctx, "RCUT_m45", 8); if (mu_button(ctx, "-45°")) s->rightCut = -s->size.y; mu_pop_id(ctx);
                mu_number_custom(ctx, &s->rightCut, 0, "%.1fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER);
            
                mu_vertical_space(ctx, 5);
                
                mu_layout_row(ctx, 2, (const int[]){50, -1}, 10);
                mu_label(ctx, "Axis:");
                mu_label(ctx, s->anchors[0] ? s->anchors[0]->name:"N/A");

                mu_label(ctx, "Start:");
                mu_label(ctx, s->anchors[1] ? s->anchors[1]->name:"N/A");

                mu_label(ctx, "End:");
                mu_label(ctx, s->anchors[2] ? s->anchors[2]->name:"N/A");    
                break;
            }

            case SURFACE_COMPONENT:{
                mu_layout_row(ctx, 1, (const int[]){-1}, 10);
                mu_label(ctx, "Fixed Global X to:");
                mu_label(ctx, s->anchors[0] ? s->anchors[0]->name:"N/A");
                mu_vertical_space(ctx, 10);

                mu_label(ctx, "Fixed Global Y to:");
                mu_label(ctx, s->anchors[1] ? s->anchors[1]->name:"N/A");
                break;
            }
        }
        
        mu_end_panel(ctx);
    }

}

void Inspector(OG_Viewport *v, mu_Context *ctx){
    GeneralPropertiesSection(v, ctx);
    AnchorPropertiesSection(v, ctx);
    SurfacePropertiesSection(v, ctx);
}

/* <== Components StateMachine =====================================> */

static Vector2 UpdateComponentsNode(OG_Viewport *v, cJSON *node, Vector2 pos, void *data){
    Vector2 mousePosition = OG_GetMouseOverlayPosition(v);

    if (CheckCollisionPointRec(mousePosition, (Rectangle){pos.x, pos.y, v->size.width, COMPONENTS_PREVIEW_SIZE})){
        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            targetComponentNode = node;
            currentState = DRAGGING_COMPONENT_STATE;
        }
    }

    return (Vector2){
        0, COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN
    };
}

static Vector2 UpdateComponentsFolder(OG_Viewport *v, cJSON *node, Vector2 pos, bool expanded, void *data){
    Vector2 mousePosition = OG_GetMouseOverlayPosition(v);

    if (CheckCollisionPointRec(mousePosition, (Rectangle){pos.x, pos.y, v->size.width, COMPONENTS_FONT_SIZE})){
        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            cJSON *expanded_ = cJSON_GetObjectItem(node, "expanded");
            cJSON_SetIntValue(expanded_, 1-expanded_->valueint);
        }
    }
    
    return (Vector2){
        COMPONENTS_INDENT_W,
        COMPONENTS_FONT_SIZE
    };
}

static void IdleState(OG_Viewport *v){
    tabs[1].contentSize = ProcessComponentsLib(
        v, componentsLib.tree, 
        (Vector2){
            0.0f, COMPONENTS_INITIAL_OFFSET - tabs[1].scroll
        }, 
        &UpdateComponentsFolder,
        &UpdateComponentsNode,
        NULL
    );
}

static void DraggingComponentState(OG_Viewport *v){
    OG.nextCursor = MOUSE_CURSOR_CROSSHAIR;
    Vector2 workspaceMousePos = OG_GetMouseViewportPosition(workspace);
    Surface *s = GetSurfaceAt(workspaceMousePos, activeTree);
    if (s) SelectSurface(s, false, false);
    else activeTree->surfaceSelection[0] = activeTree->surfaceSelection[1] = NULL;
    

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)){
        if (s){
            
            char cwd[OG_PATH_MAX];
            sprintf(cwd, "%s", GetWorkingDirectory());
            
            char surfacePath[OG_PATH_MAX];
            sprintf(surfacePath, "%s%s/data/%s", GetApplicationDirectory(), ROOT, cJSON_GetObjectItem(targetComponentNode, "path")->valuestring);
            ChangeDirectory(GetDirectoryPath(surfacePath));
            LoadSurface(GetFileName(surfacePath), NULL);
            ChangeDirectory(cwd);

            if (s == &activeTree->baseSurface || s->type == SURFACE_EXTENSIBLE){
                Vector2 sOgPos = activeTree->lastInsertedSurface->pos;
                SetSurfaceGlobalPos(activeTree->lastInsertedSurface, workspaceMousePos);
                if (activeTree->lastInsertedSurface->opts & SURFACE_OPT_FIXED_X) activeTree->lastInsertedSurface->pos.x = sOgPos.x;
                if (activeTree->lastInsertedSurface->opts & SURFACE_OPT_FIXED_Y) activeTree->lastInsertedSurface->pos.y = sOgPos.y;
            }
            
        }

        currentState = IDLE_STATE;
    }
}

/* <== Components Tab ==========================================> */

static void ComponentsUI(OG_Viewport *v, mu_Context *ctx){
}

static Vector2 RenderComponentsNode(OG_Viewport *v, cJSON *node, Vector2 pos, void *data){
    cJSON *preview = cJSON_GetObjectItem(node, "preview");
    DrawRectangleLines(
        COMPONENTS_PREVIEW_MARGIN + pos.x ,
        pos.y,
        COMPONENTS_PREVIEW_SIZE,
        COMPONENTS_PREVIEW_SIZE,
        (Color){
            v->ctx.style->colors[MU_COLOR_BORDER].r,
            v->ctx.style->colors[MU_COLOR_BORDER].g,
            v->ctx.style->colors[MU_COLOR_BORDER].b,
            v->ctx.style->colors[MU_COLOR_BORDER].a
        }
    );
    
    DrawTextureTile(
        componentsLib.atlas[cJSON_GetArrayItem(preview, 0)->valueint], 
        cJSON_GetArrayItem(preview, 1)->valueint, 
        cJSON_GetArrayItem(preview, 2)->valueint, 
        pos.x + COMPONENTS_PREVIEW_MARGIN, 
        pos.y,
        componentsLib.atlas[cJSON_GetArrayItem(preview, 0)->valueint].width/COMPONENTS_PREVIEW_SIZE
    );

    cJSON *name = cJSON_GetObjectItem(node, "name");
    DrawTextEx(
        OG.defaultFont,
        name->valuestring, 
        (Vector2){pos.x + COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN*2, pos.y}, 
        COMPONENTS_FONT_SIZE, 
        2.0f, 
        WHITE
    );

    return (Vector2){
        0, COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN
    };
}

static Vector2 RenderComponentsFolder(OG_Viewport *v, cJSON *node, Vector2 pos, bool expanded, void *data){
    cJSON *name = cJSON_GetObjectItem(node, "name");
    char nameStr[64];
    sprintf(nameStr, "%s %s", name->valuestring, expanded ? "-":"+");

    DrawTextEx(
        OG.defaultFont,
        nameStr, 
        pos, 
        COMPONENTS_FONT_SIZE, 
        2.0f, 
        WHITE
    );
    
    return (Vector2){
        COMPONENTS_INDENT_W,
        COMPONENTS_FONT_SIZE
    };
    
}

static void ComponentsRenderOverlay(OG_Viewport *v){
    if (!componentsLib.tree){
        RenderNoComponentsLabel(v);
        return;
    }

    if (activeTree == NULL){
        RenderNoProjectCurrentlyLoaded(v);
        return;
    }

    ProcessComponentsLib(
        v, componentsLib.tree, 
        (Vector2){
            0.0f, COMPONENTS_INITIAL_OFFSET - tabs[1].scroll
        }, 
        &RenderComponentsFolder,
        &RenderComponentsNode,
        NULL
    );
}

static void ComponentsUpdate(OG_Viewport *v){
    if (!componentsLib.tree) return;
    if (activeTree == NULL) return;

    // calcscroll    
    float maxScroll = fmaxf(0.0f, tabs[1].contentSize+COMPONENTS_INITIAL_OFFSET - (v->size.height*-1));
    tabs[1].scroll -= OG_GetMouseWheelMove(v) * SCROLL_SPEED;
    tabs[1].scroll = fminf(tabs[1].scroll, maxScroll);
    tabs[1].scroll = fmaxf(tabs[1].scroll, 0.0f);

    switch (currentState){
        case IDLE_STATE:               IdleState(v);              break;
        case DRAGGING_COMPONENT_STATE: DraggingComponentState(v); break;
    }

}

/* <== Extensibles Tab =========================================> */

static void ExtensbilesUI(OG_Viewport *v, mu_Context *ctx){
    if (!componentsLib.tree) return;
    if (activeTree == NULL) return;
        
    mu_layout_row(ctx, 1, (const int[]){-1}, 0);
    mu_checkbox(ctx, "Create Anchors", &extensibleCreateAnchors);

}

static Vector2 RenderExtensibleNode(OG_Viewport *v, cJSON *node, Vector2 pos, void *data){
    cJSON *preview = cJSON_GetObjectItem(node, "preview");

    float height = COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN;
    float width = v->size.width - pos.x - COMPONENTS_PREVIEW_MARGIN;

    // Preview border
    DrawRectangleLines(
        COMPONENTS_PREVIEW_MARGIN + pos.x,
        pos.y,
        COMPONENTS_PREVIEW_SIZE,
        COMPONENTS_PREVIEW_SIZE,
        (Color){
            v->ctx.style->colors[MU_COLOR_BORDER].r,
            v->ctx.style->colors[MU_COLOR_BORDER].g,
            v->ctx.style->colors[MU_COLOR_BORDER].b,
            v->ctx.style->colors[MU_COLOR_BORDER].a
        }
    );
    
    DrawTextureTile(
        componentsLib.atlas[cJSON_GetArrayItem(preview, 0)->valueint], 
        cJSON_GetArrayItem(preview, 1)->valueint, 
        cJSON_GetArrayItem(preview, 2)->valueint, 
        pos.x + COMPONENTS_PREVIEW_MARGIN, 
        pos.y,
        componentsLib.atlas[cJSON_GetArrayItem(preview, 0)->valueint].width / COMPONENTS_PREVIEW_SIZE
    );

    cJSON *name = cJSON_GetObjectItem(node, "name");

    DrawTextEx(
        OG.defaultFont,
        name->valuestring, 
        (Vector2){
            pos.x + COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN * 2,
            pos.y
        }, 
        COMPONENTS_FONT_SIZE, 
        2.0f, 
        WHITE
    );

    // Component border
    if (node == selectedExtensible){
        DrawRectangleLines(
            pos.x + COMPONENTS_PREVIEW_MARGIN,
            pos.y,
            width - COMPONENTS_PREVIEW_MARGIN,
            height,
            OG_VIEWPORT_TITLE_C
        );
    }

    return (Vector2){
        0,
        height
    };
}

static void ExtensiblesRenderOverlay(OG_Viewport *v){
    if (!componentsLib.tree){
        RenderNoComponentsLabel(v);
        return;
    }

    if (activeTree == NULL){
        RenderNoProjectCurrentlyLoaded(v);
        return;
    }

    ProcessComponentsLib(
        v, componentsLib.extensibles, 
        (Vector2){
            0.0f, EXTENSIBLES_INITIAL_OFFSET - tabs[2].scroll
        }, 
        NULL,
        &RenderExtensibleNode,
        NULL
    );

    DrawRectangle(0, 0, v->size.width, EXTENSIBLES_INITIAL_OFFSET-1, OG_VIEWPORT_BG_C);
    DrawRectangleLines(
        0, 0, 
        v->size.width, 
        EXTENSIBLES_INITIAL_OFFSET-1, 
        (Color){
            v->ctx.style->colors[MU_COLOR_BORDER].r,
            v->ctx.style->colors[MU_COLOR_BORDER].g,
            v->ctx.style->colors[MU_COLOR_BORDER].b,
            v->ctx.style->colors[MU_COLOR_BORDER].a
        }
    );
}

static Vector2 UpdateExtensibleNode(OG_Viewport *v, cJSON *node, Vector2 pos, void *data){
    Vector2 mousePosition = OG_GetMouseOverlayPosition(v);

    if (CheckCollisionPointRec(mousePosition, (Rectangle){pos.x, pos.y, v->size.width, COMPONENTS_PREVIEW_SIZE})){
        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (selectedExtensible == node){
                selectedExtensible = NULL;
                WorkspaceCancelDrawingExtensible();
            }

            else{
                selectedExtensible = node;
                WorkspaceStartDrawingExtensible();    
            }
        }
    }

    return (Vector2){
        0, COMPONENTS_PREVIEW_SIZE + COMPONENTS_PREVIEW_MARGIN
    };
}

static void ExtensiblesUpdate(OG_Viewport *v){
    if (!componentsLib.tree) return;
    if (activeTree == NULL) return;

    // calcscroll
    tabs[2].contentSize = ProcessComponentsLib(
        v, componentsLib.extensibles, 
        (Vector2){
            0.0f, EXTENSIBLES_INITIAL_OFFSET - tabs[2].scroll
        }, 
        NULL,
        &UpdateExtensibleNode,
        NULL
    );

    float maxScroll = fmaxf(0.0f, tabs[2].contentSize+EXTENSIBLES_INITIAL_OFFSET - (v->size.height*-1));
    tabs[2].scroll -= OG_GetMouseWheelMove(v) * SCROLL_SPEED;
    tabs[2].scroll = fminf(tabs[2].scroll, maxScroll);
    tabs[2].scroll = fmaxf(tabs[2].scroll, 0.0f);
    
}

void SidePanelDrawExtensible(Vector2 p1, Vector2 p2){
    Vector2 pp1 = GetWorldToScreen2D(p1, workspace->camera);
    Vector2 pp2 = GetWorldToScreen2D(p2, workspace->camera);
    
    // Straight line
    if (p1.x == p2.x || p1.y == p2.y){
        Anchor *axis = NULL;
        Anchor *start = NULL;
        Anchor *end = NULL;
    
        // vertical
        if (p1.x == p2.x){
            for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
                LinePoints lp = GetAnchorScreenPoints(workspace, a);
                if (a->type == ANCHOR_HORIZONTAL){
                    if (fabsf(pp1.y - lp.A.y) <= ANCHOR_SNAP_DISTANCE) start = a;
                    if (fabsf(pp2.y - lp.A.y) <= ANCHOR_SNAP_DISTANCE) end = a;
                }

                else if (fabsf(pp1.x - lp.A.x) <= ANCHOR_SNAP_DISTANCE) axis = a;
            }

            if (extensibleCreateAnchors && !start){
                start = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_HORIZONTAL, 
                    p1.y
                );
            }

            if (extensibleCreateAnchors && !end){
                end = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_HORIZONTAL, 
                    p2.y
                );
            }


            if (extensibleCreateAnchors && !axis){
                axis = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_VERTICAL, 
                    p1.x
                );
            }
        }

        // horizontal
        else if (p1.y == p2.y) {
            for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
                LinePoints lp = GetAnchorScreenPoints(workspace, a);
                if (a->type == ANCHOR_VERTICAL){
                    if (fabsf(pp1.x - lp.A.x) <= ANCHOR_SNAP_DISTANCE) start = a;
                    if (fabsf(pp2.x - lp.A.x) <= ANCHOR_SNAP_DISTANCE) end = a;
                }

                else if (fabsf(pp1.y - lp.A.y) <= ANCHOR_SNAP_DISTANCE) axis = a;
            }

            if (extensibleCreateAnchors && !start){
                start = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_VERTICAL, 
                    p1.x
                );
            }

            if (extensibleCreateAnchors && !end){
                end = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_VERTICAL, 
                    p2.x
                );
            }

            if (extensibleCreateAnchors && !axis){
                axis = AddAnchor(
                    activeTree, 
                    ResolveAnchorName("NewAnchor", activeTree), 
                    ANCHOR_HORIZONTAL, 
                    p1.y
                );
            }
        }

        if (axis && start && end){
            SelectSurface(&activeTree->baseSurface, false, false);
            SelectAnchor(axis, false, false);
            SelectAnchor(start, true, false);
            SelectAnchor(end, true, false);

            char cwd[OG_PATH_MAX];
            sprintf(cwd, "%s", GetWorkingDirectory());
            
            char surfacePath[OG_PATH_MAX];
            sprintf(surfacePath, "%s%s/data/%s", GetApplicationDirectory(), ROOT, cJSON_GetObjectItem(selectedExtensible, "path")->valuestring);
            
            ChangeDirectory(GetDirectoryPath(surfacePath));
            LoadSurface(GetFileName(surfacePath), NULL);
            ChangeDirectory(cwd);
        }

        else {
            if (!axis) OG_PushLog("%s", "Failed to find axis anchor");
            if (!start) OG_PushLog("%s", "Failed to find start anchor");
            if (!end) OG_PushLog("%s", "Failed to find end anchor");
        }
    }

    // Frame
    else {
        Anchor *a0 = NULL; // Left
        Anchor *a1 = NULL; // Right
        Anchor *a2 = NULL; // Top
        Anchor *a3 = NULL; // Bottom

        for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
            LinePoints lp = GetAnchorScreenPoints(workspace, a);

            if (a->type == ANCHOR_VERTICAL){
                if (fabsf(pp1.x - lp.A.x) <= ANCHOR_SNAP_DISTANCE) a0 = a;
                if (fabsf(pp2.x - lp.A.x) <= ANCHOR_SNAP_DISTANCE) a1 = a;
            }

            else if (a->type == ANCHOR_HORIZONTAL){
                if (fabsf(pp1.y - lp.A.y) <= ANCHOR_SNAP_DISTANCE) a2 = a;
                if (fabsf(pp2.y - lp.A.y) <= ANCHOR_SNAP_DISTANCE) a3 = a;
            }
        }

        // left
        if (extensibleCreateAnchors && !a0){
            a0 = AddAnchor(
                activeTree, 
                ResolveAnchorName("NewAnchor", activeTree), 
                ANCHOR_VERTICAL, 
                p1.x
            );

            SetAnchorAsOffset(a0, GetAnchorByName(activeTree, "LeftMost"));
        }

        // right
        if (extensibleCreateAnchors && !a1){
            a1 = AddAnchor(
                activeTree, 
                ResolveAnchorName("NewAnchor", activeTree), 
                ANCHOR_VERTICAL, 
                p2.x
            );

            SetAnchorAsOffset(a1, GetAnchorByName(activeTree, "RightMost"));
        }

        // top
        if (extensibleCreateAnchors && !a2){
            a2 = AddAnchor(
                activeTree, 
                ResolveAnchorName("NewAnchor", activeTree), 
                ANCHOR_HORIZONTAL, 
                p1.y
            );

            SetAnchorAsOffset(a2, GetAnchorByName(activeTree, "TopMost"));
        }

        // bottom
        if (extensibleCreateAnchors && !a3){
            a3 = AddAnchor(
                activeTree, 
                ResolveAnchorName("NewAnchor", activeTree), 
                ANCHOR_HORIZONTAL, 
                p2.y
            );

            SetAnchorAsOffset(a3, GetAnchorByName(activeTree, "BottomMost"));
        }

        if (a0 && a1 && a2 && a3){
            SelectSurface(&activeTree->baseSurface, false, false);
            SelectAnchor(a0, false, false);
            SelectAnchor(a1, true, false);
            SelectAnchor(a2, true, false);
            SelectAnchor(a3, true, false);

            char cwd[OG_PATH_MAX];
            sprintf(cwd, "%s", GetWorkingDirectory());
            
            char surfacePath[OG_PATH_MAX];
            sprintf(surfacePath, "%s%s/data/%s", GetApplicationDirectory(), ROOT, cJSON_GetObjectItem(selectedExtensible, "path")->valuestring);
            
            ChangeDirectory(GetDirectoryPath(surfacePath));
            LoadSurface(GetFileName(surfacePath), NULL);
            ChangeDirectory(cwd);
        }

        else {
            if (!a0) OG_PushLog("%s", "Failed to find left anchor");
            if (!a1) OG_PushLog("%s", "Failed to find right anchor");
            if (!a2) OG_PushLog("%s", "Failed to find top anchor");
            if (!a3) OG_PushLog("%s", "Failed to find bottom anchor");
        }
    }
    
    selectedExtensible = NULL;
}

/* <== Callbacks ===============================================> */

static void InitTabs(OG_Viewport *v){
    v->ctx.style->spacing = 0;
    v->ctx.style->padding = 0;
}

static void UI(OG_Viewport *v, mu_Context *ctx){
    switch (activeTab){
        case 0: Inspector(v, ctx);     break;
        case 1: ComponentsUI(v, ctx);  break;
        case 2: ExtensbilesUI(v, ctx); break;
    }
}

static void RenderOverlay(OG_Viewport *v){
    switch (activeTab){
        case 0: break;
        case 1: ComponentsRenderOverlay(v);  break;
        case 2: ExtensiblesRenderOverlay(v); break;
    }
}

static void Update(OG_Viewport *v){
    if (currentState != DRAGGING_COMPONENT_STATE && OG.viewports.tail != v) return;
    switch (activeTab){
        case 0: break;
        case 1: ComponentsUpdate(v);  break;
        case 2: ExtensiblesUpdate(v); break;
    }
}

/* <== UI & Other ==============================================> */

static void Tabs(OG_Viewport *v, mu_Context *ctx){
    int tabsQ = sizeof(tabs)/sizeof(tabs[0]);
    const char *tabsLabels[tabsQ];
    for (int i=0; i<tabsQ; i++)
        tabsLabels[i] = tabs[i].name;
        
    int tabsWs[tabsQ];
    for (int i=0; i<tabsQ; i++)
        tabsWs[i] = tabs[i].labelWidth;
    mu_layout_row(ctx, tabsQ, tabsWs, 20);
    mu_tabs(
        ctx, tabsQ, 
        tabsLabels, 
        &activeTab, 
        MU_OPT_ALIGNCENTER | (WorkspaceIsDrawingExtensible() ? MU_OPT_NOINTERACT:0)
    );
}

OG_Viewport *SidePanel(){
    OG_Viewport *v = OG_InitViewport(
        "Workspace_SidePanel", 
        (Rectangle){}, 
        1.0f, 1.0f, 
        true, false, false, 
        NULL, 
        &Update, 
        NULL, 
        NULL, 
        &RenderOverlay, 
        NULL, 
        NULL, 
        &UI, 
        NULL, 
        NULL
    );

    v->updateAlways = true;
    workspace = OG_GetViewportByName("Workspace_Main");
    return v;
}

OG_Viewport *SidePanelTabs(int *w){
    OG_Viewport *v = OG_InitViewport(
        "Workspace_SidePanel_Tabs", 
        (Rectangle){}, 
        1.0f, 1.0f, 
        true, false, false, 
        &InitTabs, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        &Tabs, 
        NULL, 
        NULL
    );

    if (w){
        *w = 0;
        int tabsQ = sizeof(tabs)/sizeof(tabs[0]);
        for (int i=0; i<tabsQ; i++)
            *w += tabs[i].labelWidth;
    }
    
    return v;
}