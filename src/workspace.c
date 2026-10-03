// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <string.h>
#include "microui.h"
#include "origami.h"
#include "origami_tools.h"
#include "viewports.h"
#include "surfaces.h"
#include "contextmenus.h"
#include "utils.h"
#include "main.h"

#define GRID_C CLITERAL(Color){65, 65, 65, 255}
#define DEFAULT_ZOOM 0.48f

extern OG_Viewport *TreeView();
OG_Viewport *TreeViewTabs(int *w);

typedef enum State{
    IDLE_STATE,
    MOVING_SURFACE_STATE,
    MOVING_ANCHOR_STATE,
    MOVING_ANNOTATION_STATE,
    MOVING_ANNOTATION_EXTENSION_STATE
} State;

static const char *commands[] = {
    NULL
};

static RenderTexture gridTex;
static RenderTexture ghostTex;

static State currentState;
static Surface *targetSurface;
static Vector2 targetSurfaceDelta;
static Anchor *targetAnchor;
static float targetAnchorDelta;
static Annotation *targetAnnotation;

static int enableGrid = 0;
static int enableAnchors = 1;
static int enableAnnotations = 1;
int WorkspaceDrawSkins = 1;
int WorkspaceDrawCutOuts = 0;
static bool drawZoom = false;
static float gridSpacing = 50;

static OG_LayoutLine *tabsLine;
static OG_Viewport *tabsV;
static int tabsWidth;

static Vector2 lastRClick;

static LinePoints GetAnchorScreenPoints(OG_Viewport *v, Anchor *a){
    switch (a->type){
        case ANCHOR_HORIZONTAL:{
            float p = v->size.height*-1/2;
            p += GetAnchorAbsValue(a) * v->camera.zoom - v->camera.target.y * v->camera.zoom;
            
            return (LinePoints){
                {0, p},
                {v->size.width, p}
            };
        }

        case ANCHOR_VERTICAL:{
            float p = v->size.width/2;
            p += GetAnchorAbsValue(a) * v->camera.zoom - v->camera.target.x * v->camera.zoom;

            return (LinePoints){
                {p, 0},
                {p, v->size.height*-1}
            };
        }
    }
}

void DrawAnchor(OG_Viewport *v, Anchor *a, Color c){
    LinePoints points = GetAnchorScreenPoints(v, a);
    DrawLineDashedEx(
        points.A, 
        points.B, 
        1, 
        2, 
        2, 
        c
    );
}

/* <== StateMachine ============================================> */

void IdleState(OG_Viewport *v){
    
    // PAN
    if (OG_IsMouseButtonDown(v, MOUSE_MIDDLE_BUTTON) || OG_IsKeyDown(v, KEY_LEFT_CONTROL)){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_ALL);
        OG_ViewportUpdatePan(v);
        return;
    }
    
    OG_ViewportUpdateZoom(v);

    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON))
        lastRClick = mousePos;
    
    if (activeTree == NULL) return;

    // Grab an annotation
    for (Annotation *ann = activeTree->annotations; ann != NULL; ann = ann->next){
        if (!ann->A || !ann->B) continue;
        if (ann->opts & ANNOTATION_OPT_HIDDEN) continue;
            
        float margin = 3/v->camera.zoom;
        switch (ann->A->type){
            case ANCHOR_VERTICAL:{
                Rectangle r = {
                    GetAnchorAbsValue(GetAnchorAtLeft(ann->A, ann->B)), ann->offset-margin, 
                    GetDistanceBetweenAnchors(ann->A, ann->B), margin*2,
                };

                if (CheckCollisionPointRec(mousePos, r)){
                    OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
                    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                        activeTree->selectedAnnotation = ann;
                        targetAnnotation = ann;
                        if (OG_IsKeyDown(v, KEY_LEFT_SHIFT))
                            currentState = MOVING_ANNOTATION_EXTENSION_STATE;
                        else currentState = MOVING_ANNOTATION_STATE;
                        return;
                    }

                    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                        activeTree->selectedAnnotation = ann;
                        OpenAnnotationContextMenu();
                        return;
                    }
                }
                break;
            }

            case ANCHOR_HORIZONTAL:{
                Rectangle r = {
                    ann->offset-margin, GetAnchorAbsValue(GetAnchorAtTop(ann->A, ann->B)), 
                    margin*2, GetDistanceBetweenAnchors(ann->A, ann->B),
                };

                if (CheckCollisionPointRec(mousePos, r)){
                    OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
                    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                        activeTree->selectedAnnotation = ann;
                        targetAnnotation = ann;
                        if (OG_IsKeyDown(v, KEY_LEFT_SHIFT))
                            currentState = MOVING_ANNOTATION_EXTENSION_STATE;
                        else currentState = MOVING_ANNOTATION_STATE;
                        return;
                    }

                    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                        activeTree->selectedAnnotation = ann;
                        OpenAnnotationContextMenu();
                        return;
                    }
                }
                break;
            }
        }
    }

    // Grab an anchor
    if (enableAnchors){
        for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
            if (a->opts & ANCHOR_OPT_HIDDEN) continue;
            
            float val = GetAnchorAbsValue(a);
            float margin = 3/v->camera.zoom;

            switch (a->type){
                case ANCHOR_HORIZONTAL:{
                    if (mousePos.y >= val-margin && mousePos.y <= val+margin){
                        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
                        
                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                            SelectAnchor(a, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), false);
                            targetAnchor = a;
                            
                            if (!(a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) && !(a->opts & ANCHOR_OPT_FIXED)){
                                targetAnchorDelta = GetAnchorAbsValue(targetAnchor) - mousePos.y;
                                currentState = MOVING_ANCHOR_STATE;
                            }
                                
                            return;
                        }

                        if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                            if (!IsAnchorSelected(a))
                                SelectAnchor(a, false, false);
                            OpenAchorContextMenu();
                            return;
                        }
                    
                    }
                    break;
                }

                case ANCHOR_VERTICAL:{
                    if (mousePos.x >= val-margin && mousePos.x <= val+margin){
                        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
                        
                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                            SelectAnchor(a, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), false);
                            targetAnchor = a;
                            
                            if (!(a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) && !(a->opts & ANCHOR_OPT_FIXED)){
                                targetAnchorDelta = GetAnchorAbsValue(targetAnchor) - mousePos.x;
                                currentState = MOVING_ANCHOR_STATE;
                            }
                            
                            return;
                        }
                    
                        if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                            if (!IsAnchorSelected(a))
                                SelectAnchor(a, false, false);
                            OpenAchorContextMenu();
                            return;
                        }
                    }
                    break;
                }
            }
            
        }
    }

    // Grab a surface
    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
        targetSurface = GetSurfaceAt(mousePos, activeTree);
        if (targetSurface != NULL){
            SelectSurface(targetSurface, IsKeyDown(KEY_LEFT_SHIFT), false);
            
            // Base surface can't be moved
            if (targetSurface == &activeTree->baseSurface)
                return;

            // Extensible surfaces can't be moved either
            if (targetSurface->type == SURFACE_EXTENSIBLE)
                return;
            
            Vector2 targetGlobalPos = targetSurface->globalPos;
            targetSurfaceDelta = (Vector2){
                targetGlobalPos.x - mousePos.x,
                targetGlobalPos.y - mousePos.y
            };
            currentState = MOVING_SURFACE_STATE;
        }
    }

    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
        targetSurface = GetSurfaceAt(mousePos, activeTree);
        if (targetSurface != NULL){
            if (!IsSurfaceSelected(targetSurface))
                SelectSurface(targetSurface, false, false);
            OpenSurfaceContextMenu(&lastRClick);
        }
    }
    
}

void MovingSurfaceState(OG_Viewport *v){
    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    Vector2 sOgPos = targetSurface->pos;

    SetSurfaceGlobalPos(
        targetSurface, 
        (Vector2){
            mousePos.x + targetSurfaceDelta.x,
            mousePos.y + targetSurfaceDelta.y
        }
    );

    if (targetSurface->opts & SURFACE_OPT_FIXED_X)
        targetSurface->pos.x = sOgPos.x;

    if (targetSurface->opts & SURFACE_OPT_FIXED_Y)
        targetSurface->pos.y = sOgPos.y;
    
    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON)){
        targetSurface = NULL;
        currentState = IDLE_STATE;
    }
}

void MovingAnchorState(OG_Viewport *v){
    if (targetAnchor == NULL){
        currentState = IDLE_STATE;
        return;
    }

    Vector2 mousePos = OG_GetMouseViewportPosition(v);

    if (targetAnchor->type == ANCHOR_HORIZONTAL){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
        SetAnchorAbsValue(targetAnchor, mousePos.y + targetAnchorDelta);
    }
    
    else if (targetAnchor->type == ANCHOR_VERTICAL){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
        SetAnchorAbsValue(targetAnchor, mousePos.x + targetAnchorDelta);
    }
    
    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON)){
        currentState = IDLE_STATE;
        return;
    }
}

void MovingAnnotationState(OG_Viewport *v){
    if (targetAnnotation == NULL){
        currentState = IDLE_STATE;
        return;
    }

    Vector2 mousePos = OG_GetMouseViewportPosition(v);

    if (targetAnnotation->A->type == ANCHOR_VERTICAL){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
        targetAnnotation->offset = mousePos.y;
    }
    
    else{
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
        targetAnnotation->offset = mousePos.x;
    }

    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON)){
        currentState = IDLE_STATE;
        return;
    }
}

void MovingAnnotationExtensionState(OG_Viewport *v){
    if (targetAnnotation == NULL){
        currentState = IDLE_STATE;
        return;
    }

    targetAnnotation->opts |= ANNOTATION_OPT_EXTENSION_ENABLE;

    Vector2 mousePos = OG_GetMouseViewportPosition(v);

    if (targetAnnotation->A->type == ANCHOR_VERTICAL){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
        targetAnnotation->extensionLine = mousePos.y - targetAnnotation->offset;
    }
    
    else{
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
        targetAnnotation->extensionLine = mousePos.x - targetAnnotation->offset;
    }

    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON)){
        currentState = IDLE_STATE;
        return;
    }
}

/* <== UI ======================================================> */

static void ChangeAlignContextMenu(int opt, void *param){
    activeTree->surfaceSelection[0]->anchorAlign = opt;
}

static void GeneralPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "General Properties", MU_OPT_EXPANDED)) return;
    
    mu_equal_layout_l_auto(ctx, (const char *[]){"GridStep:", 0}, 0);
    mu_label(ctx, "GridStep:");
    mu_number_ex(ctx, &gridSpacing, 0, "%.2fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "GridOn:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_GridOn", 16);
    mu_checkbox(ctx, "", &enableGrid);
    mu_pop_id(ctx);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "EnableAnchors:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_EnableAnchors", 23);
    mu_checkbox(ctx, "", &enableAnchors);
    mu_pop_id(ctx);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "EnableAnnotations:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_EnableAnnotations", 27);
    mu_checkbox(ctx, "", &enableAnnotations);
    mu_pop_id(ctx);
    
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "DrawSkins:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_DrawSkins", 19);
    mu_checkbox(ctx, "", &WorkspaceDrawSkins);
    mu_pop_id(ctx);
    
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "DrawCutOuts:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "Workspace_DrawCutOuts", 21);
    mu_checkbox(ctx, "", &WorkspaceDrawCutOuts);
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

static void SurfacePropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Surface Properties", MU_OPT_EXPANDED)) return;

    Surface *s = NULL;
    if (activeTree && activeTree->surfaceSelection[0])
        s = activeTree->surfaceSelection[0];
    
    // Local position
    mu_equal_layout_l_auto(ctx, (const char *[]){" X:", " Y:", 0}, 0);
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
    mu_equal_layout_l_auto(ctx, (const char *[]){"GX:", "GY:", 0}, 0);
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
    mu_equal_layout_l_auto(ctx, (const char *[]){" W:", " H:", 0}, 0);
    mu_label(ctx, "W:");
    mu_number_custom(
        ctx, s ? &s->size.x:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
    );

    mu_label(ctx, "H:");
    mu_number_custom(
        ctx, s ? &s->size.y:NULL, 0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
    );

    // Rotation
    { // implmentation taken mu_equal_layout_l_auto
        mu_Container *cnt = mu_get_current_container(ctx);
        int spacing = ctx->style->spacing;
        int padding = ctx->style->padding;

        int inner_w = cnt->body.w - 2 * padding;
        int half_w = (inner_w - spacing) / 2;

        int space_w = ctx->text_width(ctx->style->font, " ", 1);
        int label_w = ctx->text_width(ctx->style->font, " A:", 2) + ctx->text_width(ctx->style->font, " ", 1);

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
    
    // Anchor Setup SubSection
    if (s){
        mu_vertical_space(ctx, 5);
        mu_layout_row(ctx, 1, (const int[]){-1}, 15);
        mu_label(ctx, "Anchor Setup:");
        
        mu_layout_row(ctx, 1, (const int[]){-1}, 95);
        mu_begin_panel(ctx, "AnchorSetup");
        
        switch (s->type){
            case SURFACE_EXTENSIBLE:{
                mu_equal_layout_l_auto(ctx, (const char*[]){"LCut:","RCut:",0}, 0);
                
                mu_label(ctx, "LCut:");
                mu_number_custom(
                    ctx,
                    &s->leftCut, 
                    0, "%.1fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
                );

                mu_label(ctx, "RCut:");
                mu_number_custom(
                    ctx,
                    &s->rightCut, 
                    0, "%.1fmm", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
                );
            
            
                mu_layout_row(ctx, 2, (const int[]){90, -1}, 0);
                mu_label(ctx, "AxisAlign:");
                const char *align = GetAnchorAlignStr(s);
                if (mu_button(ctx, align))
                    OG_OpenContextMenu(&ChangeAlignContextMenu, NULL, NULL, 3, "CENTER", "MIN", "MAX");
                
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

static void UI(OG_Viewport *v, mu_Context *ctx){
    GeneralPropertiesSection(v, ctx);
    AnchorPropertiesSection(v, ctx);
    SurfacePropertiesSection(v, ctx);
}

/* <== Callbacks ===============================================> */

static const char **GetCmds(){
    return commands;
}

static void ExecCmd(OG_Viewport *v, int argc, char **argv){
    if (argc <= 0) return;
    if (strcmp(argv[0], "draw_zoom") == 0){
        drawZoom = drawZoom ? false : true;
    }
}

static void Render(OG_Viewport *v){}

static void RenderOverlay(OG_Viewport *v){
    // Remder the grid to a texture
    if (gridTex.texture.width != v->renderTexture.texture.width || gridTex.texture.height != v->renderTexture.texture.height){
        UnloadRenderTexture(gridTex);
        gridTex = LoadRenderTexture(v->renderTexture.texture.width, v->renderTexture.texture.height);
    }

    if (ghostTex.texture.width != v->renderTexture.texture.width || ghostTex.texture.height != v->renderTexture.texture.height){
        UnloadRenderTexture(ghostTex);
        ghostTex = LoadRenderTexture(v->renderTexture.texture.width, v->renderTexture.texture.height);
    }

    // This Draw the grid on the grid texture
    // Then Draw the grid on the ghostTexture
    // Then use the ghostTexture as background
    // on the main texture

    int lod = 0;
    for (int i=0; i<lodsQ; i++){
        if (v->camera.zoom >= lods[i])
            lod = i;
    }

    EndTextureMode(); // leaves the original "viewport texture"
    BeginTextureMode(gridTex);
    ClearBackground(OG_VIEWPORT_BG_C);
    DrawViewportGrid(v, gridSpacing, GRID_C, !enableGrid);
    EndTextureMode();
    
    BeginTextureMode(ghostTex);
    ClearBackground(OG_VIEWPORT_BG_C);
    
    DrawTextureRec(
        gridTex.texture,
        (Rectangle){ 
            0, 0, 
            gridTex.texture.width, 
            -gridTex.texture.height 
        },
        (Vector2){ 0, 0 },
        WHITE
    );

    if (ghostTree && ghostTree != activeTree){
        DrawSurfaceTree(
            v, ghostTree, lod,
            &gridTex.texture, 
            Vector2Zero(),
            1.0f,
            ColorWithAlpha(WHITE, 100),
            ColorWithAlpha(PINK, 100),
            8.0f,
            ghostTreeFlipX,
            ghostTreeFlipY,
            false,
            true,
            WorkspaceDrawSkins,
            WorkspaceDrawCutOuts,
            false
        );

    }
        
    EndTextureMode();
    BeginTextureMode(v->renderTexture); // returns to the original "viewport texture"
    
    DrawTextureRec(
        ghostTex.texture,
        (Rectangle){ 0, 0, gridTex.texture.width, -gridTex.texture.height },
        (Vector2){ 0, 0 },
        WHITE
    );
    
    // Draw zoom (debug only)
    if (drawZoom){
        char zoom_text[256];
        memset(zoom_text, '\0', 256);
        sprintf(zoom_text, "%f", v->camera.zoom);
        DrawText(zoom_text, 0, 0, 20, GREEN);
    }

    if (activeTree){
        DrawSurfaceTree(
            v, activeTree, lod,
            &ghostTex.texture, 
            Vector2Zero(),
            1.0f,
            WHITE, 
            PINK,
            8.0f,
            false, false,
            false,
            false,
            WorkspaceDrawSkins,
            WorkspaceDrawCutOuts,
            false
        );
        
        // Draw Anchors
        if (enableAnchors){
            Color primary   = { 255, 255, 0, 117 };
            Color secondary = { 200, 140, 0, 117 };
            Color others    = { 205, 205, 240, 117 };
            Color locked = { 140, 140, 140, 110 };
            
            for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
                if (a->opts & ANCHOR_OPT_HIDDEN) continue;
                Color c = IsAnchorLocked(a) ? locked:others;
                if (a == activeTree->anchorSelection[0]) c = primary;
                for (int i=1; i<ANCHOR_SELECTION_BUF_LEN; i++){
                    if (a == activeTree->anchorSelection[i])
                        c = secondary;
                }
                
                DrawAnchor(v, a, c);
            }
        }

        // Draw annotations 
        if (enableAnnotations){        
            for (Annotation *ann = activeTree->annotations; ann != NULL; ann = ann->next){
                if ((IsAnchorPartOfCutout(ann->A) || IsAnchorPartOfCutout(ann->B)) && !WorkspaceDrawCutOuts) continue;
                if ((IsAnchorPartOfBoundary(ann->A) || IsAnchorPartOfBoundary(ann->B)) && !WorkspaceDrawSkins) continue;

                DrawAnnotation(
                    v, ann, PINK, 
                    10.0f / v->camera.zoom,
                    OG.defaultFontSize,
                    1.0f,
                    Vector2Zero()
                );
            }
        }
    }

    // Draw camera zoom and offset
    char offsetStr[256];
    sprintf(offsetStr, "Camera Offset-> (x:%f, y:%f)", v->camera.target.x, v->camera.target.y);
    DrawTextEx(
        OG.defaultFont, 
        offsetStr, 
        (Vector2){10, 0.0f}, 
        16.0f,
        2.0f,
        WHITE
    );
    
    char zoomStr[256];
    sprintf(zoomStr, "Camera Zoom-> %f", v->camera.zoom);
    DrawTextEx(
        OG.defaultFont, 
        zoomStr, 
        (Vector2){10, 16.0f}, 
        16.0f,
        2.0f,
        WHITE
    );

}

static void WorkspaceUI(OG_Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 1, (const int[]){100}, 25);
    mu_space(ctx);

    mu_layout_row(ctx, 2, (const int[]){1,100}, 0);
    mu_space(ctx);
    if (mu_button(ctx, "Reset View")){
        v->camera.zoom = DEFAULT_ZOOM;
        v->camera.target = Vector2Zero();
    }
}

static void Init(OG_Viewport *v){
    v->updateAlways = true;
    v->camera.zoom = DEFAULT_ZOOM;
}

static void Update(OG_Viewport *v){
    if (tabsV->size.width < tabsWidth)
        tabsLine->t = GetRatio(32, v->size.height*-1);
    else tabsLine->t = GetRatio(20, v->size.height*-1);
    
    if (v->hidden) return;
    switch (currentState){
        case IDLE_STATE:                        IdleState(v);                      break;
        case MOVING_SURFACE_STATE:              MovingSurfaceState(v);             break;
        case MOVING_ANCHOR_STATE:               MovingAnchorState(v);              break;
        case MOVING_ANNOTATION_STATE:           MovingAnnotationState(v);          break;
        case MOVING_ANNOTATION_EXTENSION_STATE: MovingAnnotationExtensionState(v); break;
    }
}

void Workspace(){
    const int width = 1230;
    const int height = 600;
    
    OG_Layout *l = OG_InitLayout(
        "Workspace", 
        (Rectangle){
            0,20,
            width,height
        }
    );

    // LINES
    tabsLine = OG_LayoutAddLine(
        l, OG_LAYOUT_LINE_H, 
        GetRatio(20, height)
    );
    tabsLine->fixed = true;
    OG_LayoutLine *lefttLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_V, 0.225f);
    OG_LayoutLine *rightLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_V, 0.79f);

    // TreeSection
    OG_LayoutContainer *treeTabsContainer = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], 
        tabsLine, 
        &l->lines[OG_LAYOUT_LINE_LEFT], 
        lefttLine
    );

    tabsV = TreeViewTabs(&tabsWidth);
    OG_LayoutAddViewport(treeTabsContainer, tabsV);

    OG_LayoutContainer *treeContainer = OG_LayoutAddContainer(
        l, 
        tabsLine, 
        &l->lines[OG_LAYOUT_LINE_BOTTOM], 
        &l->lines[OG_LAYOUT_LINE_LEFT], 
        lefttLine
    );

    
    OG_LayoutAddViewport(treeContainer, TreeView());

    // MainSection
    OG_LayoutContainer *mainContainer = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], 
        &l->lines[OG_LAYOUT_LINE_BOTTOM], 
        lefttLine, 
        rightLine
    );

    OG_LayoutAddViewport(
        mainContainer, 
        OG_InitViewport(
            "Workspace_Main", 
            (Rectangle){}, 
            0.2, 6, 
            false, true, false, 
            &Init, 
            &Update, 
            NULL, 
            &Render, 
            &RenderOverlay, 
            NULL,
            NULL, 
            &WorkspaceUI, 
            &GetCmds, 
            &ExecCmd
        )
    );

    // Inspector
    OG_LayoutContainer *inspectorContainer = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], 
        &l->lines[OG_LAYOUT_LINE_BOTTOM], 
        rightLine,
        &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutAddViewport(
        inspectorContainer,
        OG_InitViewport(
            "Workspace_Inspector", 
            (Rectangle){}, 
            1.0f, 1.0f, 
            true, false, false, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            &UI, 
            NULL, 
            NULL
        )
    );
    
    
}