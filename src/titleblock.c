// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include "titleblock.h"
#include "caddraw.h"
#include "documentation.h"
#include "main.h"
#include "origami.h"
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int GetLineDependencies(TitleBlockLine *l, TitleBlockRect **rects){
    int dependenciesQ = 0;
    for (TitleBlockRect *tbr = l->titleBlock->rects; tbr != NULL; tbr = tbr->next){
        if (tbr->lines.top == l){
            if (rects)
                rects[dependenciesQ] = tbr;
            dependenciesQ++;
            continue;
        }

        if (tbr->lines.bottom == l){
            if (rects)
                rects[dependenciesQ] = tbr;
            dependenciesQ++;
            continue;
        }

        if (tbr->lines.left == l){
            if (rects)
                rects[dependenciesQ] = tbr;
            dependenciesQ++;
            continue;
        }

        if (tbr->lines.right == l){
            if (rects)
                rects[dependenciesQ] = tbr;
            dependenciesQ++;
            continue;
        }
    }
    
    return dependenciesQ;
}

static bool IsBaseLine(TitleBlockLine *l){
    TitleBlockLine *ll = l->titleBlock->lines;

    for (int i = 0; i < 4 && ll; i++){
        if (l == ll)
            return true;
        ll = ll->next;
    }

    return false;
}

static void DeleteRect(TitleBlockRect *r){
    if (!r) return;
    
    for (Page *p=mainDocument.pages; p != NULL; p = p->next){
        for (PageEntity *e = p->entities; e != NULL; e = e->next){
            if (e->type != PAGE_ENTITY_TITLEBLOCK) continue;
            if (e->block->block != r->titleBlock) continue;
            for (PageEntityTitleBlockProp *pr = e->block->properties; pr != NULL;){
                if (pr->r == r){
                    PageEntityTitleBlockProp *toDel = pr;
                    pr = toDel->next;
                    DeleteTitleBlockEntityProp(toDel);
                    continue;
                }
                
                pr = pr->next;
            }
        }
    }

    if (r->label) free(r->label);
    if (r->content) free(r->content);
    r->label = NULL;
    r->content = NULL;
    for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
        if (rr->next == r){
            rr->next = r->next;
            break;
        }
    }

    if (r->titleBlock->rects == r)
        r->titleBlock->rects = r->next;
    
    free(r);
}

static void DeleteLine(TitleBlockLine *l){
    if (!l) return;
    
    for (TitleBlockLine *ll = l->titleBlock->lines; ll != NULL; ll = ll->next){
        if (ll->next == l){
            ll->next = l->next;
            break;
        }
    }

    if (l->titleBlock->lines == l)
        l->titleBlock->lines = l->next;

    free(l);
}

TitleBlockLine *AddTitleBlockLine(TitleBlock *tb, int type, float t){
    TitleBlockLine *l = calloc(1, sizeof(TitleBlockLine));
    l->titleBlock = tb;
    l->type = type;
    l->t = t;
    
    if (tb->lines == NULL){
        tb->lines = l;
        return l;
    }

    // link the list
    for (TitleBlockLine *ll = tb->lines; ll != NULL; ll = ll->next){
        if (ll->next == NULL){
            ll->next = l;
            break;
        }
    }

    return l;
}

TitleBlockRect *AddTitleBlockRect(
    TitleBlock *tb, 
    TitleBlockLine *left, TitleBlockLine*right, 
    TitleBlockLine *top, TitleBlockLine *bottom 
    ){
        TitleBlockRect *r = calloc(1, sizeof(TitleBlockRect));
        r->titleBlock = tb;
        r->lines.left = left;
        r->lines.right = right;
        r->lines.top = top;
        r->lines.bottom = bottom;

        r->textSize = 10.0f;
        r->content = calloc(1, 1);
        *r->content = '\0';

        char newLabel[32] = {0};
        for (int i=0;; i++){
            sprintf(newLabel, "Rect%i", i);
            if (SetTitleBlockLabel(r, newLabel))
                break;
        }

        // link the list
        if (tb->rects == NULL){
            tb->rects = r;
            return r;
        }

        for (TitleBlockRect *rr = tb->rects; rr != NULL; rr = rr->next){
            if (rr->next == NULL){
                rr->next = r;
                break;
            }
        }

        return r;
}

TitleBlock *NewTitleBlock(Vector2 *size){
    TitleBlock *tb = calloc(1, sizeof(TitleBlock));
    tb->size = (Vector2){DEFAULT_TITLE_BLOCK_W, DEFAULT_TITLE_BLOCK_H};
    if (size) tb->size = *size;

    // base lines
    TitleBlockLine *top = AddTitleBlockLine(tb, 0, 0.0f);
    TitleBlockLine *bottom = AddTitleBlockLine(tb, 0, 1.0f);
    TitleBlockLine *left = AddTitleBlockLine(tb, 1, 0.0f);
    TitleBlockLine *right = AddTitleBlockLine(tb, 1, 1.0f);

    // base rect
    AddTitleBlockRect(
        tb, 
        left, right, 
        top, bottom
    );

    return tb;
}

void FreeTitleBlock(TitleBlock *tb){
    if (!tb) return;

    for (Page *p=mainDocument.pages; p != NULL; p=p->next){
        for (PageEntity *pe = p->entities; pe != NULL;){
            if (pe->type == PAGE_ENTITY_TITLEBLOCK && pe->block->block == tb){
                PageEntity *toDel = pe;
                pe = pe->next;
                DeleteEntity(toDel);
                continue;
            }
                
            pe = pe->next;
        }
    }
    
    if (tb->rects){
        while(tb->rects->next){
            TitleBlockRect *next = tb->rects->next->next;
            if (tb->rects->next->label) free(tb->rects->next->label);
            if (tb->rects->next->content) free(tb->rects->next->content);
            free(tb->rects->next);
            tb->rects->next = next;
        }

        free(tb->rects);
    }

    if (tb->lines){
        while(tb->lines->next){
            TitleBlockLine *next = tb->lines->next->next;
            free(tb->lines->next);
            tb->lines->next = next;
        }
        
        free(tb->lines);
    }

    if (tb->sourcePath)
        free(tb->sourcePath);
    
    free(tb);
}

void AddTitleBlockDivisions(TitleBlockRect *r, int type, int q, TitleBlockRect **out){
    if (!r) return;
    
    switch (type){
        case 0:{ //HORIZONTAL
            float A = r->lines.top->t;
            float B = r->lines.bottom->t;
            TitleBlockLine *tbl[q+1];
            tbl[0] = r->lines.top;

            int outI = 0;
            for (int i=1; i<q+1; i++){
                float t = (float)i / (q + 1);
                tbl[i] = AddTitleBlockLine(
                    r->titleBlock, 0, 
                    Lerp(A, B, t)
                );

                TitleBlockRect *rr;
                rr = AddTitleBlockRect(
                    r->titleBlock, 
                    r->lines.left, 
                    r->lines.right, 
                    tbl[i-1], 
                    tbl[i]
                );

                if (out)
                    out[outI++] = rr;
            }

            r->lines.top = tbl[q];

        } break;
        
        case 1:{ //VERTICAL
            float A = r->lines.left->t;
            float B = r->lines.right->t;
            TitleBlockLine *tbl[q+1];
            tbl[0] = r->lines.left;

            int outI = 0;
            for (int i=1; i<q+1; i++){
                float t = (float)i / (q + 1);
                tbl[i] = AddTitleBlockLine(
                    r->titleBlock, 1, 
                    Lerp(A, B, t)
                );

                TitleBlockRect *rr;
                rr = AddTitleBlockRect(
                    r->titleBlock, 
                    tbl[i-1], 
                    tbl[i], 
                    r->lines.top, 
                    r->lines.bottom
                );
                
                if (out)
                    out[outI++] = rr;
            }

            r->lines.left = tbl[q];

        } break;
    }
    
}

void CollapseTitleBlockLines(TitleBlockLine *A, TitleBlockLine *B){
    if (!A || !B) return;
    if (A->type != B->type) return;
    
    int aDLen = GetLineDependencies(A, NULL);
    TitleBlockRect *aD[aDLen];
    GetLineDependencies(A, aD);

    int bDLen = GetLineDependencies(B, NULL);
    TitleBlockRect *bD[bDLen];
    GetLineDependencies(B, bD);


    TitleBlockRect *common[(aDLen < bDLen) ? aDLen : bDLen];
    int commonLen = 0;

    for (int i = 0; i < aDLen; i++) {
        for (int j = 0; j < bDLen; j++) {
            if (aD[i] == bD[j]) {
                common[commonLen++] = aD[i];
                break;
            }
        }
    }

    for (int i=0; i<aDLen; i++){
        if (aD[i]->lines.top == A) aD[i]->lines.top = B;
        if (aD[i]->lines.bottom == A) aD[i]->lines.bottom = B;
        if (aD[i]->lines.left == A) aD[i]->lines.left = B;
        if (aD[i]->lines.right == A) aD[i]->lines.right = B;
    }

    for (int i=0; i<commonLen; i++)
        DeleteRect(common[i]);
    
    DeleteLine(A);
    
}

float GetTitleBlockLinePos(TitleBlockLine *l){
    float linePos = Lerp(
        !l->type ? -l->titleBlock->size.y/2 : -l->titleBlock->size.x/2, 
        !l->type ? l->titleBlock->size.y/2 : l->titleBlock->size.x/2, 
        l->t
    );

    return linePos;
}

void GetNeighboringLines(TitleBlockLine *l, TitleBlockRect *r, TitleBlockLine **out){
    if (!l || !out) return;
    
    // ================== VERTICAL ================== //    
    const int TOP_LIMIT = 0;
    const int BOTTOM_LIMIT = 1;

    if (l == r->lines.top){
        out[BOTTOM_LIMIT] = r->lines.bottom;
        
        float limit = 1.0f;
        for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
            if (rr->lines.bottom != l) continue;
            if (rr->lines.top->t < limit){
                limit = rr->lines.top->t;
                out[TOP_LIMIT] = rr->lines.top;
            }
        }
    }

    if (l == r->lines.bottom){
        out[TOP_LIMIT] = r->lines.top;

        float limit = 0.0f;
        for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
            if (rr->lines.top != l) continue;
            if (rr->lines.bottom->t > limit){
                limit = rr->lines.bottom->t;
                out[BOTTOM_LIMIT] = rr->lines.bottom;
            }

        }
    }

    // ================= HORIZONTAL ================= //
    const int LEFT_LIMIT = 0;
    const int RIGHT_LIMIT = 1;

    if (l == r->lines.left){
        out[RIGHT_LIMIT] = r->lines.right;

        float limit = 1.0f;
        for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
            if (rr->lines.right != l) continue;
            if (rr->lines.left->t < limit){
                limit = rr->lines.left->t;
                out[LEFT_LIMIT] = rr->lines.left;
            }
        }
    }

    if (l == r->lines.right){
        out[LEFT_LIMIT] = r->lines.left;

        float limit = 0.0f;
        for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
            if (rr->lines.left != l) continue;
            if (rr->lines.right->t > limit){
                limit = rr->lines.right->t;
                out[RIGHT_LIMIT] = rr->lines.right;
            }
        }
    }

}

bool IsPInsideTitleBlockRect(Vector2 p, TitleBlockRect *r){
    Vector2 pts[] = {
        (Vector2){
            Lerp(-r->titleBlock->size.x/2, r->titleBlock->size.x/2, r->lines.left->t),
            Lerp(-r->titleBlock->size.y/2, r->titleBlock->size.y/2, r->lines.top->t)    
        },

        (Vector2){
            Lerp(-r->titleBlock->size.x/2, r->titleBlock->size.x/2, r->lines.right->t),
            Lerp(-r->titleBlock->size.y/2, r->titleBlock->size.y/2, r->lines.top->t)    
        },

        (Vector2){
            Lerp(-r->titleBlock->size.x/2, r->titleBlock->size.x/2, r->lines.left->t),
            Lerp(-r->titleBlock->size.y/2, r->titleBlock->size.y/2, r->lines.bottom->t)    
        },

        (Vector2){
            Lerp(-r->titleBlock->size.x/2, r->titleBlock->size.x/2, r->lines.right->t),
            Lerp(-r->titleBlock->size.y/2, r->titleBlock->size.y/2, r->lines.bottom->t)    
        }
    };
    
    Rectangle rect = {
        pts[0].x, pts[0].y,
        Vector2Distance(pts[0], pts[1]),
        Vector2Distance(pts[0], pts[2])
    };

    if (CheckCollisionPointRec(p, rect))
        return true;
    return false;
}

bool IsPInsideTitleBlockLine(Vector2 p, TitleBlockLine *l){
    switch (l->type){
        // Horizontal
        case 0:{
            float lineY = GetTitleBlockLinePos(l);
            if (p.y >= lineY-5 && p.y <= lineY+5)
                return true;
        } break;
        
        // Vertical
        case 1:{
            float lineX = GetTitleBlockLinePos(l);
            if (p.x >= lineX-5 && p.x <= lineX+5)
                return true;
            
        } break;
    }

    return false;
}

bool SetTitleBlockLabel(TitleBlockRect *r, char *label){
    char newLabel[strlen(label)+2];
    sprintf(newLabel, "%s:", label);
    
    for (TitleBlockRect *rr = r->titleBlock->rects; rr != NULL; rr = rr->next){
        if (strcmp(newLabel, rr->label) == 0)
            return false;
    }

    if (r->label) free(r->label);
    r->label = calloc(strlen(newLabel)+1, sizeof(char));
    strcpy(r->label, newLabel);
    return true;
}

Rectangle DrawTitleBlockRect(TitleBlockRect *tbr, OG_Viewport *viewport, Vector2 offset, Color c){
    if (!tbr) return (Rectangle){0,0,0,0};
    TitleBlock *tb = tbr->titleBlock;
    
    Vector2 topLeftCorner = GetWorldToScreen2D((Vector2){
        Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.left->t) + offset.x,
        Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.top->t) + offset.y
    }, viewport->camera);

    Vector2 topRightCorner = GetWorldToScreen2D((Vector2){
        Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.right->t) + offset.x,
        Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.top->t) + offset.y
    }, viewport->camera);

    DrawCadLine( // top line
        topLeftCorner,
        topRightCorner, 
        1.0f, 
        c
    );

    Vector2 bottomLeftCorner = GetWorldToScreen2D(
        (Vector2){
            Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.left->t) + offset.x,
            Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.bottom->t) + offset.y
        }, 
        viewport->camera
    );

    Vector2 bottomRightCorner = GetWorldToScreen2D((Vector2){
        Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.right->t) + offset.x,
        Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.bottom->t) + offset.y
    }, viewport->camera);

    DrawCadLine( // bottom line
        bottomLeftCorner,
        bottomRightCorner, 
        1.0f, 
        c
    );

    DrawCadLine( // left line
        topLeftCorner, 
        bottomLeftCorner, 
        1.0f, 
        c
    );

    DrawCadLine( // right line
        topRightCorner, 
        bottomRightCorner, 
        1.0f, 
        c
    );

    return (Rectangle){
        topLeftCorner.x, 
        topLeftCorner.y, 
        Vector2Distance(topLeftCorner, topRightCorner),
        Vector2Distance(topLeftCorner, bottomLeftCorner)
    };
}

void DrawTitleBlock(TitleBlock *tb, OG_Viewport *viewport, Vector2 offset, Color c, bool noRepeat, PageEntityTitleBlock *entity){
    if(!tb) return;
    
    struct line {
        TitleBlockLine *l;
        bool used;
        Vector2 A;
        Vector2 B;
    };

    size_t linesQ = 0;
    for (TitleBlockLine *l = tb->lines; l != NULL; l=l->next){
        linesQ++;
    }
    
    size_t i=0;
    struct line lines[linesQ];
    if (noRepeat){
        for (TitleBlockLine *l = tb->lines; l != NULL; l=l->next){
            lines[i].used = false;
            lines[i].A = Vector2Zero();
            lines[i].B = Vector2Zero();
            lines[i++].l = l;
        }
    }
        
    OpenCadPath();
    for (TitleBlockRect *tbr = tb->rects; tbr != NULL; tbr = tbr->next){
        
        Rectangle r;
        if (noRepeat){
            Vector2 topLeftCorner = GetWorldToScreen2D((Vector2){
                Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.left->t) + offset.x,
                Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.top->t) + offset.y
            }, viewport->camera);

            Vector2 topRightCorner = GetWorldToScreen2D((Vector2){
                Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.right->t) + offset.x,
                Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.top->t) + offset.y
            }, viewport->camera);

            Vector2 bottomLeftCorner = GetWorldToScreen2D(
                (Vector2){
                    Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.left->t) + offset.x,
                    Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.bottom->t) + offset.y
                }, 
                viewport->camera
            );

            Vector2 bottomRightCorner = GetWorldToScreen2D((Vector2){
                Lerp(-tb->size.x/2, tb->size.x/2, tbr->lines.right->t) + offset.x,
                Lerp(-tb->size.y/2, tb->size.y/2, tbr->lines.bottom->t) + offset.y
            }, viewport->camera);

            for (size_t i=0; i<linesQ; i++){
                if (lines[i].l == tbr->lines.top){
                    if (lines[i].used){
                        if (topLeftCorner.x < lines[i].A.x) lines[i].A.x = topLeftCorner.x;
                        if (topRightCorner.x > lines[i].B.x) lines[i].B.x = topRightCorner.x;
                    }

                    else {
                        lines[i].used = true;
                        lines[i].A = topLeftCorner;
                        lines[i].B = topRightCorner;
                    }
                    
                    continue;
                }

                if (lines[i].l == tbr->lines.bottom){
                    if (lines[i].used){
                        if (bottomLeftCorner.x < lines[i].A.x) lines[i].A.x = bottomLeftCorner.x;
                        if (bottomRightCorner.x > lines[i].B.x) lines[i].B.x = bottomRightCorner.x;
                    }

                    else {
                        lines[i].used = true;
                        lines[i].A = bottomLeftCorner;
                        lines[i].B = bottomRightCorner;
                    }

                    continue;
                }

                if (lines[i].l == tbr->lines.left){
                    if (lines[i].used){
                        if (topLeftCorner.y < lines[i].A.y) lines[i].A.y = topLeftCorner.y;
                        if (bottomLeftCorner.y > lines[i].B.y) lines[i].B.y = bottomLeftCorner.y;
                    }

                    else {
                        lines[i].used = true;
                        lines[i].A = topLeftCorner;
                        lines[i].B = bottomLeftCorner;
                    }
                    
                    continue;
                }

                if (lines[i].l == tbr->lines.right){
                    float distance = Vector2Distance(topRightCorner, bottomRightCorner);
                    if (lines[i].used){
                        if (topRightCorner.y < lines[i].A.y) lines[i].A.y = topRightCorner.y;
                        if (bottomRightCorner.y > lines[i].B.y) lines[i].B.y = bottomRightCorner.y;
                    }

                    else {
                        lines[i].used = true;
                        lines[i].A = topRightCorner;
                        lines[i].B = bottomRightCorner;
                    }
                    
                    continue;
                }
            }

            r = (Rectangle){
                topLeftCorner.x, 
                topLeftCorner.y, 
                Vector2Distance(topLeftCorner, topRightCorner),
                Vector2Distance(topLeftCorner, bottomLeftCorner)
            };
        }

        else r = DrawTitleBlockRect(tbr, viewport, offset, c);
        SetCadScissors(r);
        
        Vector2 labelSize = MeasureCadText(
            cadFont, 
            tbr->label, 
            0, 
            tbr->textSize * viewport->camera.zoom, 
            0.0f, 
            1.0f
        );

        Vector2 contentSize;
        if (!tbr->externalContent){
            contentSize = MeasureCadText(
                cadFont, 
                tbr->content, 
                0, 
                tbr->textSize * viewport->camera.zoom, 
                0.0f, 
                1.0f
            );
        }

        else if (entity){
            for (PageEntityTitleBlockProp *p=entity->properties; p != NULL; p = p->next){
                if (p->r == tbr){
                    contentSize = MeasureCadText(
                        cadFont, 
                        p->content, 
                        0, 
                        tbr->textSize * viewport->camera.zoom, 
                        0.0f, 
                        1.0f
                    );
                    break;
                }
            }
        }

        float extraTextHeight = 0.0f;
        for (char *c = tbr->content; *c != '\0'; c++)
            if (*c == '\n') extraTextHeight += tbr->textSize * viewport->camera.zoom;
        
        // Draw Label
        if (!tbr->noLabel){
            DrawCadText(
                cadFont, 
                tbr->label, 
                (Vector2){
                    r.x, 
                    r.y + ( tbr->textPos == TBRTP_BOTTOM ? (r.height-extraTextHeight):labelSize.y )
                }, 
                Vector2Zero(), 
                0.0f, 
                tbr->textSize * viewport->camera.zoom, 
                0.0f, 
                1.0f,
                CAD_ALIGN_LEFT,
                c
            );
        }
        
        // Draw content
        if (!tbr->externalContent){
            DrawCadText(
                cadFont, 
                tbr->content, 
                (Vector2){
                    r.x + (tbr->noLabel ? 0:labelSize.x), 
                    r.y + ( tbr->textPos == TBRTP_BOTTOM ? r.height:contentSize.y )
                }, 
                Vector2Zero(), 
                0.0f, 
                tbr->textSize * viewport->camera.zoom, 
                0.0f, 
                1.0f,
                CAD_ALIGN_LEFT,
                c
            );
        }

        // Draw external content
        else if (entity){
            for (PageEntityTitleBlockProp *p=entity->properties; p != NULL; p = p->next){
                if (p->r == tbr){
                    DrawCadText(
                        cadFont, 
                        p->content, 
                        (Vector2){
                            r.x + (tbr->noLabel ? 0:labelSize.x), 
                            r.y + ( tbr->textPos == TBRTP_BOTTOM ? r.height:contentSize.y )
                        }, 
                        Vector2Zero(), 
                        0.0f, 
                        tbr->textSize * viewport->camera.zoom, 
                        0.0f, 
                        1.0f,
                        CAD_ALIGN_LEFT,
                        c
                    );                    
                    break;
                }
            }
        }

        UnsetCadScissors();
    }

    if (noRepeat){
        for (size_t i=0; i<linesQ; i++){
            if (!lines[i].used) continue;
            DrawCadLine(lines[i].A, lines[i].B, 1.0f, c);
        }
    }

    CloseCadPath();
}