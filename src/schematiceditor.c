// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include "contextmenus.h"
#include "microui.h"
#include "origami.h"
#include "schematics.h"
#include "utils.h"
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 10.0f
#define DOTS_COLOR (Color){63, 63, 63, 255}
#define SCHEMATICS_LIST_HEIGHT 20
#define ADD_SCHEMATIC_BTN_W 25

Schematic *schematics = NULL;
Schematic *activeSchematic = NULL;

static size_t schematicsQ = 0;
static int *schematicsWidths = NULL;

static OG_Layout *layout;
static OG_LayoutLine *schematicListTabsLine;

/* <== Utils  ==================================================> */

static void DrawDotGrid(OG_Viewport *v, float size, Color color){
    float width = v->size.width;
    float height = v->size.height*-1;
    
    const float gridSize = size * v->camera.zoom;

    Vector2 offset = {
        width/2 - (v->camera.target.x*v->camera.zoom), 
        height/2 - (v->camera.target.y*v->camera.zoom)
    };

    while (offset.x > width/2 + gridSize) offset.x -= gridSize;
    while (offset.x < width/2 - gridSize) offset.x += gridSize;
    while (offset.y > height/2 + gridSize) offset.y -= gridSize;
    while (offset.y < height/2 - gridSize) offset.y += gridSize;

    Vector2 dot = Vector2Zero();
    while (true){
        while (true){
            DrawCircleV((Vector2){ offset.x + dot.x, offset.y + dot.y }, 1.0f, color);
            DrawCircleV((Vector2){ offset.x - dot.x, offset.y + dot.y }, 1.0f, color);
            DrawCircleV((Vector2){ offset.x + dot.x, offset.y - dot.y }, 1.0f, color);
            DrawCircleV((Vector2){ offset.x - dot.x, offset.y - dot.y }, 1.0f, color);

            dot.x += gridSize;
            if (dot.x >= width)
                break;
        }

        dot.x = 0;
        dot.y += gridSize;
        if (dot.y >= height)
            break;
    }
}

static Vector2 SnapToGrid(Vector2 point, float size){
    return (Vector2) {
        roundf(point.x / size) * size,
        roundf(point.y / size) * size
    };
}

/* <== Callbacks ===============================================> */

static void SchematicListInit(OG_Viewport *v){
    v->ctx.style->spacing = 0;
    v->ctx.style->padding = 0;

    schematicsWidths = calloc(1, sizeof(int));
    schematicsWidths[0] = ADD_SCHEMATIC_BTN_W;
}

static void SchematicListUpdate(OG_Viewport *v){
    if (v->hidden) return;

    int fullW = 0;
    for (int i=0; i<schematicsQ+1; i++)
        fullW += schematicsWidths[i];
    
    if (fullW > v->size.width) schematicListTabsLine->t = GetRatio(SCHEMATICS_LIST_HEIGHT + 13, layout->size.y);
    else schematicListTabsLine->t = GetRatio(SCHEMATICS_LIST_HEIGHT, layout->size.y);

    if (OG.viewports.tail != v) return;
}

static void SchematicListUI(OG_Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, schematicsQ+1, schematicsWidths, 20);
    
    for (Schematic *s = schematics; s != NULL; s = s->next){
        int rs = mu_tab(ctx, s->name, activeSchematic == s, MU_OPT_ALIGNCENTER);
        if (rs) activeSchematic = s;
        if (rs == 2) printf("%s\n", "fuck");
        
    }

    if (mu_button(ctx, "+")){
        Schematic *s = AddSchematic(schematics, "NewSchematic");
        if (schematics == NULL) schematics = s;

        schematicsQ++;
        if (schematicsWidths) free(schematicsWidths);
        schematicsWidths = calloc(schematicsQ+1, sizeof(int));
        schematicsWidths[schematicsQ] = ADD_SCHEMATIC_BTN_W;

        int i=0;
        for (Schematic *s = schematics; s != NULL; s = s->next){
            schematicsWidths[i] = MeasureTextEx(OG.defaultFont,s->name,OG.defaultFontSize,2).x + 20;
            i++;
        }

        activeSchematic = s;
    }
}

static void CanvasUpdate(OG_Viewport *v){
    // PAN
    if (OG_IsMouseButtonDown(v, MOUSE_MIDDLE_BUTTON) || OG_IsKeyDown(v, KEY_LEFT_CONTROL)){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_ALL);
        OG_ViewportUpdatePan(v);
        return;
    }
    
    OG_ViewportUpdateZoom(v);

    if (OG_IsMouseButtonPressed(v, MOUSE_BUTTON_RIGHT)){
        OpenSchematicEditorContextMenu();
    }
}

static void CanvasRenderOverlay(OG_Viewport *v){
    DrawDotGrid(v, GRID_SIZE, DOTS_COLOR);

    // just a test
    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    Vector2 snapped = SnapToGrid(mousePos, GRID_SIZE);
    
    DrawCircleV(
        GetWorldToScreen2D(snapped, v->camera), 
        1.0f, 
        RED
    );

    if (!activeSchematic) return;
    for (Symbol *s = activeSchematic->symbols; s != NULL; s = s->next){
        DrawSymbol(s);
    }
}

void SchematicEditor(){
    const int width = 1000;
    const int height = 600;

    layout = OG_InitLayout(
        "SchematicEditor", 
        (Rectangle){0, 20, width, height}
    );

    // Lines
    OG_LayoutLine *leftL = OG_LayoutAddLine(
        layout, 
        OG_LAYOUT_LINE_V, 
        GetRatio(200, width)
    );
    
    OG_LayoutLine *rightL = OG_LayoutAddLine(
        layout, 
        OG_LAYOUT_LINE_V, 
        GetRatio(width-200, width)
    );

    schematicListTabsLine = OG_LayoutAddLine(
        layout, 
        OG_LAYOUT_LINE_H, 
        GetRatio(SCHEMATICS_LIST_HEIGHT, height)
    );
    schematicListTabsLine->fixed = true;
    
    // Containers
    OG_LayoutContainer *leftContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        &layout->lines[OG_LAYOUT_LINE_LEFT], 
        leftL
    );
    
    OG_LayoutContainer *centerContainerTop = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        schematicListTabsLine, 
        leftL,
        rightL
    );

    OG_LayoutContainer *centerContainerBottom = OG_LayoutAddContainer(
        layout, 
        schematicListTabsLine, 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        leftL,
        rightL
    );

    OG_LayoutContainer *rightContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        rightL,
        &layout->lines[OG_LAYOUT_LINE_RIGHT]
    );

    // Viewports
    OG_LayoutAddViewport(
        leftContainer, 
        OG_InitViewport(
            "SchematicEditor_Tree", 
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
            NULL,
            NULL,
            NULL
        )
    );

    OG_Viewport *schematicList = OG_InitViewport(
        "SchematicEditor_SchematicList", 
        (Rectangle){}, 
        1.0f, 1.0f, 
        true, false, false, 
        &SchematicListInit, 
        &SchematicListUpdate, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        &SchematicListUI, 
        NULL, 
        NULL
    );

    OG_LayoutAddViewport( centerContainerTop, schematicList);
    schematicList->updateAlways = true;

    OG_LayoutAddViewport(
        centerContainerBottom, 
        OG_InitViewport(
            "SchematicEditor_Canvas", 
            (Rectangle){}, 
            1.0f, 5.0f, 
            true, false, false, 
            NULL, 
            &CanvasUpdate, 
            NULL, 
            NULL, 
            &CanvasRenderOverlay, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL
        )
    );

    OG_LayoutAddViewport(
        rightContainer, 
        OG_InitViewport(
            "SchematicEditor_Inspector", 
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
            NULL, 
            NULL, 
            NULL
        )
    );
}
