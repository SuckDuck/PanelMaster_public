// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"
#include "origami.h"
#include "viewports.h"
#include "surfaces.h"
#include "contextmenus.h"
#include "utils.h"
#include "main.h"
#include "workspace.h"

#define GRID_C CLITERAL(Color){65, 65, 65, 255}
#define DEFAULT_ZOOM 0.48f

// These come from treeview.c and sidepanel.c
extern OG_Viewport *TreeView();
extern OG_Viewport *TreeViewTabs(int *w);
extern OG_Viewport *SidePanel();
extern OG_Viewport *SidePanelTabs(int *w);
extern void SidePanelDrawExtensible(Vector2 p1, Vector2 p2);

typedef enum State{
    IDLE_STATE,
    MOVING_SURFACE_STATE,
    MOVING_ANCHOR_STATE,
    MOVING_ANNOTATION_STATE,
    MOVING_ANNOTATION_EXTENSION_STATE,
    DRAWING_EXTENSIBLE
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

// inspector settings
InspectorSettings inspectorSettings = {
    .enableGrid = 0,
    .enableAnchors = 1,
    .enableAnnotations = 1,
    .DrawSkins = 1,
    .DrawCutOuts = 0,
    .gridSpacing = 50
};

static struct {
    OG_LayoutLine *line;
    OG_Viewport *v;
    int width;
} treeTabs;

static struct {
    OG_LayoutLine *line;
    OG_Viewport *v;
    int width;
} sidePanelTabs;

static Vector2 lastRClick;
static Vector2 *drawingExtensiblePoints[2];

/* <== Utils =======================================================> */

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

/* <== StateMachine ================================================> */

static void IdleState(OG_Viewport *v){
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
    if (inspectorSettings.enableAnchors){
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

static void MovingSurfaceState(OG_Viewport *v){
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

static void MovingAnchorState(OG_Viewport *v){
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

static void MovingAnnotationState(OG_Viewport *v){
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

static void MovingAnnotationExtensionState(OG_Viewport *v){
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

static void DrawingExtensibleState(OG_Viewport *v){
    // PAN
    if (OG_IsMouseButtonDown(v, MOUSE_MIDDLE_BUTTON) || OG_IsKeyDown(v, KEY_LEFT_CONTROL)){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_ALL);
        OG_ViewportUpdatePan(v);
        return;
    }
    
    OG_ViewportUpdateZoom(v);
    Vector2 mousePos = OG_GetMouseViewportPosition(v);

    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
        drawingExtensiblePoints[0] = calloc(1, sizeof(Vector2));
        *drawingExtensiblePoints[0] = mousePos;
    }

    if (drawingExtensiblePoints[0]){
        if (!drawingExtensiblePoints[1])
            drawingExtensiblePoints[1] = calloc(1, sizeof(Vector2));

        Vector2 p0 = *drawingExtensiblePoints[0];
        Vector2 p1 = mousePos;

        if (fabsf(p1.x - p0.x) > fabsf(p1.y - p0.y)) p1.y = p0.y; // horizontal
        else p1.x = p0.x; // vertical
            
        *drawingExtensiblePoints[1] = p1;
    }

    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON) && drawingExtensiblePoints[0] && drawingExtensiblePoints[1]){
        SidePanelDrawExtensible(*drawingExtensiblePoints[0], *drawingExtensiblePoints[1]);
        free(drawingExtensiblePoints[0]);
        free(drawingExtensiblePoints[1]);
        drawingExtensiblePoints[0] = NULL;
        drawingExtensiblePoints[1] = NULL;
        currentState = IDLE_STATE;
        return;
    }
    
    OG_ChangeCursor(v, MOUSE_CURSOR_CROSSHAIR);
}

/* <== Callbacks ===============================================> */

static const char **GetCmds(){
    return commands;
}

static void ExecCmd(OG_Viewport *v, int argc, char **argv){
    if (argc <= 0) return;
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
    DrawViewportGrid(v, inspectorSettings.gridSpacing, GRID_C, !inspectorSettings.enableGrid);
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
            inspectorSettings.DrawSkins,
            inspectorSettings.DrawCutOuts,
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
            inspectorSettings.DrawSkins,
            inspectorSettings.DrawCutOuts,
            false
        );
        
        // Draw Anchors
        if (inspectorSettings.enableAnchors){
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
        if (inspectorSettings.enableAnnotations){        
            for (Annotation *ann = activeTree->annotations; ann != NULL; ann = ann->next){
                if ((IsAnchorPartOfCutout(ann->A) || IsAnchorPartOfCutout(ann->B)) && !inspectorSettings.DrawCutOuts) continue;
                if ((IsAnchorPartOfBoundary(ann->A) || IsAnchorPartOfBoundary(ann->B)) && !inspectorSettings.DrawSkins) continue;

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

    // Draw the "drawing extensible" points and line
    if (currentState == DRAWING_EXTENSIBLE && drawingExtensiblePoints[0] && drawingExtensiblePoints[1]){
        Vector2 p0 = GetWorldToScreen2D(*drawingExtensiblePoints[0], v->camera);
        Vector2 p1 = GetWorldToScreen2D(*drawingExtensiblePoints[1], v->camera);

        const float radius = 3.0f;
        const float lineWidth = 2.0f;
        const Color c = {0, 220, 255, 255};

        DrawLineEx(p0, p1, lineWidth, c);

        DrawCircleV(p0, radius, c);
        DrawCircleV(p1, radius, c);
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
    // Autoadjust the treeview tabs height
    if (treeTabs.v->size.width < treeTabs.width)
        treeTabs.line->t = GetRatio(32, v->size.height*-1);
    else treeTabs.line->t = GetRatio(20, v->size.height*-1);

    // Autoadjust the sidepanel tabs height
    if (sidePanelTabs.v->size.width < sidePanelTabs.width)
        sidePanelTabs.line->t = GetRatio(32, v->size.height*-1);
    else sidePanelTabs.line->t = GetRatio(20, v->size.height*-1);

    if (v->hidden) return;
    switch (currentState){
        case IDLE_STATE:                        IdleState(v);                      break;
        case MOVING_SURFACE_STATE:              MovingSurfaceState(v);             break;
        case MOVING_ANCHOR_STATE:               MovingAnchorState(v);              break;
        case MOVING_ANNOTATION_STATE:           MovingAnnotationState(v);          break;
        case MOVING_ANNOTATION_EXTENSION_STATE: MovingAnnotationExtensionState(v); break;
        case DRAWING_EXTENSIBLE:                DrawingExtensibleState(v);         break;
    }
}

void WorkspaceStartDrawingExtensible(){
    targetSurface = NULL;
    targetAnchor = NULL;
    activeTree->surfaceSelection[0] = NULL;
    activeTree->surfaceSelection[1] = NULL;
    activeTree->anchorSelection[0] = NULL;
    activeTree->anchorSelection[1] = NULL;
    activeTree->anchorSelection[2] = NULL;

    currentState = DRAWING_EXTENSIBLE;
}

void WorkspaceCancelDrawingExtensible(){
    currentState = IDLE_STATE;
}

bool WorkspaceIsDrawingExtensible(){
    return currentState == DRAWING_EXTENSIBLE;
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
    treeTabs.line = OG_LayoutAddLine(
        l, OG_LAYOUT_LINE_H, 
        GetRatio(20, height)
    );
    treeTabs.line->fixed = true;

    sidePanelTabs.line = OG_LayoutAddLine(
        l, OG_LAYOUT_LINE_H, 
        GetRatio(20, height)
    );
    sidePanelTabs.line->fixed = true;

    OG_LayoutLine *lefttLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_V, 0.225f);
    OG_LayoutLine *rightLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_V, 0.79f);

    // TreeSection
    OG_LayoutContainer *treeTabsContainer = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], 
        treeTabs.line, 
        &l->lines[OG_LAYOUT_LINE_LEFT], 
        lefttLine
    );

    treeTabs.v = TreeViewTabs(&treeTabs.width);
    OG_LayoutAddViewport(treeTabsContainer, treeTabs.v);

    OG_LayoutContainer *treeContainer = OG_LayoutAddContainer(
        l, 
        treeTabs.line, 
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

    // SidePanel
    OG_LayoutContainer *inspectorTabsContainer = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], 
        sidePanelTabs.line, 
        rightLine,
        &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    sidePanelTabs.v = SidePanelTabs(&sidePanelTabs.width);
    OG_LayoutAddViewport(inspectorTabsContainer, sidePanelTabs.v);
    
    OG_LayoutContainer *inspectorContainer = OG_LayoutAddContainer(
        l, 
        sidePanelTabs.line, 
        &l->lines[OG_LAYOUT_LINE_BOTTOM], 
        rightLine,
        &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutAddViewport(inspectorContainer, SidePanel());
    
}