// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "microui.h"
#include "origami.h"
#include "surfaces.h"
#include "utils.h"
#include "contextmenus.h"
#include "main.h"
#include "workspace.h"

static const char *commands[] = {
    NULL
};

const static float fontSize = 16;
const static float scrollBarWidth = 16;
const static float leftPadding = 10;

static int activeTab = 0;
static Tab tabs[] = {
    {"TreeList", 75},
    {"AnchorList", 95},
    {"SurfaceTree", 105},
    {"Annotations", 105}
};

/* <== Utils ===================================================> */

static void CalcScrollbar(OG_Viewport *v, Tab *t, int count){
    t->contentSize = fontSize * count;
    if (t->contentSize <= v->size.height*-1)
        t->scroll = 0;
    
    else {
        t->scroll += OG_GetMouseWheelMove(v) * OG_SCROLL_SPEED;
        if (t->scroll > 0) t->scroll = 0;
        if (t->scroll < v->size.height*-1 - t->contentSize){
            t->scroll = v->size.height*-1 - t->contentSize;
        }
    }
}

/* <== Callbacks ===============================================> */

static void InitTree(OG_Viewport *v){
    v->updateAlways = true;
}

static void InitTabs(OG_Viewport *v){
    v->ctx.style->spacing = 0;
    v->ctx.style->padding = 0;
}

static void OnTreeList(OG_Viewport *v){
    if (treeListQ <= 0) return;
    if (WorkspaceIsDrawingExtensible()) return;

    // Select tree
    float verticalOffset = 0;
    for (int i=0; i<treeListQ; i++){
        Rectangle r = {
            0, tabs[0].scroll+verticalOffset, 
            v->size.width, 
            fontSize
        };

        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                if (OG_IsKeyDown(v, KEY_LEFT_SHIFT)){
                    if (ghostTree == treeList[i])
                        ghostTree = NULL;
                    else ghostTree = treeList[i];
                    
                }

                else activeTree = treeList[i];
            }
        }

        verticalOffset += fontSize;
    }

    CalcScrollbar(v, &tabs[0], treeListQ);

    // Context Menu
    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
        verticalOffset = 0;
        for (int i=0; i<treeListQ; i++){
            Rectangle r = {
                0, tabs[0].scroll+verticalOffset, 
                v->size.width, 
                fontSize
            };

            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                activeTree = treeList[i];
                OpenSurfaceTreeContextMenu();
                return;
            }

            verticalOffset += fontSize;
        }
        
        OpenTreeListContextMenu();
    }
}

static void OnAnchorList(OG_Viewport *v){
    if (activeTree == NULL) return;
    if (activeTree->anchorsQ <= 0) return;
    if (WorkspaceIsDrawingExtensible()) return;
    
    // Select an anchor
    float verticalOffset = 0;
    Anchor *a = activeTree->anchors;
    int totalCount = activeTree->anchorsQ;
    while (true){
        Rectangle r = {
            0, tabs[1].scroll+verticalOffset, 
            v->size.width, 
            fontSize
        };

        if (a->refSurface){
            r.height += fontSize;
            totalCount++;
        }

        if (a->refA){
            r.height += fontSize;
            totalCount++;
        }

        if (a->refB){
            r.height += fontSize;
            totalCount++;
        }

        if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
            if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON))
                SelectAnchor(a, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), true);
            
            if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                if (!IsAnchorSelected(a))
                    SelectAnchor(a, false, false);
                OpenAchorContextMenu();
            }
        }

        verticalOffset += r.height;
        
        a = a->next;
        if (a== NULL)
            break;
    }

    CalcScrollbar(v, &tabs[1], totalCount);
}

static void OnTreeView(OG_Viewport *v){
    if (activeTree == NULL) return;
    if (WorkspaceIsDrawingExtensible()) return;

    // Select viewport
    float verticalOffset = 0;
    for (int i=0; i<activeTree->surfacesQ; i++){
        Surface *s = activeTree->linearSurfaceTree[i];
        Rectangle r = {
            0, tabs[2].scroll+verticalOffset, 
            v->size.width, 
            fontSize
        };

        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r))
                SelectSurface(s, OG_IsKeyDown(v, KEY_LEFT_SHIFT), true);
        }

        if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                if (!IsSurfaceSelected(s))
                    SelectSurface(s, false, false);
                OpenSurfaceContextMenu(NULL);
            }
        }

        verticalOffset += fontSize;
    }

    CalcScrollbar(v, &tabs[2], activeTree->surfacesQ);
}

static void OnAnnotationsList(OG_Viewport *v){
    if (!activeTree) return;
    if (activeTree->annotationsQ <= 0) return;
    if (WorkspaceIsDrawingExtensible()) return;

    float verticalOffset = 0;
    Annotation *a = activeTree->annotations;
    while (a != NULL){
        Rectangle r = {
            0, tabs[3].scroll+verticalOffset, 
            v->size.width, 
            fontSize
        };

        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                if (activeTree->selectedAnnotation == a)
                    activeTree->selectedAnnotation = NULL;
                else activeTree->selectedAnnotation = a;
            }
        }

        if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                activeTree->selectedAnnotation = a;
                OpenAnnotationContextMenu();
            }
        }

        verticalOffset += fontSize;
        a = a->next;
    }
    
    CalcScrollbar(v, &tabs[3], activeTree->annotationsQ);
}

static void Update(OG_Viewport *v){
    for (int i=0; i<treeListQ; i++){
        if (treeList[i] != activeTree){
            treeList[i]->surfaceSelection[0] = NULL;
            treeList[i]->surfaceSelection[1] = NULL;
        }
    }
    
    if (v->hidden) return;
    switch (activeTab){
        case 0: OnTreeList(v);        break;
        case 1: OnAnchorList(v);      break;
        case 2: OnTreeView(v);        break;
        case 3: OnAnnotationsList(v); break;
    }
}


/* <== Drawing =================================================> */

static void DrawScrollBar(OG_Viewport *v, Tab *t){
    if (t->contentSize > (v->size.height*-1)){
        DrawRectangle( //bg
            v->size.width - scrollBarWidth, 0,
            scrollBarWidth, (v->size.height*-1),
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLBASE]
        );

        float scrollPercent = t->scroll*-1/t->contentSize;
        DrawRectangle( //thumb
            v->size.width - scrollBarWidth, (v->size.height*-1)*scrollPercent, 
            scrollBarWidth, 
            ((v->size.height*-1)/t->contentSize)*((v->size.height*-1)),
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLTHUMB]
        );
    }
}

static void DrawTreeList(OG_Viewport *v){
    float verticalOffset = 0;
    for (int i=0; i<treeListQ; i++){
        Color color = OG_TEXT_C;
        if (treeList[i] == ghostTree) color = HIGHLIGHT_C1;
        if (treeList[i] == activeTree) color = HIGHLIGHT_C;
        
        DrawTextEx(
            OG.defaultFont, 
            treeList[i]->baseSurface.id, 
            (Vector2){
                leftPadding,
                tabs[0].scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            color
        );
        verticalOffset += fontSize;
    }

    DrawScrollBar(v, &tabs[0]);
}

static void DrawAnchorList(OG_Viewport *v){
    if (!activeTree) return;
    if (activeTree->anchorsQ <= 0) return;
    float verticalOffset = 0;

    Anchor *a = activeTree->anchors;
    while (true){
        Color c = OG_TEXT_C;
        
        if (a->opts & ANCHOR_OPT_HIDDEN)
            c = (Color){100,100,100, 255};
        
        if (a == activeTree->anchorSelection[0]) c = HIGHLIGHT_C;
        for (int i=1; i<ANCHOR_SELECTION_BUF_LEN; i++){
            if (a == activeTree->anchorSelection[i])
                c = HIGHLIGHT_C1;
        }
         
        // Draw anchor name
        DrawTextEx(
            OG.defaultFont, 
            a->name, 
            (Vector2){
                0 + leftPadding,
                tabs[1].scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            c
        );

        // Draw constraint type
        if (a->constraint != ANCHOR_CONSTRAINT_NONE){
            Vector2 textSize = MeasureTextEx(OG.defaultFont, a->name, fontSize, 2.0f);
            Vector2 depthSize = MeasureTextEx(OG.defaultFont, "─── A:", fontSize, 0);
            
            DrawTextEx(
                OG.defaultFont, 
                GetAnchorConstraintStr(a),
                (Vector2){
                    0 + leftPadding + textSize.x + 10,
                    tabs[1].scroll + verticalOffset
                },

                fontSize,
                2.0f,
                c
            );

            // Draw Boxing
            if (a->refSurface){
                verticalOffset += fontSize;
                DrawTextEx(
                    OG.defaultFont, 
                    ("└── S:"),
                    (Vector2){
                        0 + leftPadding,
                        tabs[1].scroll + verticalOffset
                    },

                    fontSize,
                    0,
                    c
                );
            
                // Draw ref name
                char refName[ strlen(a->refSurface->name)+1 ];
                sprintf(refName, "%s", a->refSurface->name);
                DrawTextEx(
                    OG.defaultFont, 
                    refName,
                    (Vector2){
                        0 + leftPadding + depthSize.x + 5,
                        tabs[1].scroll + verticalOffset
                    },

                    fontSize,
                    2.0f,
                    c
                );
            }
            
            else {
                if (a->refA){
                    verticalOffset += fontSize;
                    DrawTextEx(
                        OG.defaultFont, 
                        (a->refB ? "├── A:": "└── A:"),
                        (Vector2){
                            0 + leftPadding,
                            tabs[1].scroll + verticalOffset
                        },

                        fontSize,
                    0,
                        c
                    );
                
                    // Draw ref name
                    char refName[ strlen(a->refA->name) + (a->refA->tree == a->tree ? 0:(2 + strlen(a->refA->tree->baseSurface.name))) +1];
                    if (a->refA->tree != a->tree) sprintf(refName, "%s::%s", a->refA->tree->baseSurface.name, a->refA->name);
                    else sprintf(refName, "%s", a->refA->name);
                    DrawTextEx(
                        OG.defaultFont, 
                        refName,
                        (Vector2){
                            0 + leftPadding + depthSize.x + 5,
                            tabs[1].scroll + verticalOffset
                        },

                        fontSize,
                        2.0f,
                        c
                    );
                }

                if (a->refB){
                    verticalOffset += fontSize;
                    DrawTextEx(
                        OG.defaultFont, 
                        "└── B:",
                        (Vector2){
                            0 + leftPadding,
                            tabs[1].scroll + verticalOffset
                        },

                        fontSize,
                    0,
                        c
                    );
                
                    // Draw ref name
                    char refName[ strlen(a->refB->name) + (a->refB->tree == a->tree ? 0:(2 + strlen(a->refB->tree->baseSurface.name))) +1];
                    if (a->refB->tree != a->tree) sprintf(refName, "%s::%s", a->refB->tree->baseSurface.name, a->refB->name);
                    else sprintf(refName, "%s", a->refB->name);
                    DrawTextEx(
                        OG.defaultFont, 
                        refName,
                        (Vector2){
                            0 + leftPadding + depthSize.x + 5,
                            tabs[1].scroll + verticalOffset
                        },

                        fontSize,
                        2.0f,
                        c
                    );
                }
            }
        }

        verticalOffset += fontSize;
        
        a = a->next;
        if (a== NULL)
            break;
    }

    DrawScrollBar(v, &tabs[1]);
}

static void DrawTreeView(OG_Viewport *v){
    if (activeTree == NULL) return;

    float verticalOffset = 0;
    Vector2 depthSize = MeasureTextEx(OG.defaultFont, "───", fontSize, 0);
    
    // FOR EACH ELEMENT ON TREE
    SetTextureFilter(OG.defaultFont.texture, TEXTURE_FILTER_POINT);
    for (int i=0; i<activeTree->surfacesQ; i++){
        Surface *s = activeTree->linearSurfaceTree[i];
        
        // DRAW TREE
        for (int o=0; o<s->treeDepth; o++){
            char *c = "   ";
           
            if (o == s->treeDepth-1)
                c = "└──";
            
            // Search for siblings
            bool haveSiblings = false;
            if (i<activeTree->surfacesQ-1){
                for (int y=i+1; y<activeTree->surfacesQ; y++){
                    if (activeTree->linearSurfaceTree[y]->treeDepth == o+1)
                        haveSiblings = true;
                    if (activeTree->linearSurfaceTree[y]->treeDepth <= o)
                        break;
                }
            }

            if (haveSiblings){
                if (o == s->treeDepth-1) c = "├──";
                else c = "│  ";
            }
                
            DrawTextEx(
                OG.defaultFont, 
                c, 
                (Vector2){
                    (int)(leftPadding + depthSize.x*o),
                    (int)(tabs[2].scroll + verticalOffset)
                },
                fontSize,
                0,
                OG_TEXT_C
            );
        }
        
        Color c = OG_TEXT_C;
        if (s->opts & SURFACE_OPT_HIDDEN)
            c = (Color){100,100,100, 255};
        
        if (s == activeTree->surfaceSelection[0]) c = HIGHLIGHT_C;
        for (int i=1; i<SURFACE_SELECTION_BUF_LEN; i++){
            if (s == activeTree->surfaceSelection[i])
                c = HIGHLIGHT_C1;
        }
        
        // DRAW TEXT
        DrawTextEx(
            OG.defaultFont, 
            s->name, 
            (Vector2){
                leftPadding + depthSize.x*s->treeDepth, 
                tabs[2].scroll + verticalOffset
            }, 
            fontSize, 
            2.0f, 
            c
        );

        verticalOffset += fontSize;
    }
    SetTextureFilter(OG.defaultFont.texture, TEXTURE_FILTER_BILINEAR);

    DrawScrollBar(v, &tabs[2]);
}

static void DrawAnnotationsList(OG_Viewport *v){
    if (!activeTree) return;
    if (activeTree->annotationsQ <= 0) return;

    float verticalOffset = 0;
    Annotation *a = activeTree->annotations;

    while (a != NULL){
        Color c = OG_TEXT_C;
        if (a->opts & ANNOTATION_OPT_HIDDEN)
            c = (Color){100,100,100, 255};

        if (a == activeTree->selectedAnnotation) 
            c = HIGHLIGHT_C;
        
        DrawTextEx(
            OG.defaultFont, 
            a->A->name, 
            (Vector2){
                leftPadding,
                tabs[3].scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            c
        );

        float leftOffset = MeasureTextEx(
            OG.defaultFont, 
            a->A->name, 
            fontSize, 
            2.0f
        ).x;

        DrawTextEx(
            OG.defaultFont, 
            "->", 
            (Vector2){
                leftPadding + leftOffset,
                tabs[3].scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            c
        );

        leftOffset += MeasureTextEx(
            OG.defaultFont, 
            "->", 
            fontSize, 
            2.0f
        ).x;

        DrawTextEx(
            OG.defaultFont, 
            a->B->name, 
            (Vector2){
                leftPadding + leftOffset,
                tabs[3].scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            c
        );

        verticalOffset += fontSize;
        a = a->next;
    }

    DrawScrollBar(v, &tabs[3]);
}

static void RenderOverlay(OG_Viewport *v){
    switch (activeTab){
        case 0: DrawTreeList(v);        break;
        case 1: DrawAnchorList(v);      break;
        case 2: DrawTreeView(v);        break;
        case 3: DrawAnnotationsList(v); break;
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
    mu_tabs(ctx, tabsQ, tabsLabels, &activeTab, MU_OPT_ALIGNCENTER);
}

static const char **GetCmds(){
    return commands;
}

static void ExecCmd(OG_Viewport *v, int argc, char **argv){
}

OG_Viewport *TreeView(){
    OG_Viewport *v = OG_InitViewport(
        "Workspace_TreeView", 
        (Rectangle){}, 
        1.0f, 1.0f, 
        false, true, false, 
        &InitTree, 
        &Update, 
        NULL, 
        NULL, 
        &RenderOverlay, 
        NULL,
        NULL, 
        NULL,
        &GetCmds, 
        &ExecCmd
    );

    return v;
}

OG_Viewport *TreeViewTabs(int *w){
    OG_Viewport *v = OG_InitViewport(
        "Workspace_TreeView_Tabs", 
        (Rectangle){}, 
        1.0f, 1.0f, 
        false, true, false, 
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