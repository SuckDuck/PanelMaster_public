// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include "caddraw.h"
#include "microui.h"
#include "origami.h"
#include "documentation.h"
#include "contextmenus.h"
#include "main.h"
#include "surfaces.h"
#include "titleblock.h"
#include "utils.h"

typedef enum State{
    IDLE_STATE,
    MOVING_ENTITY_STATE
} State;

const static float fontSize = 16;
const static float scrollBarWidth = 16;
const static float leftPadding = 10;

static State currentState;
static PageEntity *targetEntity;
static Vector2 targetDelta;

float contentSize;
float scroll;

static RenderTexture bgTex;

/* <== Utils ===================================================> */

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

/* <== Tree Section ============================================> */

static void TreeInit(OG_Viewport *v){
    v->updateAlways = true;    
}

static void TreeUpdate(OG_Viewport *v){
    if (v->hidden) return;
    
    bool over_entity = false;
    if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON) || OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
        float verticalOffset = 0;
        for (Page *p = mainDocument.pages; p != NULL; p = p->next){
            Rectangle r = {
                0,scroll+verticalOffset, 
                v->size.width, 
                fontSize
            };
            
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                SelectPage(p);
                goto out;
            }
            
            verticalOffset += fontSize;
            if (p == p->document->selectedPage){
                for (PageEntity *e = p->entities; e != NULL; e=e->next){
                    Rectangle r = {
                        0,scroll+verticalOffset, 
                        v->size.width, 
                        fontSize
                    };

                    if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                        SelectEntity(e);
                        over_entity = true;
                        goto out;
                    }       

                    verticalOffset += fontSize;
                }
            }
        }

    }
    out:
    CalcScrollbar(v, mainDocument.pagesQ);

    if (OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)){
        if (over_entity) OpenEntityContextMenu();
        else {
            OpenPageListContextMenu();
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

static void TreeRenderOverlay(OG_Viewport *v){
    float verticalOffset = 0;
    Vector2 depthSize = MeasureTextEx(OG.defaultFont, "├── ", fontSize, 0);
    for (Page *p = mainDocument.pages; p != NULL; p = p->next){
        Color color = OG_TEXT_C;
        if (p == p->document->selectedPage) 
            color = HIGHLIGHT_C;
        
        DrawTextEx(
            OG.defaultFont, 
            p->name, 
            (Vector2){
                leftPadding,
                scroll + verticalOffset
            }, 
            fontSize,
            2.0f,
            color
        );
        verticalOffset += fontSize;

        int i=0;
        if (p == p->document->selectedPage){
            for (PageEntity *e = p->entities; e != NULL; e=e->next){
                Color color = OG_TEXT_C;
                if (e == e->page->selectedEntity) 
                    color = HIGHLIGHT_C;
                
                // Draw boxing
                DrawTextEx(
                    OG.defaultFont, 
                    i == p->entitiesQ-1 ? "└── ":"├── ", 
                    (Vector2){
                        leftPadding,
                        scroll + verticalOffset
                    }, 
                    fontSize,
                    0,
                    OG_TEXT_C
                );

                // Draw entity name
                DrawTextEx(
                    OG.defaultFont, 
                    e->name, 
                    (Vector2){
                        leftPadding + depthSize.x,
                        scroll + verticalOffset
                    }, 
                    fontSize,
                    0,
                    color
                );

                verticalOffset += fontSize;
                i++;
            }
        }
    }
    
    DrawScrollBar(v);
}

/* <== Canvas Section ==========================================> */

static void CanavasInit(OG_Viewport *v){}

static void CanvasOnIdleState(OG_Viewport *v){
    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    OG_ViewportUpdateZoom(v);
    if (OG_IsMouseButtonDown(v, MOUSE_MIDDLE_BUTTON) || OG_IsKeyDown(v, KEY_SPACE)){
        OG_ChangeCursor(v, MOUSE_CURSOR_RESIZE_ALL);
        OG_ViewportUpdatePan(v);
        return;
    }

    // Grab an entity
    if ((OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON) || OG_IsMouseButtonPressed(v, MOUSE_RIGHT_BUTTON)) && mainDocument.selectedPage){
        
        // Ivert entities order
        PageEntity *stack[256];
        int count = 0;
        for (PageEntity *e = mainDocument.selectedPage->entities; e != NULL; e = e->next) {
            if (count < 256) 
                stack[count++] = e;
        }

        for (int i = count - 1; i >= 0; i--) {
            PageEntity *e = stack[i];
            
            switch (e->type){
                case PAGE_ENTITY_SURFACE:{
                    float w = e->surface->surface->size.x * e->surface->scale;
                    float h = e->surface->surface->size.y * e->surface->scale;

                    Rectangle r = {
                        e->pos.x - w/2, 
                        e->pos.y - h/2, 
                        w, 
                        h, 
                    };

                    if (CheckCollisionPointRec(mousePos, r)){
                        SelectEntity(e);
                        targetEntity = e;
                        targetDelta = (Vector2){
                            e->pos.x - mousePos.x,
                            e->pos.y - mousePos.y
                        };

                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON))
                            currentState = MOVING_ENTITY_STATE;
                        else OpenEntityContextMenu(); 
                        return;
                    }

                } break;

                case PAGE_ENTITY_TEXT:{
                    Vector2 mt = MeasureCadText(
                        cadFont, 
                        e->text->text, 
                        0,
                        e->text->fontSize, 
                        0.0f, 1.0f
                    );

                    Rectangle r = {
                        e->pos.x - mt.x/2, 
                        e->pos.y - mt.y/2, 
                        mt.x, 
                        mt.y, 
                    };

                    if (CheckCollisionPointRec(mousePos, r)){
                        SelectEntity(e);
                        targetEntity = e;
                        targetDelta = (Vector2){
                            e->pos.x - mousePos.x,
                            e->pos.y - mousePos.y
                        };

                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON))
                            currentState = MOVING_ENTITY_STATE;
                        else OpenEntityContextMenu(); 
                        return;
                    }
                } break;

                case PAGE_ENTITY_TITLEBLOCK:{
                    Rectangle r = {
                        e->pos.x - e->block->block->size.x/2, 
                        e->pos.y - e->block->block->size.y/2, 
                        e->block->block->size.x, 
                        e->block->block->size.y, 
                    };
                    
                    if (CheckCollisionPointRec(mousePos, r)){
                        SelectEntity(e);
                        targetEntity = e;
                        targetDelta = (Vector2){
                            e->pos.x - mousePos.x,
                            e->pos.y - mousePos.y
                        };

                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON))
                            currentState = MOVING_ENTITY_STATE;
                        else OpenEntityContextMenu(); 
                        return;
                    }
                } break;

                case PAGE_ENTITY_STICKER:{
                    Rectangle r = {
                        e->pos.x - e->sticker->sticker->img.width/2.0f,
                        e->pos.y - e->sticker->sticker->img.height/2.0f,
                        e->sticker->sticker->img.width,
                        e->sticker->sticker->img.height
                    };

                    if (CheckCollisionPointRec(mousePos, r)){
                        SelectEntity(e);
                        targetEntity = e;
                        targetDelta = (Vector2){
                            e->pos.x - mousePos.x,
                            e->pos.y - mousePos.y
                        };

                        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON))
                            currentState = MOVING_ENTITY_STATE;
                        else OpenEntityContextMenu(); 
                        return;
                    }
                } break;

            }
            
        }
    }
}

static void CanvasOnMovingEntityState(OG_Viewport *v){
    Vector2 mousePos = OG_GetMouseViewportPosition(v);
    targetEntity->pos = Vector2Add(mousePos, targetDelta);

    if (OG_IsMouseButtonUp(v, MOUSE_BUTTON_LEFT)){
        currentState = IDLE_STATE;
        return;
    }
}

static void CanvasUpdate(OG_Viewport *v){
    switch (currentState){
        case IDLE_STATE:           CanvasOnIdleState(v);          break;
        case MOVING_ENTITY_STATE:  CanvasOnMovingEntityState(v);  break;
    }
}

static void CanvasRender(OG_Viewport *v){
    if (!mainDocument.selectedPage) return;
    
    // -------------- RENDERS THE PAGE (the white rect)
    if (bgTex.texture.width != v->renderTexture.texture.width || bgTex.texture.height != v->renderTexture.texture.height){
        UnloadRenderTexture(bgTex);
        bgTex = LoadRenderTexture(v->renderTexture.texture.width, v->renderTexture.texture.height);
    }

    // leaves the original "viewport texture"
    EndMode2D();
    EndTextureMode();
    BeginTextureMode(bgTex);
    ClearBackground(OG_VIEWPORT_BG_C);
    BeginMode2D(v->camera);

    DrawRectanglePro(
        (Rectangle){
            -mainDocument.width/2.0f, 
            -mainDocument.height/2.0f, 
            mainDocument.width, 
            mainDocument.height, 
        },
        Vector2Zero(),
        0.0f,
        WHITE
    );

    // returns to the original "viewport texture"
    EndMode2D();
    EndTextureMode();
    BeginTextureMode(v->renderTexture);
    
    DrawTextureRec(
        bgTex.texture,
        (Rectangle){ 0, 0, bgTex.texture.width, -bgTex.texture.height },
        (Vector2){ 0, 0 },
        WHITE
    );

    BeginMode2D(v->camera);
    
}

static void DrawEntitySelectionRect(OG_Viewport *v, PageEntity *e, Vector2 size){
    Rectangle r = {
        e->pos.x - size.x/2, 
        e->pos.y - size.y/2, 
        size.x, 
        size.y, 
    };

    Vector2 tl = { r.x, r.y };
    Vector2 tr = { r.x + r.width, r.y };
    Vector2 br = { r.x + r.width, r.y + r.height };
    Vector2 bl = { r.x, r.y + r.height };

    DrawLineDashedEx(
        GetWorldToScreen2D(tl, v->camera), 
        GetWorldToScreen2D(tr, v->camera), 
        1.0f, 5, 5, 
        HIGHLIGHT_C
    );
    
    DrawLineDashedEx(
        GetWorldToScreen2D(tr, v->camera), 
        GetWorldToScreen2D(br, v->camera), 
        1.0f, 5, 5, 
        HIGHLIGHT_C
    );
    
    DrawLineDashedEx(
        GetWorldToScreen2D(br, v->camera), 
        GetWorldToScreen2D(bl, v->camera), 
        1.0f, 5, 5, 
        HIGHLIGHT_C
    );
    
    DrawLineDashedEx(
        GetWorldToScreen2D(bl, v->camera), 
        GetWorldToScreen2D(tl, v->camera), 
        1.0f, 5, 5, 
        HIGHLIGHT_C
    );
}

static void CanvasRenderOverlay(OG_Viewport *v){
    if (!mainDocument.selectedPage) return;
    
    for (PageEntity *e = mainDocument.selectedPage->entities; e != NULL; e = e->next){
        switch (e->type){
            case PAGE_ENTITY_SURFACE:{
                DrawSurfaceTree(
                    v, e->surface->surface->tree, 
                    e->surface->lod, 
                    &bgTex.texture, 
                    e->pos, 
                    e->surface->scale, 
                    BLACK, 
                    BLACK, 
                    HATCH_SPACE * v->camera.zoom,
                    false, false, false,
                    true,
                    e->surface->flags & PESF_DRAW_SKINS,
                    e->surface->flags & PESF_DRAW_CUTOUTS,
                    false
                );

                // Draw Annotations
                if (e->surface->flags & PESF_DRAW_ANNOTATIONS){
                    for (Annotation *ann = e->surface->surface->tree->annotations; ann != NULL; ann = ann->next){
                        if ((IsAnchorPartOfCutout(ann->A) || IsAnchorPartOfCutout(ann->B)) && !(e->surface->flags & PESF_DRAW_CUTOUTS)) continue;
                        if ((IsAnchorPartOfBoundary(ann->A) || IsAnchorPartOfBoundary(ann->B)) && !(e->surface->flags & PESF_DRAW_SKINS)) continue;
                        
                        DrawAnnotation(
                            v, ann, BLACK, 
                            e->surface->annotationsArrowSize,
                            e->surface->annotationsFontSize * /*just for visualization*/ v->camera.zoom,
                            e->surface->scale,
                            e->pos
                        );
                    }
                }

                // Selected Frame
                if (e == mainDocument.selectedPage->selectedEntity){
                    float w = e->surface->surface->size.x * e->surface->scale;
                    float h = e->surface->surface->size.y * e->surface->scale;
                    DrawEntitySelectionRect(v, e, (Vector2){w, h});

                }

            } break;

            case PAGE_ENTITY_TEXT:{
                Vector2 mt = MeasureCadText(
                    cadFont, 
                    e->text->text, 
                    0,
                    e->text->fontSize, 
                    0.0f, 1.0f
                );
                
                DrawCadText(
                    cadFont, 
                    e->text->text,
                    GetWorldToScreen2D(
                        e->pos, 
                        v->camera
                    ),
                    (Vector2){
                        mt.x/2 * v->camera.zoom, 
                        -mt.y/2 * v->camera.zoom
                    },
                    0.0f, 
                    e->text->fontSize * v->camera.zoom,
                    0.0f, 
                    1.0f,
                    e->text->alignment,
                    e->text->color
                );

                // Selected Frame
                if (e == mainDocument.selectedPage->selectedEntity){
                    DrawEntitySelectionRect(v, e, mt);
                }

            } break;

            case PAGE_ENTITY_TITLEBLOCK:{
                DrawTitleBlock(e->block->block, v, e->pos, BLACK, false, e->block);
                if (e == mainDocument.selectedPage->selectedEntity){
                    DrawEntitySelectionRect(v, e, e->block->block->size);
                }
            } break;

            case PAGE_ENTITY_STICKER:{
                DrawCadSticker(
                    e->sticker->sticker, 
                    GetWorldToScreen2D(e->pos, v->camera), 
                    e->sticker->scale * v->camera.zoom
                );

                if (e == mainDocument.selectedPage->selectedEntity){
                    DrawEntitySelectionRect(
                        v, e, 
                        (Vector2){
                            e->sticker->sticker->img.width * e->sticker->scale, 
                            e->sticker->sticker->img.height * e->sticker->scale
                        }
                    );
                }
            } break;
        }
    }

    if (mainDocument.selectedPage->drawFrame){
        Vector2 tlc = GetWorldToScreen2D(
            (Vector2){
                mainDocument.selectedPage->frame.left - mainDocument.width/2.0f,
                mainDocument.selectedPage->frame.top - mainDocument.height/2.0f
            }, 
            v->camera
        );
        
        Vector2 trc = GetWorldToScreen2D(
            (Vector2){
                mainDocument.width - mainDocument.selectedPage->frame.right - mainDocument.width/2.0f,
                mainDocument.selectedPage->frame.top - mainDocument.height/2.0f
            },
            v->camera
        );

        Vector2 blc = GetWorldToScreen2D(
            (Vector2){
                mainDocument.selectedPage->frame.left - mainDocument.width/2.0f,
                mainDocument.height - mainDocument.selectedPage->frame.bottom - mainDocument.height/2.0f
            },
            v->camera
        );

        Vector2 brc = GetWorldToScreen2D(
            (Vector2){
                mainDocument.width - mainDocument.selectedPage->frame.right - mainDocument.width/2.0f,
                mainDocument.height - mainDocument.selectedPage->frame.bottom - mainDocument.height/2.0f
            },
            v->camera
        );

        DrawCadLine(tlc, trc, 1.0f, BLACK);
        DrawCadLine(trc, brc, 1.0f, BLACK);
        DrawCadLine(brc, blc, 1.0f, BLACK);
        DrawCadLine(blc, tlc, 1.0f, BLACK);
    }

}

/* <== Inspector Section =======================================> */

static void DocumentProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Document Properties", MU_OPT_EXPANDED)) return;
    Page *p = mainDocument.selectedPage;

    // Page Size
    float widht = mainDocument.width;
    float height = mainDocument.height;

    mu_equal_layout_l_auto(ctx, (const char *[]){"W:", "H:", 0}, 0);
    mu_label(ctx, "W:");
    if (mu_number_custom(
        ctx, &widht, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    )){
        mainDocument.width = (int)widht;
    }

    mu_label(ctx, "H:");
    if (mu_number_custom(
        ctx, &height, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    )){
        mainDocument.height = (int)height;
    }
}

static void PageProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Page Properties", MU_OPT_EXPANDED)) return;
    
    if (mainDocument.selectedPage->drawFrame){
        mu_equal_layout_l_auto(ctx, (const char *[]){"BottomMargin:",0}, 0);
        
        // Left Margin
        mu_label(ctx, "LeftMargin:");
        mu_number_custom(
            ctx, 
            &mainDocument.selectedPage->frame.left, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );

        // Right Margin
        mu_label(ctx, "RightMargin:");
        mu_number_custom(
            ctx, 
            &mainDocument.selectedPage->frame.right, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );

        // Top Margin
        mu_label(ctx, "TopMargin:");
        mu_number_custom(
            ctx, 
            &mainDocument.selectedPage->frame.top, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );

        // Bottom Margin    
        mu_label(ctx, "BottomMargin:");
        mu_number_custom(
            ctx, 
            &mainDocument.selectedPage->frame.bottom, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );
    };

    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "Draw Frame:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);

    bool *id = &mainDocument.selectedPage->drawFrame;
    mu_push_id(ctx, &id, sizeof(id));
    mu_checkbox(ctx, "", (int*)(&mainDocument.selectedPage->drawFrame));
    mu_pop_id(ctx);
}   

static void EntityProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Entity Properties", MU_OPT_EXPANDED)) return;
    PageEntity *e = mainDocument.selectedPage->selectedEntity;
    
    mu_equal_layout_l_auto(ctx, (const char *[]){"X:", "Y:", 0}, 0);
    mu_label(ctx, "X:");
    mu_number_custom(
        ctx, &e->pos.x, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

    mu_label(ctx, "Y:");
    mu_number_custom(
        ctx, &e->pos.y, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

}

static void SurfaceEntityProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Surface Properties", MU_OPT_EXPANDED)) return;
    PageEntitySurface *ee = mainDocument.selectedPage->selectedEntity->surface;
    
    // LOD 
    float lod = ee->lod;
    mu_equal_layout_l_auto(ctx, (const char *[]){"LOD:", 0}, 0);
    mu_label(ctx, "LOD:");
    if (mu_slider_ex(ctx, &lod, 0, 2, 1, "%.0f", 0)){
        ee->lod = (int)lod;
    }

    // Scale
    mu_equal_layout_l_auto(ctx, (const char *[]){"Scale:", 0}, 0);
    mu_label(ctx, "Scale:");
    mu_number_custom(
        ctx, &ee->scale, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

    if (ee->flags & PESF_DRAW_ANNOTATIONS){
        
        // Annotations text size
        mu_equal_layout_l_auto(ctx, (const char *[]){"AnnFontSize:", 0}, 0);
        mu_label(ctx, "AnnFontSize:");
        mu_number_custom(
            ctx, &ee->annotationsFontSize, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );  

        // Annotations arrow size
        mu_equal_layout_l_auto(ctx, (const char *[]){"AnnArrowSize:", 0}, 0);
        mu_label(ctx, "AnnArrowSize:");
        mu_number_custom(
            ctx, &ee->annotationsArrowSize, 
            0, "%.2f", 
            MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
        );  
    }

    // DrawAnnotations
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "Draw Annotations:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "draw_annotations", 16);
    
    int drawAnnotations = ee->flags & PESF_DRAW_ANNOTATIONS;
    if (mu_checkbox(ctx, "", &drawAnnotations))
        ee->flags ^= PESF_DRAW_ANNOTATIONS;
    mu_pop_id(ctx);

    // DrawSkins
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "Draw Skins:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "draw_skins", 10);
    
    int drawSkins = ee->flags & PESF_DRAW_SKINS;
    if (mu_checkbox(ctx, "", &drawSkins))
        ee->flags ^= PESF_DRAW_SKINS;
    mu_pop_id(ctx);

    // DrawCutouts
    mu_layout_row(ctx, 2, (const int[]){-25, -1}, 0);
    mu_button_ex(ctx, "Draw Cutouts:", 0, MU_OPT_NOINTERACT | MU_OPT_NOFRAME | MU_OPT_ALIGNRIGHT);
    mu_push_id(ctx, "draw_cutouts", 12);
    
    int drawCutouts = ee->flags & PESF_DRAW_CUTOUTS;
    if (mu_checkbox(ctx, "", &drawCutouts))
        ee->flags ^= PESF_DRAW_CUTOUTS;
    mu_pop_id(ctx);

}

static void TextEntityProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Text Properties", MU_OPT_EXPANDED)) return;
    PageEntityText *et = mainDocument.selectedPage->selectedEntity->text;

    // Text Content
    mu_layout_row(ctx, 1, (const int[]){-1}, 0);    
    mu_label(ctx, "Text:");
    mu_layout_row(ctx, 1, (const int[]){-1}, 100);
    mu_infinite_input(ctx, "TitleBoxRectPropertiesSection_Text", &et->text);
    
    // Text Size
    mu_equal_layout_l_auto(ctx, (const char *[]){"Size:", 0}, 0);
    mu_label(ctx, "Size:");
    mu_number_custom(
        ctx, &et->fontSize, 
        0, "%.2f", 
        MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER
    );

    // Text Alignment
    mu_equal_layout_l_auto(ctx, (const char *[]){"Left:", "Center:", "Right:", 0}, 0);
    
    mu_label(ctx, "Left:");
    int left = et->alignment == CAD_ALIGN_LEFT;
    mu_push_id(ctx, "TextAlignment_Left", 18);
    if(mu_checkbox(ctx, "", &left))
        et->alignment = CAD_ALIGN_LEFT;
    mu_pop_id(ctx);

    int center = et->alignment == CAD_ALIGN_CENTER;
    mu_label(ctx, "Center:");
    mu_push_id(ctx, "TextAlignment_Center", 20);
    if(mu_checkbox(ctx, "", &center))
        et->alignment = CAD_ALIGN_CENTER;
    mu_pop_id(ctx);
    
    int right = et->alignment == CAD_ALIGN_RIGHT;
    mu_label(ctx, "Right:");
    mu_push_id(ctx, "TextAlignment_Right", 19);
    if(mu_checkbox(ctx, "", &right))
        et->alignment = CAD_ALIGN_RIGHT;
    mu_pop_id(ctx);

    // Text Color
    mu_equal_layout_l_auto(ctx, (const char *[]){"R:", "G:", "B:", 0}, 0);
    mu_label(ctx, "R:");
    float r = et->color.r;
    if (mu_slider_ex(ctx, &r, 0.0f, 255.0f, 1.0f, "%.0f", 0))
        et->color.r = (unsigned char)r;
    
    mu_label(ctx, "G:");
    float g = et->color.g;
    if (mu_slider_ex(ctx, &g, 0.0f, 255.0f, 1.0f, "%.0f", 0))
        et->color.g = (unsigned char)g; 

    mu_label(ctx, "B:");
    float b = et->color.b;
    if (mu_slider_ex(ctx, &b, 0.0f, 255.0f, 1.0f, "%.0f", 0))
        et->color.b = (unsigned char)b; 

}

static void TitleBlockEntityProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Title Block Properties", MU_OPT_EXPANDED)) return;
    PageEntityTitleBlock *etb = mainDocument.selectedPage->selectedEntity->block;

    mu_layout_row(ctx, 3, (const int[]){24, 24, 24}, 0);
    
    // Top Left
    if (mu_button(ctx, "┌")){
        etb->entity->pos = (Vector2){
            (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.left:0) - mainDocument.width/2.0f + etb->block->size.x/2,
            (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.top:0) - mainDocument.height/2.0f + etb->block->size.y/2
        };
    }
    
    // Top center
    mu_push_id(ctx, "topCenter", 9);
    if (mu_button(ctx, "-")){
        etb->entity->pos = (Vector2){
            0, (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.top:0) - mainDocument.height/2.0f + etb->block->size.y/2
        };
    }
    mu_pop_id(ctx);
    
    // Top Right
    if (mu_button(ctx, "┐")){
        etb->entity->pos = (Vector2){
            mainDocument.width - (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.right:0) - mainDocument.width/2.0f - etb->block->size.x/2,
            (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.top:0) - mainDocument.height/2.0f + etb->block->size.y/2
        };
    }

    
    // Bottom Left
    if (mu_button(ctx, "└")){
        etb->entity->pos = (Vector2){
            (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.left:0) - mainDocument.width/2.0f + etb->block->size.x/2,
            mainDocument.height - (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.bottom:0) - mainDocument.height/2.0f - etb->block->size.y/2
        };
    }
    
    // Bottom Center
    mu_push_id(ctx, "bottomCenter", 12);
    if (mu_button(ctx, "-")){
        etb->entity->pos = (Vector2){
            0, mainDocument.height - (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.bottom:0) - mainDocument.height/2.0f - etb->block->size.y/2
        };
    }
    mu_pop_id(ctx);
    
    // Bottom Right
    if(mu_button(ctx, "┘")){
        etb->entity->pos = (Vector2){
            mainDocument.width - (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.right:0) - mainDocument.width/2.0f - etb->block->size.x/2,
            mainDocument.height - (mainDocument.selectedPage->drawFrame ? mainDocument.selectedPage->frame.bottom:0) - mainDocument.height/2.0f - etb->block->size.y/2
        };
    }

    int propCnt = 0;
    char propId[9];
    for (PageEntityTitleBlockProp *p = etb->properties; p != NULL; p = p->next){
        mu_layout_row(ctx, 1, (const int[]){-1}, 0);
        mu_label(ctx, p->r->label);
        mu_layout_row(ctx, 1, (const int[]){-1}, 100);
        
        sprintf(propId, "prop_%03d", propCnt++);
        mu_infinite_input(ctx, propId, &p->content);
    }
}

static void StickerEntityProperties(OG_Viewport *v, mu_Context *ctx){
    if (!mu_header_ex(ctx, "Sticker Properties", MU_OPT_EXPANDED)) return;
    PageEntitySticker *es = mainDocument.selectedPage->selectedEntity->sticker;

    mu_equal_layout_l_auto(ctx, (const char *[]){"Scale:", 0}, 0);
    mu_label(ctx, "Scale:");
    mu_number_custom(ctx, &es->scale, 0.0f, "%.2f", MU_OPT_KEYBOARD_ONLY | MU_OPT_ALIGNCENTER);
}

static void InspectorUI(OG_Viewport *v, mu_Context *ctx){
    if (!mainDocument.selectedPage) return;
    DocumentProperties(v, ctx);
    PageProperties(v, ctx);
    if (mainDocument.selectedPage->selectedEntity){
        EntityProperties(v, ctx);
        switch (mainDocument.selectedPage->selectedEntity->type){
            case PAGE_ENTITY_SURFACE:    SurfaceEntityProperties(v, ctx);    break;
            case PAGE_ENTITY_TEXT:       TextEntityProperties(v, ctx);       break;
            case PAGE_ENTITY_TITLEBLOCK: TitleBlockEntityProperties(v, ctx); break;
            case PAGE_ENTITY_STICKER:    StickerEntityProperties(v, ctx);    break;
        }
    }
}

void DocumentEditor(){
    
    OG_Layout *layout = OG_InitLayout(
        "DocumentEditor", 
        (Rectangle){0, 20, 1200, 600}
    );

    // Lines
    OG_LayoutLine *leftL = OG_LayoutAddLine(layout, OG_LAYOUT_LINE_V, 0.2f);
    OG_LayoutLine *rightL = OG_LayoutAddLine(layout, OG_LAYOUT_LINE_V, 0.8f);

    // Containers
    OG_LayoutContainer *treeContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        &layout->lines[OG_LAYOUT_LINE_LEFT], 
        leftL
    );

    OG_LayoutContainer *canvasContaienr = OG_LayoutAddContainer(
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
            "DocumentEditor_Tree", 
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
        canvasContaienr, 
        OG_InitViewport(
            "DocumentEditor_Canvas", 
            (Rectangle){}, 
            0.5f, 10.0f, 
            true, false, false, 
            &CanavasInit, 
            &CanvasUpdate, 
            NULL, 
            &CanvasRender, 
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
            "DocumentEditor_Inspector", 
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