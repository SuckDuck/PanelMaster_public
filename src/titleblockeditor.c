// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <ctype.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include "contextmenus.h"
#include "documentation.h"
#include "microui.h"
#include "origami.h"
#include "origami_tools.h"
#include "titleblock.h"
#include "utils.h"
#include "main.h"

typedef enum State{
    IDLE_STATE,
    MOVING_LINE_STATE
} State;

const static float fontSize = 16;
const static float scrollBarWidth = 16;
const static float leftPadding = 10;
static float contentSize = 0.0f;
static float scroll = 0.0f;

static State currentState;
static TitleBlockLine *targetLine;
static float targetDelta;

/* <== Utils ===================================================> */

static void UpdateRectLabelName(char *name){
    char *p = name;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '\0') {
        OG_PushLog("Title block label cannot be empty");
        return;
    }

    if (!SetTitleBlockLabel(activeTitleBlock->selectedRect, name))
        OG_PushLog("Title block label \"%s\" already exists", name);
}

static void CalcScrollbar(OG_Viewport *v, int count){
    contentSize = fontSize * count;
    if (contentSize <= v->size.height*-1)
        scroll = 0;
    
    else {
        scroll += OG_GetMouseWheelMove(v) * OG_SCROLL_SPEED;
        if (scroll > 0) scroll = 0;
        if (scroll < v->size.height*-1 - contentSize){
            scroll = v->size.height*-1 - contentSize;
        }
    }
}

static void DrawScrollBar(OG_Viewport *v){
    if (contentSize > (v->size.height*-1)){
        DrawRectangle( //bg
            v->size.width - scrollBarWidth, 0,
            scrollBarWidth, (v->size.height*-1),
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLBASE]
        );

        float scrollPercent = scroll*-1/contentSize;
        DrawRectangle( //thumb
            v->size.width - scrollBarWidth, (v->size.height*-1)*scrollPercent, 
            scrollBarWidth, 
            ((v->size.height*-1)/contentSize)*((v->size.height*-1)),
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLTHUMB]
        );
    }
}

/* <== Canvas Section ==========================================> */

static void CanvasInit(OG_Viewport *v){
}

// StateMachine
static void IdleState(OG_Viewport *v){
    if (!activeTitleBlock) return;
    OG_ViewportUpdateZoom(v);
    if (OG_IsMouseButtonDown(v, MOUSE_MIDDLE_BUTTON) || OG_IsKeyDown(v, KEY_SPACE)){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_ALL);
        OG_ViewportUpdatePan(v);
        return;
    }

    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    
    // for each rect
    for (TitleBlockRect *tbr = activeTitleBlock->rects; tbr != NULL; tbr = tbr->next){
        if (IsPInsideTitleBlockRect(mousePos, tbr)){
            if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
                OpenTitleBlockRectContextMenu(tbr);
                activeTitleBlock->selectedRect = tbr;
                return;
            }

            if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                if (activeTitleBlock->selectedRect == tbr) 
                    activeTitleBlock->selectedRect = NULL;
                else activeTitleBlock->selectedRect = tbr;
            }
            
            TitleBlockLine *lines[] = {
                tbr->lines.top,
                tbr->lines.bottom,
                tbr->lines.left,
                tbr->lines.right
            };

            // for each lin in rect
            for (int o=0; o<4; o++){
                if (lines[o] == activeTitleBlock->lines) continue;
                if (lines[o] == activeTitleBlock->lines->next) continue;
                if (lines[o] == activeTitleBlock->lines->next->next) continue;
                if (lines[o] == activeTitleBlock->lines->next->next->next) continue;

                if (IsPInsideTitleBlockLine(mousePos, lines[o])){
                    OG_ChangeCursor(v, lines[o]->type ? MOUSE_CURSOR_RESIZE_EW:MOUSE_CURSOR_RESIZE_NS);
                    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
                        if (activeTitleBlock->selectedRect != tbr)
                            activeTitleBlock->selectedRect = tbr;

                        targetLine = lines[o];
                        currentState = MOVING_LINE_STATE;
                        if (targetLine->type)
                            targetDelta = GetTitleBlockLinePos(targetLine) - mousePos.x;
                        else targetDelta = GetTitleBlockLinePos(targetLine) - mousePos.y;
                        return;
                    }
                }
            }

        }
    }
}

static void MovingLineState(OG_Viewport *v){
    Vector2 p = OG_GetMouseViewportPosition(v);
    TitleBlockLine *collapseWith = NULL;

    switch (targetLine->type){
        case 1:{ // Horizontal
            OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_EW);
            p.x += targetDelta;
            float A = -targetLine->titleBlock->size.x/2;
            float B = targetLine->titleBlock->size.x/2;
            float t = (p.x - A) / (B - A);

            float lowDelT = 0.0f;
            float hightDelT = 1.0f;
            TitleBlockLine *lowDelL = NULL;
            TitleBlockLine *hightDelL = NULL;

            float minT = GetRatio(10, targetLine->titleBlock->size.x);
            float maxT = GetRatio(targetLine->titleBlock->size.x - 10, targetLine->titleBlock->size.x);
            float myPos = GetTitleBlockLinePos(targetLine);

            // Getting neighboring lines
            for (TitleBlockLine *l = targetLine->titleBlock->lines; l != NULL; l = l->next){
                if (l == targetLine) continue;
                if (l->type != targetLine->type) continue;
                
                bool nF = false;
                for (TitleBlockRect *r = l->titleBlock->rects; r != NULL; r = r->next){
                    int c = 0;
                    if (r->lines.top == targetLine || r->lines.top == l) c++;
                    if (r->lines.bottom == targetLine || r->lines.bottom == l) c++;
                    if (r->lines.left == targetLine || r->lines.left == l) c++;
                    if (r->lines.right == targetLine || r->lines.right == l) c++;

                    if (c >= 2) nF = true;
                }

                if (!nF) continue;

                float pos = GetTitleBlockLinePos(l);
                if (pos < myPos){
                    float limit = GetRatio((pos + 10.0f) + targetLine->titleBlock->size.x/2, targetLine->titleBlock->size.x);
                    if (limit >= minT){
                        minT = limit;
                        lowDelT = GetRatio((pos + 1.0f) + targetLine->titleBlock->size.x/2, targetLine->titleBlock->size.x);
                        lowDelL = l;
                    }
                }

                if (pos > myPos){
                    float limit = GetRatio((pos - 10.0f) + targetLine->titleBlock->size.x/2, targetLine->titleBlock->size.x);
                    if (limit <= maxT){
                        maxT = limit;
                        hightDelT = GetRatio((pos - 0.5f) + targetLine->titleBlock->size.x/2, targetLine->titleBlock->size.x);
                        hightDelL = l;
                    }
                }
            }

            if (t <= lowDelT){
                collapseWith = lowDelL;
                t = lowDelT;
            }
            else if (t < minT) t = minT;
            
            if (t >= hightDelT){
                collapseWith = hightDelL;
                t = hightDelT;
            }
            else if (t > maxT) t = maxT;
                
            targetLine->t = t;
        } break;

        case 0:{ // Vertical
            OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_NS);
            p.y += targetDelta;
            float A = -targetLine->titleBlock->size.y/2;
            float B = targetLine->titleBlock->size.y/2;
            float t = (p.y - A) / (B - A);

            float lowDelT = 0.0f;
            float hightDelT = 1.0f;
            TitleBlockLine *lowDelL = NULL;
            TitleBlockLine *hightDelL = NULL;

            float minT = GetRatio(10, targetLine->titleBlock->size.y);
            float maxT = GetRatio(targetLine->titleBlock->size.y - 10, targetLine->titleBlock->size.y);
            float myPos = GetTitleBlockLinePos(targetLine);

            // Getting neighboring lines
            for (TitleBlockLine *l = targetLine->titleBlock->lines; l != NULL; l = l->next){
                if (l == targetLine) continue;
                if (l->type != targetLine->type) continue;

                bool nF = false;
                for (TitleBlockRect *r = l->titleBlock->rects; r != NULL; r = r->next){
                    int c = 0;
                    if (r->lines.top == targetLine || r->lines.top == l) c++;
                    if (r->lines.bottom == targetLine || r->lines.bottom == l) c++;
                    if (r->lines.left == targetLine || r->lines.left == l) c++;
                    if (r->lines.right == targetLine || r->lines.right == l) c++;

                    if (c >= 2) nF = true;
                }
                
                if (!nF) continue;

                float pos = GetTitleBlockLinePos(l);
                if (pos < myPos){
                    float limit = GetRatio((pos + 10.0f) + targetLine->titleBlock->size.y/2, targetLine->titleBlock->size.y);
                    if (limit >= minT){
                        minT = limit;
                        lowDelT = GetRatio((pos + 1.0f) + targetLine->titleBlock->size.y/2, targetLine->titleBlock->size.y);
                        lowDelL = l;
                    }   
                }

                if (pos > myPos){
                    float limit = GetRatio((pos - 10.0f) + targetLine->titleBlock->size.y/2, targetLine->titleBlock->size.y);
                    if (limit <= maxT){
                        maxT = limit;
                        hightDelT = GetRatio((pos - 1.0f) + targetLine->titleBlock->size.y/2, targetLine->titleBlock->size.y);
                        hightDelL = l;
                    }
                }
            }

            if (t <= lowDelT){
                collapseWith = lowDelL;
                t = lowDelT;
            }
            else if (t < minT) t = minT;
            
            if (t >= hightDelT){
                collapseWith = hightDelL;
                t = hightDelT;
            }
            else if (t > maxT) t = maxT;

            targetLine->t = t;
        } break;
    }

    if (OG_IsMouseButtonReleased(v, MOUSE_LEFT_BUTTON)){
        if (collapseWith){
            activeTitleBlock->selectedRect = NULL;
            CollapseTitleBlockLines(targetLine, collapseWith);
        }
            
        currentState = IDLE_STATE;
        return;
    }
}

static void CanvasUpdate(OG_Viewport *v){
    switch (currentState){
        case IDLE_STATE:        IdleState(v);       break;
        case MOVING_LINE_STATE: MovingLineState(v); break;
    }
}

static void CanvasRenderOverlay(OG_Viewport *v){
    if (!activeTitleBlock) return;
    DrawTitleBlock(activeTitleBlock, v, Vector2Zero(), WHITE, false, NULL);
    DrawTitleBlockRect(activeTitleBlock->selectedRect, v, Vector2Zero(), HIGHLIGHT_C);
}

/* <== Inspector Section =======================================> */

static void TitleBoxPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!activeTitleBlock) return;
    if (!mu_header_ex(ctx, "TitleBoxProperties", MU_OPT_EXPANDED)) return;
    
    mu_equal_layout_l_auto(ctx, (const char *[]){"W:", "H:", 0}, 0);
    mu_label(ctx, "W:");
    mu_number_custom(
        ctx, &activeTitleBlock->size.x, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

    mu_label(ctx, "H:");
    mu_number_custom(
        ctx, &activeTitleBlock->size.y, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );
    
}

static void TitleBoxRectPropertiesSection(OG_Viewport *v, mu_Context *ctx){
    if (!activeTitleBlock) return;
    if (!activeTitleBlock->selectedRect) return;
    if (!mu_header_ex(ctx, "RectProperties", MU_OPT_EXPANDED)) return;
    
    // Label
    mu_equal_layout_l_auto(ctx, (const char *[]){"Label:", 0}, 0);
    mu_label(ctx, "Label:");
    mu_Rect next = mu_layout_next(ctx);
    mu_Id label_id = mu_get_id(ctx, "label_control", 13);
    mu_update_control(ctx, label_id, next, 0);
    mu_draw_control_frame(ctx, label_id, next, MU_COLOR_BASE, 0);
    mu_draw_text(
        ctx, 
        ctx->style->font, 
        activeTitleBlock->selectedRect->label, -1, 
        mu_vec2(
            next.x + mu_min(
                next.w - ctx->style->padding - ctx->text_width(ctx->style->font, activeTitleBlock->selectedRect->label, -1) - 1, 
                ctx->style->padding
            ), 
            next.y + (next.h - ctx->text_height(ctx->style->font)) / 2
        ), 
        ctx->style->colors[MU_COLOR_TEXT]
    );
    
    if (ctx->focus == label_id && ctx->mouse_pressed)
        OG_OpenInputDialog(&UpdateRectLabelName, NULL);

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "No label:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "TitleBoxRectPropertiesSection_noLabel", 37);
    mu_checkbox(ctx, "", &activeTitleBlock->selectedRect->noLabel);
    mu_pop_id(ctx);

    // Content
    if (!activeTitleBlock->selectedRect->externalContent){
        mu_layout_row(ctx, 1, (const int[]){-1}, 0);    
        mu_label(ctx, "Content:");
        mu_layout_row(ctx, 1, (const int[]){-1}, 100);
        mu_infinite_input(ctx, "TitleBoxRectPropertiesSection_Text", &activeTitleBlock->selectedRect->content);
    }

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "External Content:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "TitleBoxRectPropertiesSection_externalContent", 45);
    if (mu_checkbox(ctx, "", &activeTitleBlock->selectedRect->externalContent)){
        if (activeTitleBlock->selectedRect->externalContent){
            for (Page *p = mainDocument.pages; p != NULL; p = p->next){
                for (PageEntity *pe = p->entities; pe != NULL; pe = pe->next){
                    if (pe->type != PAGE_ENTITY_TITLEBLOCK) continue;
                    if (pe->block->block != activeTitleBlock) continue;
                    AddTitleBlockEntityProp(pe->block, activeTitleBlock->selectedRect);
                }
            }
        }
            
        else {
            for (Page *p = mainDocument.pages; p != NULL; p = p->next){
                for (PageEntity *pe = p->entities; pe != NULL; pe = pe->next){
                    if (pe->type != PAGE_ENTITY_TITLEBLOCK) continue;
                    if (pe->block->block != activeTitleBlock) continue;
                    for (PageEntityTitleBlockProp *pr = pe->block->properties; pr != NULL;){
                        if (pr->r == activeTitleBlock->selectedRect){
                            PageEntityTitleBlockProp *toDel = pr;
                            pr = toDel->next;
                            DeleteTitleBlockEntityProp(toDel);
                            continue;
                        }
                        
                        pr = pr->next;
                    }
                }
            }
        }
            
    }
    mu_pop_id(ctx);

    // TextSize
    mu_equal_layout_l_auto(ctx, (const char *[]){"TextSize:", 0}, 0);
    mu_label(ctx, "TextSize:");
    mu_number_custom(
        ctx, &activeTitleBlock->selectedRect->textSize, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

    // TextPosition
    mu_layout_row(ctx, 3, (const int[]){-130, -25, -1}, 0);
    mu_space(ctx);
    mu_label(ctx, "TextAtBottom:");
    int bottom = activeTitleBlock->selectedRect->textPos == TBRTP_BOTTOM;
    mu_push_id(ctx, "TitleBoxRectProperties_TextAtBottom", 35);
    if(mu_checkbox(ctx, "", &bottom))
        activeTitleBlock->selectedRect->textPos = TBRTP_BOTTOM;
    mu_pop_id(ctx);

    mu_layout_row(ctx, 3, (const int[]){-108, -25, -1}, 0);
    mu_space(ctx);
    mu_label(ctx, "TextAtTop:");
    int top = activeTitleBlock->selectedRect->textPos == TBRTP_TOP;
    mu_push_id(ctx, "TitleBoxRectProperties_TextAtTop", 32);
    if(mu_checkbox(ctx, "", &top))
        activeTitleBlock->selectedRect->textPos = TBRTP_TOP;
    mu_pop_id(ctx);

}

static void InspectorUI(OG_Viewport *v, mu_Context *ctx){
    TitleBoxPropertiesSection(v, ctx);
    TitleBoxRectPropertiesSection(v, ctx);
}

/* <== Tree Section ============================================> */

static void TreeInit(OG_Viewport *v){
    v->renderAlways = true;
    v->updateAlways = true;
}

static void TreeUpdate(OG_Viewport *v){
    if (OG_IsMouseButtonReleased(v, MOUSE_RIGHT_BUTTON)){
        OpenTitleBlockEditorContextMenu();
    }
    
    if (v->hidden) return;
    if (titleBlocksQ <= 0) return;

    // Select a TitleBox
    float verticalOffset = 0;
    for (int i=0; i<titleBlocksQ; i++){
        Rectangle r = {
            0, scroll+verticalOffset, 
            v->size.width, 
            fontSize
        };

        if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                activeTitleBlock = titleBlocks[i];
                OpenTitleBlockContextMenu();
                return;
            }
        }

        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                activeTitleBlock = titleBlocks[i];
            }
        }

        verticalOffset += fontSize;
    }

    CalcScrollbar(v, titleBlocksQ);
}

static void TreeRenderOverlay(OG_Viewport *v){
    if (v->hidden) return;
    float verticalOffset = 0;
    for (int i=0; i<titleBlocksQ; i++){
        Color color = OG_TEXT_C;
        if (titleBlocks[i] == activeTitleBlock)
            color = HIGHLIGHT_C;

        char *filename = NULL;
        if (titleBlocks[i]->sourcePath)
            filename = GetFileName(titleBlocks[i]->sourcePath);

        DrawTextEx(
            OG.defaultFont, 
            filename ? filename:"[UNSAVED TITLEBOX]",
            (Vector2){
                leftPadding,
                scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            color
        );
        verticalOffset += fontSize;
    }

    DrawScrollBar(v);
}

void TitleBlockEditor(){
    const int width = 1000;
    const int height = 600;

    OG_Layout *layout = OG_InitLayout(
        "TitleBlockEditor", 
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
    
    // Containers
    OG_LayoutContainer *treeContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        &layout->lines[OG_LAYOUT_LINE_LEFT], 
        leftL
    );
    
    OG_LayoutContainer *canvasContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        leftL,
        rightL
    );

    OG_LayoutContainer *inspectorContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        rightL,
        &layout->lines[OG_LAYOUT_LINE_RIGHT]
    );

    // Viewports
    OG_LayoutAddViewport(
        treeContainer, 
        OG_InitViewport(
            "TitleBlockEditor_Tree", 
            (Rectangle){}, 
            1.0f, 1.0f, 
            true, false, false, 
            &TreeInit,
            &TreeUpdate,
            NULL,
            NULL,
            &TreeRenderOverlay,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        )
    );

    OG_LayoutAddViewport(
        canvasContainer, 
        OG_InitViewport(
            "TitleBlockEditor_Canvas", 
            (Rectangle){}, 
            1.0f, 5.0f, 
            true, false, false, 
            &CanvasInit, 
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
        inspectorContainer, 
        OG_InitViewport(
            "TitleBlockEditor_Inspector", 
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
            &InspectorUI, 
            NULL, 
            NULL
        )
    );

}