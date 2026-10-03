// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "documentation.h"
#include "caddraw.h"
#include "main.h"
#include "pdfgen.h"
#include "dxfout.h"
#include "surfaces.h"
#include "svg.h"
#include "titleblock.h"

Page *NewPage(Document *document, char *name){
    Page *p = calloc(1, sizeof(Page));
    document->pagesQ++;
    
    p->document = document;
    document->lastInsertedPage = p;
    document->selectedPage = p;

    p->frame.left   = 20.0f;
    p->frame.right  = 10.0f;
    p->frame.top    = 10.0f;
    p->frame.bottom = 10.0f;

    if (name){
        p->name = calloc(strlen(name)+1, sizeof(char));
        strcpy(p->name, name);
    }

    else{ 
        p->name = calloc(7+floor(log10(abs(document->pagesQ))), sizeof(char));
        sprintf(p->name, "page_%i", document->pagesQ);
    }
    
    if (!document->pages){
        document->pages = p;
        return p;
    }

    Page *pp = document->pages;
    while (true){
        if (!pp->next)
            break;
        pp = pp->next;
    }

    pp->next = p;
    return p;
}

void DeletePage(Page *p){
    p->selectedEntity = NULL;
    if (p->entities){
        while (p->entities->next)
            DeleteEntity(p->entities->next);
        DeleteEntity(p->entities);
        p->entities = NULL;
    }

    p->document->pagesQ--;
    if (p->document->selectedPage == p)
        p->document->selectedPage = NULL;

    if (p->document->lastInsertedPage == p)
        p->document->lastInsertedPage = NULL;

    if (p == p->document->pages)
        p->document->pages = p->next;
    else {
        for (Page *pp = p->document->pages; pp != NULL; pp = pp->next){
            if (pp->next == p){
                pp->next = p->next;
                break;
            }
        }
    }    

    free(p->name);
    free(p);
}

void SelectPage(Page *p){
    if (!p) return;
    p->document->selectedPage = p;
}

bool IsEntityNameAvailable(Page *p, char *name){
    for (PageEntity *pe = p->entities; pe != NULL; pe = pe->next){
        if (!pe->name) continue;
        if (strcmp(name, pe->name) == 0)
            return false;
    }
    
    return true;
}

char *ResolveEntityName(char *ogName, Page *p){
    char *rs = calloc(strlen(ogName)+1, sizeof(char));
    sprintf(rs, "%s", ogName);

    while (true){
        if (!IsEntityNameAvailable(p, rs)){
            
            bool incremental = false;
            char *underScore = strrchr(rs, '_');
            if (underScore){
                int len = strlen(rs);
                int underScoreIndex = underScore - rs;

                if (len > underScoreIndex+1){
                    int tmp;
                    char tmpStr[32] = {0};
                    if (sscanf(underScore+1, "%d", &tmp) == 1) {
                        incremental = true;
                        sprintf(tmpStr, "%i", ++tmp);
                        char *newName = calloc(underScoreIndex+strlen(tmpStr)+2, sizeof(char));
                        memcpy(newName, rs, underScoreIndex+1);
                        sprintf(newName, "%s%i", newName, tmp);
                        free(rs);
                        rs = newName;
                    }
                }
            }

            if (!incremental){
                char *newName = calloc(strlen(rs)+3, sizeof(char));
                sprintf(newName, "%s_0", rs);
                free(rs);
                rs = newName;
            }

            continue;
        }

        return rs;
    }
    
    return NULL;
}

void SelectEntity(PageEntity *e){
    if (!e) return;
    e->page->selectedEntity = e;
}

static PageEntity *AddEntity(Page *p, PageEntityType type){
    PageEntity *e = calloc(1, sizeof(PageEntity));
    e->type = type;
    e->page = p;
    e->pos = Vector2Zero();

    if (!p->entities){
        p->entities = e;
        p->entitiesQ++;
        return e;
    }

    PageEntity *ee;
    for (ee = p->entities; ee->next != NULL; ee = ee->next);
    ee->next = e;
    p->entitiesQ++;

    return e;
}

PageEntitySurface *AddSurfaceEntity(Page *p, Surface *s){
    PageEntity *e = AddEntity(p, PAGE_ENTITY_SURFACE);
    e->name = ResolveEntityName(s->name, e->page);
    
    PageEntitySurface *es = calloc(1, sizeof(PageEntitySurface));

    e->surface = es;
    es->entity = e;
    es->surface = s;
    es->scale = 1.0f;
    
    // setup flags
    es->flags |= PESF_DRAW_ANNOTATIONS;
    es->flags |= PESF_DRAW_SKINS;

    es->annotationsFontSize = OG.defaultFontSize;
    es->annotationsArrowSize = 10.0f;
    
    return es;
}

PageEntityText *AddTextEntity(Page *p, char *text){
    PageEntity *e = AddEntity(p, PAGE_ENTITY_TEXT);
    e->name = ResolveEntityName("Text", e->page);

    PageEntityText *et = calloc(1, sizeof(PageEntityText));
    e->text = et;
    et->entity = e;
    et->text = calloc(strlen(text)+1, sizeof(char));
    strcpy(et->text, text);
    et->fontSize = 20.0f;
    et->color = BLACK;

    return et;
}

PageEntityTitleBlock *AddTitleBlockEntity(Page *p, TitleBlock *tb){
    PageEntity *e = AddEntity(p, PAGE_ENTITY_TITLEBLOCK);
    e->name = ResolveEntityName(GetFileName(tb->sourcePath), p);

    PageEntityTitleBlock *etb = calloc(1, sizeof(PageEntityTitleBlock));

    e->block = etb;
    etb->entity = e;
    etb->block = tb;

    for (TitleBlockRect *r = tb->rects; r != NULL; r = r->next){
        if (!r->externalContent) continue;
        AddTitleBlockEntityProp(etb, r);
    }

    return etb;
}

PageEntityTitleBlockProp *AddTitleBlockEntityProp(PageEntityTitleBlock *tbe, TitleBlockRect *r){
    if (!tbe) return NULL;
    for (PageEntityTitleBlockProp *p=tbe->properties; p != NULL; p = p->next)
        if(p->r == r) return NULL;
    
    PageEntityTitleBlockProp *newProp = calloc(1, sizeof(PageEntityTitleBlockProp));
    newProp->parent = tbe;
    newProp->r = r;
    newProp->content = calloc(1,1);

    if (tbe->properties == NULL){
        tbe->properties = newProp;
        return newProp;
    }

    for (PageEntityTitleBlockProp *p=tbe->properties; p != NULL; p = p->next){
        if (p->next == NULL){
            p->next = newProp;
            break;
        }
    }

    return newProp;
}

PageEntitySticker *AddStickerEntity(Page *p, Sticker *s){
    PageEntity *e = AddEntity(p, PAGE_ENTITY_STICKER);
    e->name = ResolveEntityName("Sticker", e->page);

    PageEntitySticker *es = calloc(1, sizeof(PageEntitySticker));
    e->sticker = es;
    es->entity = e;
    es->sticker = s;
    es->scale = 1.0f;
    s->refQ++;

    return es;
}

void DeleteTitleBlockEntityProp(PageEntityTitleBlockProp *prop){
    if (!prop) return;
    for (PageEntityTitleBlockProp *pp = prop->parent->properties; pp != NULL; pp = pp->next){
        if (pp->next == prop) {
            pp->next = prop->next;
            break;
        }
    }

    if (prop->parent->properties == prop)
        prop->parent->properties = prop->next;

    if (prop->content)
        free(prop->content);
    free(prop);
}

void DeleteEntity(PageEntity *e){
    if (!e) return;
    e->page->entitiesQ--;
    if (e->page->selectedEntity == e)
        e->page->selectedEntity = NULL;
    if (e == e->page->entities)
        e->page->entities = e->next;
    else {
        for (PageEntity *ee = e->page->entities; ee != NULL; ee = ee->next){
            if (ee->next == e){
                ee->next = e->next;
                break;
            }
        }
    }

    switch (e->type){
        case PAGE_ENTITY_SURFACE:{
            free(e->surface);
        } break;
        
        case PAGE_ENTITY_TEXT:{
            free(e->text->text);
            free(e->text);
        } break;
    
        case PAGE_ENTITY_TITLEBLOCK:{
            PageEntityTitleBlockProp *prop = e->block->properties;
            while (prop != NULL){
                PageEntityTitleBlockProp *toDel = prop;
                prop = prop->next;
                free(toDel->content);
                free(toDel);
            }

            free(e->block);
        } break;

        case PAGE_ENTITY_STICKER:{
            e->sticker->sticker->refQ--;
            if (e->sticker->sticker->refQ <= 0)
                RemoveSticker(e->sticker->sticker);
            free(e->sticker);
        } break;
    }

    free(e->name);
    free(e);
}

bool ShiftEntityUp(PageEntity *e){
    if (!e) return false;
    if (e == e->page->entities)
        return false;

    PageEntity *prev;
    PageEntity *prevPrev = NULL;

    for (PageEntity *ee = e->page->entities; ee != NULL; ee = ee->next){
        if (ee->next == e){
            prev = ee;
            break;
        }
        prevPrev = ee;
    }

    if (prevPrev)
        prevPrev->next = e;
    else
        e->page->entities = e;

    prev->next = e->next;
    e->next = prev;

    return true;
}

bool ShiftEntityDown(PageEntity *e){
    if (!e) return false;
    if (e->next == NULL)
        return false;

    PageEntity *tmp = e->next;

    if (e == e->page->entities){
        e->next = tmp->next;
        tmp->next = e;
        e->page->entities = tmp;
        return true;
    }

    PageEntity *prev;
    for (PageEntity *ee = e->page->entities; ee != NULL; ee = ee->next){
        if (ee->next == e){
            prev = ee;
            break;
        }
    }

    e->next = tmp->next;
    tmp->next = e;
    prev->next = tmp;

    return true;
}

bool ExportDoc2PDF(Document *doc, char *path){
    struct pdf_info info;
    struct pdf_doc *pdf = pdf_create(
        PDF_MM_TO_POINT(doc->width), 
        PDF_MM_TO_POINT(doc->height), 
        &info
    );

    OG_Viewport tmp;
    tmp.camera.target = (Vector2){
        (doc->width/2)*-1, 
        (doc->height/2)*-1
    };
    tmp.camera.offset = Vector2Zero();
    tmp.camera.rotation = 0.0f;
    tmp.camera.zoom = 1.0f;
    
    SetCadOutput(CAD_OUTPUT_PDF, pdf);
    for (Page *p = doc->pages; p != NULL; p = p->next){
        pdf_append_page(pdf);
        
        for (PageEntity *pe = p->entities; pe != NULL; pe = pe->next){
            switch (pe->type){
                case PAGE_ENTITY_SURFACE: {
                    Texture tmptxt;
                    // Yes, it is empty. It is used
                    // only to satisfy validation checks.

                    DrawSurfaceTree(
                        &tmp, pe->surface->surface->tree, 
                        pe->surface->lod, 
                        &tmptxt, 
                        pe->pos, 
                        pe->surface->scale, 
                        BLACK, 
                        BLACK,
                        HATCH_SPACE,
                        false, false, false, 
                        true,
                        pe->surface->flags & PESF_DRAW_SKINS,
                        pe->surface->flags & PESF_DRAW_CUTOUTS,
                        false
                    );

                    if (pe->surface->flags & PESF_DRAW_ANNOTATIONS){
                        for (Annotation *ann = pe->surface->surface->tree->annotations; ann != NULL; ann = ann->next){
                            if ((IsAnchorPartOfCutout(ann->A) || IsAnchorPartOfCutout(ann->B)) && !(pe->surface->flags & PESF_DRAW_CUTOUTS)) continue;
                            if ((IsAnchorPartOfBoundary(ann->A) || IsAnchorPartOfBoundary(ann->B)) && !(pe->surface->flags & PESF_DRAW_SKINS)) continue;
                            
                            DrawAnnotation(
                                &tmp, ann, BLACK, 
                                pe->surface->annotationsArrowSize,
                                pe->surface->annotationsFontSize,
                                pe->surface->scale,
                                pe->pos
                            );
                        }
                    }

                } break;

                case PAGE_ENTITY_TEXT: {
                    Vector2 mt = MeasureCadText(
                        cadFont, 
                        pe->text->text, 
                        0,
                        pe->text->fontSize, 
                        0.0f,
                        1.0f
                    );
                    
                    DrawCadText(
                        cadFont, 
                        pe->text->text,
                        GetWorldToScreen2D(
                            pe->pos, 
                            tmp.camera
                        ),
                        (Vector2){
                            mt.x/2 * tmp.camera.zoom, 
                            -mt.y/2 * tmp.camera.zoom
                        },
                        0.0f, 
                        pe->text->fontSize * tmp.camera.zoom,
                        0.0f, 
                        1.0f,
                        pe->text->alignment,
                        pe->text->color
                    );
                } break;

                case PAGE_ENTITY_TITLEBLOCK: {
                    DrawTitleBlock(
                        pe->block->block, 
                        &tmp, 
                        pe->pos, 
                        BLACK,
                        true,
                        pe->block
                    );
                } break;

                case PAGE_ENTITY_STICKER: {
                    DrawCadSticker(
                        pe->sticker->sticker, 
                        GetWorldToScreen2D(
                            pe->pos, 
                            tmp.camera
                        ),
                        pe->sticker->scale
                    );
                } break;
            }
        }

        // DrawFrame
        if (p->drawFrame){
            Vector2 tlc = GetWorldToScreen2D(
                (Vector2){
                    mainDocument.selectedPage->frame.left - mainDocument.width/2.0f,
                    mainDocument.selectedPage->frame.top - mainDocument.height/2.0f
                }, 
                tmp.camera
            );
            
            Vector2 trc = GetWorldToScreen2D(
                (Vector2){
                    mainDocument.width - mainDocument.selectedPage->frame.right - mainDocument.width/2.0f,
                    mainDocument.selectedPage->frame.top - mainDocument.height/2.0f
                },
                tmp.camera
            );

            Vector2 blc = GetWorldToScreen2D(
                (Vector2){
                    mainDocument.selectedPage->frame.left - mainDocument.width/2.0f,
                    mainDocument.height - mainDocument.selectedPage->frame.bottom - mainDocument.height/2.0f
                },
                tmp.camera
            );

            Vector2 brc = GetWorldToScreen2D(
                (Vector2){
                    mainDocument.width - mainDocument.selectedPage->frame.right - mainDocument.width/2.0f,
                    mainDocument.height - mainDocument.selectedPage->frame.bottom - mainDocument.height/2.0f
                },
                tmp.camera
            );

            DrawCadLine(tlc, trc, 1.0f, BLACK);
            DrawCadLine(trc, brc, 1.0f, BLACK);
            DrawCadLine(brc, blc, 1.0f, BLACK);
            DrawCadLine(blc, tlc, 1.0f, BLACK);
        }
    }

    int rs = pdf_save(pdf, path);
    pdf_destroy(pdf);
    SetCadOutput(CAD_OUTPUT_DEFAULT, NULL);
    return rs >= 0;
}

bool ExportSurface2SVG(PageEntitySurface *s, char *path){
    FILE *file = fopen(path, "w");
    if (!file) return false;
    
    SVG *psvg;

    OG_Viewport tmp;
    tmp.camera.target = (Vector2){ 0, 0 };
    tmp.camera.offset = Vector2Zero();
    tmp.camera.rotation = 0.0f;
    tmp.camera.zoom = 1.0f;
    
    Texture tmptxt;
    // Yes, it is empty. It is used
    // only to satisfy validation checks.

    PageEntity *entity = mainDocument.selectedPage->selectedEntity;
    SurfaceTree *tree = entity->surface->surface->tree;

    Vector2 offset = Vector2Zero();

    for (int i=0; i<2; i++){

        if (i == 0) SetCadOutput(CAD_OUTPUT_MEASURE, NULL);
        if (i == 1){
            CadBounds bounds = GetCadBounds();
            psvg = SVG_create(
                file, 
                fabs(bounds.min.x - bounds.max.x) + 2, 
                fabs(bounds.min.y - bounds.max.y) + 2
            );
            SetCadOutput(CAD_OUTPUT_SVG, psvg);
        }
        
        DrawSurfaceTree(
            &tmp, tree, 
            entity->surface->lod, 
            &tmptxt, 
            offset, 
            1.0, 
            BLACK, 
            BLACK, 
            0.0f, 
            false, false, false, 
            true, 
            entity->surface->flags & PESF_DRAW_SKINS , 
            entity->surface->flags & PESF_DRAW_CUTOUTS, 
            false
        );

        // Draw Annotations
        if (entity->surface->flags & PESF_DRAW_ANNOTATIONS){
            for (Annotation *ann = entity->surface->surface->tree->annotations; ann != NULL; ann = ann->next){
                if ((IsAnchorPartOfCutout(ann->A) || IsAnchorPartOfCutout(ann->B)) && !(entity->surface->flags & PESF_DRAW_CUTOUTS)) continue;
                if ((IsAnchorPartOfBoundary(ann->A) || IsAnchorPartOfBoundary(ann->B)) && !(entity->surface->flags & PESF_DRAW_SKINS)) continue;
                
                DrawAnnotation(
                    &tmp, ann, BLACK, 
                    entity->surface->annotationsArrowSize,
                    entity->surface->annotationsFontSize/entity->surface->scale,
                    1.0f,
                    offset
                );
            }
        }

        if (i == 0){
            CadBounds bounds = GetCadBounds();
            offset = (Vector2){
                bounds.min.x * -1,
                bounds.min.y * -1
            };
        }
    }

    SetCadOutput(CAD_OUTPUT_DEFAULT, NULL);
    SVG_finalize(psvg);
    SVG_free(psvg);
    fclose(file);

    return true;
}

// Not sure if this should be here :/
bool ExportSurfaceCutouts2DXF(SurfaceTree *t, char *path){
    FILE *file = fopen(path, "w");
    if (!file) return false;
    
    DXF dxf = { file, "SURFACE", 1 };
    DXF_Begin(file, DXF_MILLIMETERS);
    SetCadOutput(CAD_OUTPUT_DXF, &dxf );

    // ________________ DRAW THE BASE SURFACE CONTOUR
    DrawCadLine((Vector2){0,0}, (Vector2){0, activeTree->baseSurface.size.y}, 1.0f, WHITE);
    DrawCadLine((Vector2){0, activeTree->baseSurface.size.y}, (Vector2){activeTree->baseSurface.size.x, activeTree->baseSurface.size.y}, 1.0f, WHITE);
    DrawCadLine((Vector2){activeTree->baseSurface.size.x, activeTree->baseSurface.size.y}, (Vector2){activeTree->baseSurface.size.x, 0}, 1.0f, WHITE);
    DrawCadLine((Vector2){activeTree->baseSurface.size.x, 0}, (Vector2){0,0}, 1.0f, WHITE);

    // ________________ DRAW THE CUTOUTS
    OG_Viewport tmp;
    tmp.camera.target = (Vector2){ 0, 0 };
    tmp.camera.offset = Vector2Zero();
    tmp.camera.rotation = 0.0f;
    tmp.camera.zoom = 1.0f;
    
    Texture tmptxt;
    // Yes, it is empty. It is used
    // only to satisfy validation checks.

    dxf.layer = "CUTOUTS";
    dxf.color = 2;
    DrawSurfaceTree(
        &tmp, 
        activeTree, 
        0, 
        &tmptxt, 
        (Vector2){
            activeTree->baseSurface.size.x/2, 
            activeTree->baseSurface.size.y/2
        }, 
        1.0f, 
        WHITE, 
        WHITE, 
        0.0f, 
        false, true, 
        false, true, 
        false, true,
        true
    );

    SetCadOutput(CAD_OUTPUT_DEFAULT, NULL);
    DXF_End(file);
    fclose(file);

    return true;
}