// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surfaces.h"
#include "caddraw.h"
#include "origami.h"
#include "utils.h"
#include "main.h"

/*************************************/
/* TODO: use matrices for transforms */
/*************************************/

/* --------------------------------- */
/* PLEASE MANTAIN PROPER ABSTRACTION */
/* --------------------------------- */
// don't use "activeTree" here
// don't use "PushLog"

// NOTE: DrawExtensibleSkin assumes axis-aligned rotations (0/90/180/270).
// Should be updated if full arbitrary rotation support is introduced.

// WARNING: DeleteSurface(Surface *s) uses recursion to delete child surfaces.
// Deep surface hierarchies can cause a stack overflow, especially on Windows
// where the available stack may be limited. Consider replacing the recursive
// traversal with an iterative approach, possibly using the linear surface tree
// instead of following the parent-child hierarchy.

/* <== Surfaces ================================================> */

static bool IsSurfaceRelated2Anchor(Surface *s, Anchor *a);

const char *GetSurfaceTypeStr(SurfaceType t){
    switch (t){
        case SURFACE_COMPONENT:  return "SURFACE_COMPONENT";
        case SURFACE_EXTENSIBLE: return "SURFACE_EXTENSIBLE";
    }
}

static void UpdateSurfaceTreeHelper(Surface *base, int *callCount, int depth){
    base->treeDepth = depth;
    base->tree->linearSurfaceTree[(*callCount)++] = base;
    
    if (base->children == NULL) return;
    Surface *ss = base->children;
    while (ss){
        UpdateSurfaceTreeHelper(ss, callCount, depth+1);
        ss = ss->next;
    }
}

void UpdateSurfaceTree(SurfaceTree *tree){
    if (tree == NULL) return;
    if (tree->linearSurfaceTree != NULL)
        free(tree->linearSurfaceTree);
    
    tree->linearSurfaceTree = calloc(tree->surfacesQ, sizeof(Surface*));
    
    int callCount = 0;
    UpdateSurfaceTreeHelper(&tree->baseSurface, &callCount, 0);
}

Surface *AddSurface(Surface *parent, char *id, char *name, SurfaceType type){
    if (parent == NULL) return NULL;
    if (id == NULL) return NULL;
    if (name == NULL) return NULL;
    
    Surface *newSurface = calloc(1, sizeof(Surface));
    newSurface->tree = parent->tree;
    newSurface->parent = parent;
    newSurface->type = type;
    if (parent->children == NULL){
        parent->children = newSurface;
        goto end;
    }

    // link the list
    Surface *s = parent->children;
    while (s->next != NULL){
        s = s->next;
    }
    s->next = newSurface;
    
    end:
    parent->tree->surfacesQ++;
    UpdateSurfaceTree(parent->tree);
    newSurface->id = calloc(strlen(id)+1, sizeof(char));
    strcpy(newSurface->id, id);
    newSurface->name = calloc(strlen(name)+1, sizeof(char));
    strcpy(newSurface->name, name);
    newSurface->tree->lastInsertedSurface = newSurface;
    return newSurface;
}

bool ClearSurfaceSkin(Skin *s){
    if (!s->points) return false;
    
    free(s->points);
    for (int i=0; i<s->lodsQ; i++){
        if (s->lods[i].lines)
            free(s->lods[i].lines);
    }
        
    free(s->lods);
    s->surface = NULL;
    s->points = NULL;
    s->pointsQ = 0;
    s->lods = NULL;
    s->lodsQ = 0;
    s->len = 0.0f;
    s->tint = WHITE;
    return true;
}

static bool AnchorDependsOnSurface(Anchor *a, Surface *s, Surface **sout){
    if (a->refSurface == s){
        if (sout) *sout = s;
        return true;
    }
    for (Surface *ss = s->children; ss != NULL; ss = ss->next){
        if (AnchorDependsOnSurface(a, ss, sout))
            return true;
    }

    return false;
}

int DeleteSurface(Surface *s){
    if (s == NULL) return -1;
    if (s->parent == NULL) return -2;
    
    int results[1024] = {0};
    int rsi = 0;
    for (Anchor *a = s->tree->anchors; a != NULL; a = a->next){
        if (AnchorDependsOnSurface(a, s, NULL))
            results[rsi++] = (IsAnchorReferencedByAnchor(a, treeList, treeListQ) || IsAnchorReferencedBySurface(a) || a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) ? 2:1;
    }

    for (int i=0; i<rsi; i++){
        if (results[i] == 2)
            return -4; // circular dependency
    }

    for (Anchor *a = s->tree->anchors; a != NULL;){
        Anchor *tmp = a;
        a = a->next;
        if (AnchorDependsOnSurface(tmp, s, NULL))
            DeleteAnchor(tmp, treeList, treeListQ);
    }
    
    if (s->tree->lastInsertedSurface == s){
        s->tree->lastInsertedSurface = NULL;
    }

    // Delete every children recursive
    Surface *ss = s->children;
    while (ss != NULL){
        Surface *next = ss->next;
        DeleteSurface(ss);
        ss = next;
    }

    // Delete skin
    ClearSurfaceSkin(&s->skin);

    // Delete cutout
    if (s->cutout.points){
        free(s->cutout.points);
        s->cutout.len = 0;
    }
    
    // Unlink the list
    ss = s->parent->children;
    if (ss == s)
        s->parent->children = s->next;
    
    else{
        while (ss != NULL){
            if (ss->next == s){
                ss->next = s->next;
                break;
            }
                
            ss = ss->next;
        }
    }

    s->tree->surfacesQ--;
    UpdateSurfaceTree(s->tree);
    free(s->id);
    free(s->name);
    free(s);

    return 0;
}

int ReparentSurface(Surface *s, Surface *parent){
    if (!s) return -1;
    if (!parent) return -2;
    if (IsSurfaceBase(s)) return -3;
    if (s->parent == parent) return -4;

    // prevent cyclic parent hierarchy
    Surface *p = parent;
    while (p){
        if (p == s) return -5;
        p = p->parent;
    }

    Vector2 ogPos = s->globalPos;
    float ogRot = s->globalRotation;

    // unlink surface from current parent's children list
    Surface *prev = NULL;
    Surface *cur = s->parent->children;

    while (cur) {
        if (cur == s) {
            if (prev) prev->next = cur->next;
            else s->parent->children = cur->next;
            break;
        }

        prev = cur;
        cur = cur->next;
    }

    // attach surface to end of new parent's children list
    s->next = NULL;
    s->parent = parent;

    if (!parent->children)
        parent->children = s;
    
    else {
        Surface *last = parent->children;
        while (last->next)
            last = last->next;
        last->next = s;
    }

    UpdateSurfaceTree(s->tree);
    SetSurfaceGlobalPos(s, ogPos);
    SetSurfaceGlobalRotation(s, ogRot);
    return 0;
}

void ClearSurfaceTree(SurfaceTree *t){
    Surface *s = t->baseSurface.children;
    while (s != NULL){
        Surface *next = s->next;
        DeleteSurface(s);
        s = next;
    }
}

static RectCorners GetSurfaceBoundaries(OG_Viewport *v, Surface *s, Vector2 offset, Vector2 finalOffset, float scale, bool flipX, bool flipY){
    Rectangle r = {
        s->globalPos.x * scale, 
        s->globalPos.y * scale, 
        s->size.x, 
        s->size.y
    };

    // GET RECT CORNERS INTERNALS
    // ---------------------------------------------------
    // (made this way because applying leftCut and rightCut 
    // before turning is easier)
    RectCorners rc;
    Vector2 center = {
        s->size.x/2 + offset.x,
        s->size.y/2 + offset.y
    };
    
    rc.points[0] = Vector2RotateDegrees((Vector2){
        (center.x*-1) - (s->leftCut < 0 ? s->leftCut:0),
        center.y*-1
    },s->globalRotation);
    
    rc.points[1] = Vector2RotateDegrees((Vector2){
        (r.width-center.x) + (s->rightCut < 0 ? s->rightCut:0),
        center.y*-1
    },s->globalRotation);
    

    rc.points[2] = Vector2RotateDegrees((Vector2){
        (r.width-center.x) - (s->rightCut > 0 ? s->rightCut:0),
        (r.height-center.y)
    },s->globalRotation);
    
    rc.points[3] = Vector2RotateDegrees((Vector2){
        (center.x*-1) + (s->leftCut > 0 ? s->leftCut:0),
        (r.height-center.y)
    },s->globalRotation);

    rc.points[0] = (Vector2){rc.points[0].x * scale, rc.points[0].y * scale};
    rc.points[1] = (Vector2){rc.points[1].x * scale, rc.points[1].y * scale};
    rc.points[2] = (Vector2){rc.points[2].x * scale, rc.points[2].y * scale};
    rc.points[3] = (Vector2){rc.points[3].x * scale, rc.points[3].y * scale};
    rc.points[4] = (Vector2){rc.points[4].x * scale, rc.points[4].y * scale};
    
    rc.points[3] = Vector2Add((Vector2){r.x,r.y}, rc.points[3]);
    rc.points[2] = Vector2Add((Vector2){r.x,r.y}, rc.points[2]);
    rc.points[1] = Vector2Add((Vector2){r.x,r.y}, rc.points[1]);
    rc.points[0] = Vector2Add((Vector2){r.x,r.y}, rc.points[0]);
    rc.points[4] = rc.points[0];

    rc.points[0] = (Vector2){rc.points[0].x * (flipX ? -1:1), rc.points[0].y * (flipY ? -1:1)};
    rc.points[1] = (Vector2){rc.points[1].x * (flipX ? -1:1), rc.points[1].y * (flipY ? -1:1)};
    rc.points[2] = (Vector2){rc.points[2].x * (flipX ? -1:1), rc.points[2].y * (flipY ? -1:1)};
    rc.points[3] = (Vector2){rc.points[3].x * (flipX ? -1:1), rc.points[3].y * (flipY ? -1:1)};
    rc.points[4] = (Vector2){rc.points[4].x * (flipX ? -1:1), rc.points[4].y * (flipY ? -1:1)};

    rc.points[0] = Vector2Add(rc.points[0], finalOffset);
    rc.points[1] = Vector2Add(rc.points[1], finalOffset);
    rc.points[2] = Vector2Add(rc.points[2], finalOffset);
    rc.points[3] = Vector2Add(rc.points[3], finalOffset);
    rc.points[4] = Vector2Add(rc.points[4], finalOffset);

    // ---------------------------------------------------

    if (!v) return rc;

    return (RectCorners){
        {
            GetWorldToScreen2D(rc.points[0], v->camera),
            GetWorldToScreen2D(rc.points[1], v->camera),
            GetWorldToScreen2D(rc.points[2], v->camera),
            GetWorldToScreen2D(rc.points[3], v->camera),
            GetWorldToScreen2D(rc.points[0], v->camera),
        }
    };
}

static void DrawSurfaceBoundaries(RectCorners rc, Color c, bool dashed){
    
    if (!dashed){
        DrawCadLine(rc.points[0], rc.points[1], 1.0, c);
        DrawCadLine(rc.points[1], rc.points[2], 1.0, c);
        DrawCadLine(rc.points[2], rc.points[3], 1.0, c);
        DrawCadLine(rc.points[3], rc.points[0], 1.0, c);
    }

    else {
        DrawLineDashedEx(rc.points[0], rc.points[1], 1.0, 5, 5, c);
        DrawLineDashedEx(rc.points[1], rc.points[2], 1.0, 5, 5, c);
        DrawLineDashedEx(rc.points[2], rc.points[3], 1.0, 5, 5, c);
        DrawLineDashedEx(rc.points[3], rc.points[0], 1.0, 5, 5, c);
    }
}

static void DrawSurfaceCutout(OG_Viewport *v, Surface *s, Vector2 pos, float rotation, Vector2 scale, bool flipX, bool flipY, float lineSpace, Color c){
    if (!s || !v) return;

    bool useRect = s->cutout.rect.x > 0 && s->cutout.rect.y > 0;

    if (!useRect && (!s->cutout.points || s->cutout.len < 3))
        return;

    int pointsQ = useRect ? 4 : s->cutout.len;
    Vector2 points[pointsQ];

    if (useRect) {
        Vector2 rect = s->cutout.rect;

        points[0] = (Vector2){-rect.x/2, -rect.y/2};
        points[1] = (Vector2){rect.x/2, -rect.y/2};
        points[2] = (Vector2){rect.x/2, rect.y/2};
        points[3] = (Vector2){-rect.x/2, rect.y/2};
    }

    for (int i = 0; i < pointsQ; i++) {
        Vector2 source = useRect ? points[i] : s->cutout.points[i];

        Vector2 ver = {
            source.x * scale.x,
            source.y * scale.y
        };

        ver = Vector2RotateDegrees(BlenderToRaylib(ver), rotation);

        points[i] = GetWorldToScreen2D(
            (Vector2){
                ver.x * (flipX ? -1 : 1) + pos.x,
                ver.y * (flipY ? -1 : 1) + pos.y
            },
            v->camera
        );
    }

    DrawHatchPoly(points, pointsQ, lineSpace, c);
}

// TODO: Move transformations (scale, rotation, position and flips) to the GPU
// to improve rendering performance

static bool DrawSurfaceSkin(OG_Viewport *v, Vector2 *mask, int maskC, Skin skin, int lod, Color color, Vector2 pos, float rotation, Vector2 scale, bool flipX, bool flipY){
    if (skin.lodsQ <= 0) return false;
    if (skin.pointsQ <= 0) return false;
    if (skin.points == NULL) return false;
    if (skin.lods == NULL) return false;
    if (skin.surface == NULL) return false;

    // Project the points
    Vector2 points[skin.pointsQ];
    for (int i=0; i<skin.pointsQ; i++){
        Vector2 ver = (Vector2){skin.points[i].x*scale.x, skin.points[i].y*scale.y};
        ver = Vector2RotateDegrees(BlenderToRaylib(ver), rotation);
        
        if (v){
            points[i] = GetWorldToScreen2D(
                (Vector2){
                    ver.x * (flipX ? -1:1) + pos.x,
                    ver.y * (flipY ? -1:1) + pos.y
                }, 
                v->camera
            );
        }

    }

    // LOD order in component files is inverted
    lod = lodsQ-lod-1;
    if (lod >= skin.lodsQ)
        lod = skin.lodsQ-1;

    
    // For each edge in the selected LOD
    OpenCadPath();
    SetCadMask(mask, maskC);
    
    SkinLod *selLod = &skin.lods[lod];
    for (int i=0; i<selLod->linesQ; i++){
        Vector2 A = points[selLod->lines[i].A];
        Vector2 B = points[selLod->lines[i].B];
        DrawCadLine( A, B, 1.0, color );
    }

    UnsetCadMask();
    CloseCadPath();
    return true;
}

static bool DrawExtensibleSkin(OG_Viewport *v, RectCorners rc, Skin skin, int lod, Color color, Vector2 pos, float rotation, float scale, bool flipX, bool flipY){
    if (skin.lodsQ <= 0) return false;
    if (skin.pointsQ <= 0) return false;
    if (skin.points == NULL) return false;
    if (skin.lods == NULL) return false;
    if (skin.surface == NULL) return false;
    
    bool isHorizontal = IsHorizontal(skin.surface->globalRotation);

    float acc = 0.0f;
    while (true){
        float cut = (
            (skin.surface->size.x*scale)/2 - 
            (skin.len*scale)/2 - 
            fabsf(
                (
                    isHorizontal ? (skin.surface->rotation == 0 ? skin.surface->rightCut:skin.surface->leftCut):
                    (skin.surface->rotation == 90 ? skin.surface->rightCut:skin.surface->leftCut)
                ) * scale
            )
        );

        // FIRST ONE
        if (acc == 0.0f){
            DrawSurfaceSkin(
                v, rc.points, 4, 
                skin, lod, color, 
                (Vector2){pos.x, pos.y}, 
                rotation, Vector2Scale(Vector2One(), scale), 
                flipX, flipY
            );
        }

        // RIGHT SIDE
        else if (acc > cut){
            DrawSurfaceSkin(
                v, rc.points, 4, 
                skin, lod, color, 
                (Vector2){
                    isHorizontal ? (pos.x + acc):pos.x, 
                    isHorizontal ? pos.y : (pos.y + acc)
                }, 
                rotation, Vector2Scale(Vector2One(), scale), 
                flipX, flipY
            );
        }

        else {
            DrawSurfaceSkin(
                v, NULL, 0,
                skin, lod, color, 
                (Vector2){
                    isHorizontal ? (pos.x + acc):pos.x, 
                    isHorizontal ? pos.y : (pos.y + acc)
                }, 
                rotation, Vector2Scale(Vector2One(), scale), 
                flipX, flipY
            );
        }

        // LEFT SIDE
        if (acc > 0){
            float cut = (
                (skin.surface->size.x*scale)/2 - 
                (skin.len*scale)/2 - 
                fabsf(
                    (
                        isHorizontal ? (skin.surface->rotation == 0 ? skin.surface->leftCut:skin.surface->rightCut):
                        (skin.surface->rotation == 90 ? skin.surface->leftCut:skin.surface->rightCut)
                    ) * scale
                )
            );

            if (acc > cut){
                DrawSurfaceSkin(
                    v, rc.points, 4, 
                    skin, lod, color, 
                    (Vector2){
                        isHorizontal ? (pos.x - acc):pos.x, 
                        isHorizontal ? pos.y : (pos.y - acc)
                    }, 
                    rotation, Vector2Scale(Vector2One(), scale), 
                    flipX, flipY
                );
            }

            else {
                DrawSurfaceSkin(
                    v, NULL, 0,
                    skin, lod, color, 
                    (Vector2){
                        isHorizontal ? (pos.x - acc):pos.x, 
                        isHorizontal ? pos.y : (pos.y - acc)
                    }, 
                    rotation, Vector2Scale(Vector2One(), scale), 
                    flipX, flipY
                );
            }
        }

        if (acc > (skin.surface->size.x*scale)/2 - (skin.len*scale)/2)
            break;
        acc += skin.len * scale;
        
    }
    return true;
}

void DrawSurfaceTree(OG_Viewport *v, SurfaceTree *tree, int lod, Vector2 offset, float scale, Color lineColor, Color cutoutColor, float hatchLineSpace, float fontSize, bool flipX, bool flipY, bool drawBounds, bool noSelectionLines, bool drawSkins, bool drawcutouts, bool drawLabels, bool omitBaseSkin){
    for (int i=0; i<tree->surfacesQ; i++){
        Surface *s = tree->linearSurfaceTree[i];
        if (s->opts & SURFACE_OPT_HIDDEN) continue;
        
        RectCorners rc = GetSurfaceBoundaries(v, s, s->origin, offset, scale, flipX, flipY);
        if (drawSkins || s == &tree->baseSurface){
            EraseCadArea(rc.points, 5);
        }

        Color colors[3] = {
            ColorIsEqual(s->skin.tint, WHITE) ? lineColor:s->skin.tint, 
            HIGHLIGHT_C, 
            HIGHLIGHT_C1
        };

        int c = 0;
        if (s == s->tree->surfaceSelection[0]) c = 1;
        for (int o=1; o<SURFACE_SELECTION_BUF_LEN; o++){
            if (s == s->tree->surfaceSelection[o]) c = 2;
        }

        Vector2 center = Vector2RotateDegrees(s->origin, s->globalRotation);
        Vector2 pos = {
            (s->globalPos.x - center.x) * (flipX ? -1:1),
            (s->globalPos.y - center.y) * (flipY ? -1:1)
        };

        bool didDrawSkin = false;
        
        Vector2 posScaled = Vector2Add(
            Vector2Scale(pos, scale),
            offset
        );
        
        switch (s->type){
            case SURFACE_COMPONENT:{
                Color ccc = ColorIsEqual(s->skin.tint, WHITE) ? lineColor:ColorWithAlpha(s->skin.tint, lineColor.a); 
                Vector2 surfaceScale = Vector2Multiply(Vector2Divide(s->size, s->ogSize), Vector2Scale(Vector2One(), scale));
                
                if (drawcutouts && (s->cutout.points || (s->cutout.rect.x > 0 && s->cutout.rect.y > 0))){
                    DrawSurfaceCutout(
                        v, s, posScaled, 
                        s->globalRotation, 
                        s->opts & SURFACE_OPT_GENERIC ? (Vector2Scale(Vector2One(), scale)) : surfaceScale, 
                        flipX, flipY,
                        hatchLineSpace,
                        ColorWithAlpha(cutoutColor, ccc.a)
                    );
                }

                if (drawSkins || s == &tree->baseSurface){
                    if (s != &tree->baseSurface || !omitBaseSkin){
                        didDrawSkin = DrawSurfaceSkin(
                            v, NULL, 0,
                            s->skin, lod,
                            ccc, 
                            posScaled, 
                            s->globalRotation, 
                            surfaceScale, 
                            flipX, flipY
                        );
                    }
                    
                    else didDrawSkin = true;
                }

                else  didDrawSkin = true;

                break;   
            }

            case SURFACE_EXTENSIBLE:{
                didDrawSkin = DrawExtensibleSkin(
                    v, rc, s->skin, lod,
                    ColorIsEqual(s->skin.tint, WHITE) ? lineColor:ColorWithAlpha(s->skin.tint, lineColor.a), 
                    posScaled, 
                    s->globalRotation, 
                    scale, flipX, flipY
                );
        
                if (!didDrawSkin)
                    EraseCadArea(rc.points, 5);
                
                break;
            }
        }

        // if surface have no skin or is extensible
        if (!didDrawSkin || drawBounds || s->type == SURFACE_EXTENSIBLE){
            DrawSurfaceBoundaries(
                rc, 
                ColorWithAlpha(
                    colors[0], 
                    lineColor.a
                ), 
                false
            );
        }

        // if surface is selected
        if (c > 0 && !noSelectionLines){
            DrawSurfaceBoundaries(
                rc, 
                ColorWithAlpha(
                    colors[c], 
                    lineColor.a
                ), 
                s->type == SURFACE_COMPONENT ? true:false
            );
        }

        // Draw skin label
        if (drawLabels){
            Vector2 size = MeasureCadText(cadFont, s->skin.label.text, strlen(s->skin.label.text), fontSize, 0.0f, 0.0f);
            Vector2 labelPos = Vector2Add(
                Vector2Scale(Vector2Add(pos, s->skin.label.offset), scale),
                offset
            );

            DrawCadText(
                cadFont, 
                s->skin.label.text, 
                GetWorldToScreen2D(labelPos, v->camera), 
                (Vector2){size.x/2, size.y/2*-1}, 
                s->skin.label.rotation, 
                fontSize, 
                2.0f, 
                0.0f, 
                CAD_ALIGN_CENTER, 
                colors[0]
            );
        }

    }
}

Surface *GetSurfaceAt(Vector2 point, SurfaceTree *tree){
    for (int i=tree->surfacesQ-1; i>=0; i--){
        Surface *s = tree->linearSurfaceTree[i];
        if (s->opts & SURFACE_OPT_HIDDEN) continue;

        RectCorners r = GetSurfaceBoundaries(NULL, s, s->origin, (Vector2){0}, 1.0f, false, false);
        if (CheckCollisionPointPoly(point, r.points, 4)){
            return s;
        }
    }

    return NULL;
}

void SetSurfaceGlobalPos(Surface *s, Vector2 pos){
    if (s == NULL) return;

    if (s->parent == NULL){
        s->pos = pos;
        return;
    }

    Vector2 parentPos = s->parent->globalPos;
    float parentRot = s->parent->globalRotation;

    Vector2 local = {
        pos.x - parentPos.x,
        pos.y - parentPos.y
    };

    local = Vector2RotateDegrees(local, -parentRot);
    s->pos = local;
    s->globalPos = pos;
}

void SetSurfaceGlobalRotation(Surface *s, float rot){
    if (s == NULL) return;

    if (s->parent == NULL){
        s->rotation = rot;
        s->globalRotation = rot;
        return;
    }

    float parentRot = s->parent->globalRotation;
    s->rotation = NormalizeAngle(rot - parentRot);
    s->globalRotation = NormalizeAngle(rot);
}

void SelectSurface(Surface *s, bool aditive, bool canDeselect){
    if (aditive){
        for (int i=0; i<SURFACE_SELECTION_BUF_LEN; i++){
            if (s->tree->surfaceSelection[i] == s) break;
            if (s->tree->surfaceSelection[i] == NULL || i == SURFACE_SELECTION_BUF_LEN-1){
                s->tree->surfaceSelection[i] = s;
                break;
            }
        }
    }

    else {
        
        if (s->tree->surfaceSelection[0] ==s && canDeselect){
            for (int i=0; i<SURFACE_SELECTION_BUF_LEN; i++)
                s->tree->surfaceSelection[i] = NULL;
        }
        
        else {
            for (int i=0; i<SURFACE_SELECTION_BUF_LEN; i++)
                s->tree->surfaceSelection[i] = NULL;
            s->tree->surfaceSelection[0] = s;
        }
    }

}

bool IsSurfaceSelected(Surface *s){
    for (int i=0; i<SURFACE_SELECTION_BUF_LEN; i++){
        if (s->tree->surfaceSelection[i] == s)
            return true;
    }

    return false;
}

bool IsSurfaceBase(Surface *s){
    return s->parent ? false:true;   
}

int CanAddExtensible(SurfaceTree *tree, Vector2 *p){
    if (tree->anchorSelection[0] == NULL) return -1;
    
    // If only one anchor is selected, try to find the closest anchor on each side.
    if (tree->anchorSelection[1] == NULL && tree->anchorSelection[2] == NULL && p){
        Anchor *min = NULL;
        Anchor *max = NULL;
        for (Anchor *a = tree->anchors; a != NULL; a = a->next){
            if (a->opts & ANCHOR_OPT_HIDDEN) continue;
            if (a->type == tree->anchorSelection[0]->type) continue;

            float absVal = GetAnchorAbsValue(a);
            if (a->type == ANCHOR_HORIZONTAL){
                if (absVal < p->y){
                    if (!min) min = a;
                    else if (absVal > GetAnchorAbsValue(min))
                        min = a;
                }

                if (absVal > p->y){
                    if (!max) max = a;
                    else if (absVal < GetAnchorAbsValue(max))
                        max = a;
                }
            }

            else { // VERTICAL
                if (absVal < p->x){
                    if (!min) min = a;
                    else if (absVal > GetAnchorAbsValue(min))
                        min = a;
                }

                if (absVal > p->x){
                    if (!max) max = a;
                    else if (absVal < GetAnchorAbsValue(max))
                        max = a;
                }
            }
        }

        if (min && max){
            tree->anchorSelection[1] = min;
            tree->anchorSelection[2] = max;
        }
    }
        
    for (int i=0; i<3; i++){
        if (tree->anchorSelection[i] == NULL)
            return -1;    
    }

    if (tree->anchorSelection[3]){
        int hCount = 0;
        int vCount = 0;
        for (int i=0; i<4; i++){
            if (tree->anchorSelection[i]->type == ANCHOR_HORIZONTAL) hCount++;
            else vCount++;
        }

        if (hCount == vCount)
            return 0;
        return -2;
    }

    int hCount = 0;
    int vCount = 0;
    for (int i=0; i<3; i++){
        if (tree->anchorSelection[i]->type == ANCHOR_HORIZONTAL) hCount++;
        else vCount++;
    }

    if (hCount == 0 || vCount == 0)
        return -3;
    return 0;
}

int GetSurfaceIndex(Surface *s){
    SurfaceTree *t = s->tree;
    for (int i=0; i<t->surfacesQ; i++){
        Surface *ss = t->linearSurfaceTree[i];
        if (s == ss) return i;
    }

    return -1;
}

Surface *GetSurfaceByName(SurfaceTree *st, char *name){
    for (int i=0; i<st->surfacesQ; i++){
        Surface *s = st->linearSurfaceTree[i];
        if (strcmp(s->name, name) == 0)
            return s;
    }

    return NULL;
}

static Vector2 AlignChildSurfacesHelper(Surface *s, Vector2 dir, float offset, Vector2 p, bool isHorizontal, bool recursive){
    Rectangle bounds = RectFromCorners(
        GetSurfaceBoundaries(
            NULL, s, 
            (Vector2){0},
            (Vector2){0},
            1.0f,
            false, false
        )
    );

    Vector2 org = Vector2RotateDegrees(
        (Vector2){
            IsHorizontal(s->rotation) ? s->origin.x:0,
            IsVertical(s->rotation) ? s->origin.y:0
        }, 
        s->globalRotation
    );

    for (int i=0; i<2; i++){
        p = Vector2Add(
            p, 
            Vector2Multiply(
                dir, 
                (Vector2){
                    (isHorizontal ? bounds.width:bounds.height)/2,
                    (isHorizontal ? bounds.width:bounds.height)/2
                }
            )
        );

        if (i==0) SetSurfaceGlobalPos(s, Vector2Add(p, org));
    }

    p = Vector2Add(
        p, 
        Vector2Multiply(
            dir, 
            (Vector2){
                offset, 
                offset
            }
        )
    );

    if (recursive){
        for (Surface *c = s->children; c != NULL; c = c->next){
            Vector2 pp = AlignChildSurfacesHelper(c, dir, offset, p, isHorizontal, true);
            p = pp;
        }
    }

    return p;
}

void AlignChildSurfaces(Surface *s, int dir, float offset, bool recursive){
    if (!s) return;
    
    Vector2 direction = Vector2RotateDegrees(
        (Vector2){1*(dir ? -1:1),0}, 
        s->globalRotation
    );

    Rectangle bounds = RectFromCorners(
        GetSurfaceBoundaries(
            NULL, s, 
            (Vector2){0},
            (Vector2){0},
            1.0f,
            false, false
        )
    );

    bool isHorizontal = IsHorizontal(s->globalRotation);
    Vector2 p = Vector2Add(
        s->globalPos, 
        Vector2Multiply(
            direction, 
            (Vector2){
                (isHorizontal ? bounds.width:bounds.height)/2,
                (isHorizontal ? bounds.width:bounds.height)/2
            }
        )
    );

    Vector2 org = Vector2RotateDegrees(
        (Vector2){s->origin.x,0}, 
        s->globalRotation
    );

    p = Vector2Add(
        Vector2Subtract(p,org), 
        Vector2Multiply(
            direction, 
            (Vector2){
                offset, 
                offset
            }
        )
    );

    for (Surface *c = s->children; c != NULL; c = c->next){
        p = AlignChildSurfacesHelper(c, direction, offset, p, isHorizontal, recursive);
    }

}

bool IsSurfaceAnchored(Surface *s){
    if (!s) return false;
    return (s->anchors[0] != NULL || s->anchors[1] != NULL);
}

bool ShiftSurfaceUp(Surface *s){
    if (!s) return false;
    if (!s->parent) return false;
    if (s->parent->children == s) return false;

    Surface *beforePrev = NULL;
    Surface *prev = NULL;
    Surface *c;

    c = s->parent->children;
    while (c != NULL){
        if (c->next == s){
            prev = c;
            break;
        }

        c = c->next;
    }

    if (!prev) return false;

    c = s->parent->children;
    while (c != NULL){
        if (c->next == prev){
            beforePrev = c;
            break;
        }

        c = c->next;
    }

    if (beforePrev)
        beforePrev->next = s;

    if (prev){
        prev->next = s->next;
        s->next = prev;
    }

    if (s->parent->children == prev){
        s->parent->children = s;
    }

    UpdateSurfaceTree(s->tree);
    return true;
}

bool ShiftSurfaceDown(Surface *s){
    if (!s) return false;
    if (!s->parent) return false;
    if (!s->next) return false;

    Surface *prev = NULL;
    Surface *next = s->next;
    Surface *c = s->parent->children;

    while (c != NULL){
        if (c->next == s){
            prev = c;
            break;
        }

        c = c->next;
    }

    s->next = next->next;
    next->next = s;

    if (prev)
        prev->next = next;
    else
        s->parent->children = next;

    UpdateSurfaceTree(s->tree);
    return true;
}

void FlatSurfaceFromFather(Surface *s, bool recursive){
    if (!s) return;
    if (!s->children) return;
    if (!s->parent) return;

    Surface *c = s->children;
    while (c != NULL){
        Surface *cc = c->next;
        ReparentSurface(c, s->parent);
        if (recursive) FlatSurfaceFromFather(c, true);
        c = cc;
    }
}

void FlatSurfaceChildren(Surface *s){
    if (!s) return;
    if (!s->children) return;
    for (Surface *c = s->children; c != NULL; c=c->next){
        FlatSurfaceFromFather(c, true);
    }
}

bool IsSurfaceNameAvailable(SurfaceTree *tree, char *name){
    for (int i=0; i<tree->surfacesQ; i++){
        if (!tree->linearSurfaceTree[i]->name) continue;
        if (strcmp(name, tree->linearSurfaceTree[i]->name) == 0)
            return false;
    }

    return true;
}

char *ResolveSurfaceName(char *ogName, SurfaceTree *st){
    char *rs = calloc(strlen(ogName)+1, sizeof(char));
    sprintf(rs, "%s", ogName);

    while (true){
        if (!IsSurfaceNameAvailable(st, rs)){
            
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

int SetSurfaceName(Surface *s, char *name){
    if (!IsSurfaceNameAvailable(s->tree, name))
        return -1;
    
    if (s->name) free(s->name);
    s->name = calloc(strlen(name)+1, sizeof(char));
    strcpy(s->name, name);
    
    return 0;
}

bool IsSurfaceHidden(Surface *s){
    if (!s) return false;
    if (s->opts & SURFACE_OPT_HIDDEN) 
        return true;
    return IsSurfaceHidden(s->parent);
}

void HideSurface(Surface *s){
    s->opts |= SURFACE_OPT_HIDDEN;
    for (Surface *c = s->children; c != NULL; c = c->next){
        HideSurface(c);
    }
}

void ShowSurface(Surface *s){
    if (IsSurfaceHidden(s->parent)) return;
    s->opts &= ~SURFACE_OPT_HIDDEN;
    for (Surface *c = s->children; c != NULL; c = c->next){
        ShowSurface(c);
    }
}

void ShowHideSurface(Surface *s){
    if (s->opts & SURFACE_OPT_HIDDEN){
        ShowSurface(s);
        return;
    }
    
    HideSurface(s);
}

static void UpdateBoundarieAnchors(Surface *s){
    float w = s->size.x;
    float h = s->size.y;

    Vector2 offset = {
        s->origin.x, 
        s->origin.y
    };
    
    if (s->globalRotation != 0 && s->globalRotation != 180){
        float tmp = w;
        w = h;
        h = tmp;

        tmp = offset.x;
        offset.x = offset.y;
        offset.y = tmp;
    }

    if (s->globalRotation == 90 || s->globalRotation == 180){
        offset.x *= -1;
        offset.y *= -1;
    }

    // TOP
    if (s->boundariesAnchors.top){
        s->boundariesAnchors.top->val = h/2 *-1 - offset.y;
        s->boundariesAnchors.top->opts |= ANCHOR_OPT_FIXED;    
    }
    
    // BOTTOM
    if (s->boundariesAnchors.bottom){
        s->boundariesAnchors.bottom->val = h/2 - offset.y;
        s->boundariesAnchors.bottom->opts |= ANCHOR_OPT_FIXED;    
    }
    
    // LEFT
    if (s->boundariesAnchors.left){
        s->boundariesAnchors.left->val = w/2 *-1 - offset.x;
        s->boundariesAnchors.left->opts |= ANCHOR_OPT_FIXED;    
    }
    
    // RIGHT
    if (s->boundariesAnchors.right){
        s->boundariesAnchors.right->val = w/2 - offset.x;
        s->boundariesAnchors.right->opts |= ANCHOR_OPT_FIXED;    
    }
    
}

void CreateSurfaceBoundarieAnchors(Surface *s){
    if (!s) return;
    if (s->type == SURFACE_EXTENSIBLE){
        SelectAnchor(s->anchors[0], false, false);
        switch (s->anchorAlign){
            case ANCHOR_ALIGN_MIN:
            case ANCHOR_ALIGN_MAX:{
                float val = s->size.y * (s->anchorAlign == ANCHOR_ALIGN_MIN ? -1:1);
                float absVal = GetAnchorAbsValue(s->anchors[0]) + val;
                for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
                    if (a->type != s->anchors[0]->type) continue;
                    if (FloatEquals(GetAnchorAbsValue(a), absVal)) return;
                }

                Anchor *newA = AddAnchorOffset(
                    s->anchors[0],
                    ResolveAnchorName(s->anchors[0]->name, s->tree), 
                    val
                );

                AnchorToggleLock(newA);
            }
            break;

            case ANCHOR_ALIGN_CENTER:{
                float val = s->size.y/2;
                float absVal = GetAnchorAbsValue(s->anchors[0]) + val;
                for (Anchor *a = activeTree->anchors; a != NULL; a = a->next){
                    if (a->type != s->anchors[0]->type) continue;
                    if (FloatEquals(GetAnchorAbsValue(a), absVal)) return;
                }

                for (int i=0; i<2; i++){
                    Anchor *newA = AddAnchorOffset(
                        s->anchors[0],
                        ResolveAnchorName(s->anchors[0]->name, s->tree), 
                        val * (i==0 ? -1:1)
                    );

                    AnchorToggleLock(newA);
                }
            }
            break;
        }
        return;
    }

    char name[2048] = {0};

    float w = s->size.x;
    float h = s->size.y;

    Vector2 offset = {
        s->origin.x, 
        s->origin.y
    };

    if (s->globalRotation != 0 && s->globalRotation != 180){
        float tmp = w;
        w = h;
        h = tmp;

        tmp = offset.x;
        offset.x = offset.y;
        offset.y = tmp;
    }

    if (s->globalRotation == 90 || s->globalRotation == 180){
        offset.x *= -1;
        offset.y *= -1;
    }

    // TOP
    if (!s->boundariesAnchors.top){
        sprintf(name, "%s_top", s->name);
        Anchor *top = AddAnchor(
            s->tree, 
            ResolveAnchorName(name, s->tree),
            ANCHOR_HORIZONTAL, 
            0
        );
        FixAnchorToSurface(top, s);
        s->boundariesAnchors.top = top;
    }

    // BOTTOM
    if (!s->boundariesAnchors.bottom){
        sprintf(name, "%s_bottom", s->name);
        Anchor *bottom = AddAnchor(
            s->tree, 
            ResolveAnchorName(name, s->tree), 
            ANCHOR_HORIZONTAL, 
            0
        );
        FixAnchorToSurface(bottom, s);
        s->boundariesAnchors.bottom = bottom;
    }

    // LEFT
    if (!s->boundariesAnchors.left){
        sprintf(name, "%s_left", s->name);
        Anchor *left = AddAnchor(
            s->tree, 
            ResolveAnchorName(name, s->tree), 
            ANCHOR_VERTICAL, 
            0
        );
        FixAnchorToSurface(left, s);
        s->boundariesAnchors.left = left;
    }

    // RIGHT
    if (!s->boundariesAnchors.right){
        sprintf(name, "%s_right", s->name);
        Anchor *right = AddAnchor(
            s->tree, 
            ResolveAnchorName(name, s->tree), 
            ANCHOR_VERTICAL, 
            0
        );
        FixAnchorToSurface(right, s);
        s->boundariesAnchors.right = right;
    }

    UpdateBoundarieAnchors(s);
}

/* <== Anchors =================================================> */

const char *GetAnchorTypeStr(AnchorType t){
    switch (t){
        case ANCHOR_VERTICAL:   return "ANCHOR_VERTICAL";
        case ANCHOR_HORIZONTAL: return "ANCHOR_HORIZONTAL";
    }
}

Anchor *GetAnchorByIndex(SurfaceTree *tree, int index){
    int i=0;
    Anchor *a = tree->anchors;
    if (!a) return NULL;
    while (true){
        if (index == i)
            return a;

        i++;
        a = a->next;
        if (a == NULL)
            break;        
    }

    return NULL;
}

Anchor *GetAnchorByName(SurfaceTree *tree, char *name){
    Anchor *a = tree->anchors;
    if (!a) return NULL;
    while (true){
        if (strcmp(a->name, name) == 0)
            return a;
        
        a = a->next;
        if (a == NULL)
            break;        
    }

    return NULL;
}

int SetAnchorName(Anchor *a, char *name){
    if (!IsAnchorNameAvailable(a->tree, name))
        return -1;
    
    if (a->name) free(a->name);
    a->name = calloc(strlen(name)+1, sizeof(char));
    strcpy(a->name, name);
    
    return 0;
}

Anchor *AddAnchor(SurfaceTree *tree, char *name, AnchorType type, float val){
    if (!IsAnchorNameAvailable(tree, name)){
        return NULL;
    }
    
    tree->anchorsQ++;
    Anchor *new = calloc(1, sizeof(Anchor));
    new->name = calloc(strlen(name)+1, sizeof(char));
    strcpy(new->name, name);

    new->tree = tree;
    new->type = type;
    new->val = val;

    if (tree->anchors == NULL){
        tree->anchors = new;
        return new;
    }

    Anchor *a = tree->anchors;
    while (a->next != NULL){
        a = a->next;
    }

    a->next = new;
    return new;
}

static bool IsRelated(Anchor *current, Anchor *target, Anchor **visited, size_t *count){
    if (current == NULL) return false;
    if (current == target) return true;

    for (size_t i = 0; i < *count; i++) {
        if (visited[i] == current)
            return false;
    }

    visited[(*count)++] = current;

    if (current->refSurface && IsSurfaceRelated2Anchor(current->refSurface, target)) return true;
    return IsRelated(current->refA, target, visited, count) || IsRelated(current->refB, target, visited, count);
}

static bool AreAnchorsRelated(Anchor *a, Anchor *b){
    if (a == NULL || b == NULL) return false;

    Anchor *visited[1024];
    size_t count = 0;

    return IsRelated(a, b, visited, &count) || IsRelated(b, a, visited, &count);
}

static bool IsSurfaceRelated2Anchor(Surface *s, Anchor *a){
    if (!s || !a) return false;

    for (Surface *ss = s; ss != NULL; ss = ss->parent){
        for (int i=0; i<sizeof(ss->anchors)/sizeof(ss->anchors[0]); i++){
            Anchor *aa = ss->anchors[i];

            if (AreAnchorsRelated(a, aa))
                return true;
        }
    }

    return false;
}

// this sucks
#define RestoreAnchorSurfaceRefPT1()                                          \
    bool topCutout       = false;                                             \
    bool bottomCutout    = false;                                             \
    bool leftCutout      = false;                                             \
    bool rightCutout     = false;                                             \
    bool topboundarie    = false;                                             \
    bool bottomboundarie = false;                                             \
    bool leftboundarie   = false;                                             \
    bool rightboundarie  = false;                                             \
    if (refSurfaceBackup){                                                    \
        topCutout = refSurfaceBackup->cutoutAnchors.top == a;                 \
        bottomCutout = refSurfaceBackup->cutoutAnchors.bottom == a;           \
        leftCutout = refSurfaceBackup->cutoutAnchors.left == a;               \
        rightCutout = refSurfaceBackup->cutoutAnchors.right == a;             \
        topboundarie = refSurfaceBackup->boundariesAnchors.top == a;          \
        bottomboundarie = refSurfaceBackup->boundariesAnchors.bottom == a;    \
        leftboundarie = refSurfaceBackup->boundariesAnchors.left == a;        \
        rightboundarie = refSurfaceBackup->boundariesAnchors.right == a;      \
    }                                                                         \

#define RestoreAnchorSurfaceRefPT2()                                          \
    if (refSurfaceBackup){                                                    \
        if (topCutout) refSurfaceBackup->cutoutAnchors.top = a;               \
        if (bottomCutout) refSurfaceBackup->cutoutAnchors.bottom = a;         \
        if (leftCutout) refSurfaceBackup->cutoutAnchors.left = a;             \
        if (rightCutout) refSurfaceBackup->cutoutAnchors.right = a;           \
        if (topboundarie) refSurfaceBackup->boundariesAnchors.top = a;        \
        if (bottomboundarie) refSurfaceBackup->boundariesAnchors.bottom = a;  \
        if (leftboundarie) refSurfaceBackup->boundariesAnchors.left = a;      \
        if (rightboundarie) refSurfaceBackup->boundariesAnchors.right = a;    \
    }                                                                         \


int SetAnchorAsOffset(Anchor *a, Anchor *b){
   if (!a || !b) return -1;
   if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return -3;
   if (a->type != b->type) return -2;
   
    AnchorConstraint constraintBackup = a->constraint;
    Anchor *refABackup = a->refA;
    Anchor *refBBackup = a->refB;
    Surface *refSurfaceBackup = a->refSurface;
    RestoreAnchorSurfaceRefPT1();
    float valueBackup = a->val;

    AnchorFreeConstraint(a);
   
    if (AreAnchorsRelated(a, b)){
        a->constraint = constraintBackup;
        a->refA = refABackup;
        a->refB = refBBackup;
        a->val = valueBackup;
        a->refSurface = refSurfaceBackup;
        RestoreAnchorSurfaceRefPT2();
        return -4;
    }
   
   float aAbs = GetAnchorAbsValue(a);
   a->constraint = ANCHOR_CONSTRAINT_OFFSET;
   a->refA = b;
   a->refB = NULL;
   a->refSurface = NULL;
   SetAnchorAbsValue(a, aAbs);
   return 0; 
}

static int SetAnchorAs(Anchor *a, Anchor *b, Anchor *c, AnchorConstraint constraint){
    if (!a || !b || !c) return -1;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return -3;
    if (a->type != b->type || a->type != c->type) return -2;
    
    AnchorConstraint constraintBackup = a->constraint;
    Anchor *refABackup = a->refA;
    Anchor *refBBackup = a->refB;
    Surface *refSurfaceBackup = a->refSurface;
    RestoreAnchorSurfaceRefPT1();
    float valueBackup = a->val;

    AnchorFreeConstraint(a);
    
    if (AreAnchorsRelated(a, b)){
        a->constraint = constraintBackup;
        a->refA = refABackup;
        a->refB = refBBackup;
        a->val = valueBackup;
        a->refSurface = refSurfaceBackup;
        RestoreAnchorSurfaceRefPT2();
        return -4;
    }

    if (AreAnchorsRelated(a, c)){
        a->constraint = constraintBackup;
        a->refA = refABackup;
        a->refB = refBBackup;
        a->val = valueBackup;
        a->refSurface = refSurfaceBackup;
        RestoreAnchorSurfaceRefPT2();
        return -4;
    }

    a->opts = a->opts & ~ANCHOR_OPT_FIXED;
    a->constraint = constraint;
    a->refA = b;
    a->refB = c;
    a->refSurface = NULL;
    return 0;
}

int SetAnchorAsCenter(Anchor *a, Anchor *b, Anchor *c){
    return SetAnchorAs(a, b, c, ANCHOR_CONSTRAINT_CENTER);
}

int SetAnchorAsMirror(Anchor *a, Anchor *b, Anchor *c){
    return SetAnchorAs(a, b, c, ANCHOR_CONSTRAINT_MIRROR);
}

int SetAnchorAsProjection(Anchor *a, Anchor *b, bool inverted){
    if (!a || !b) return -1;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return -3;
    if (a->type != b->type) return -2;
    
    AnchorConstraint constraintBackup = a->constraint;
    Anchor *refABackup = a->refA;
    Anchor *refBBackup = a->refB;
    Surface *refSurfaceBackup = a->refSurface;
    RestoreAnchorSurfaceRefPT1();
    float valueBackup = a->val;

    AnchorFreeConstraint(a);
   
    if (AreAnchorsRelated(a, b)){
        a->constraint = constraintBackup;
        a->refA = refABackup;
        a->refB = refBBackup;
        a->val = valueBackup;
        a->refSurface = refSurfaceBackup;
        RestoreAnchorSurfaceRefPT2();
        return -4;
    }
      
    a->constraint = inverted ? ANCHOR_CONSTRAINT_INVERSED_PROJECTION : ANCHOR_CONSTRAINT_PROJECTION;
    a->refA = b;
    a->refB = NULL;
    a->refSurface = NULL;
    return 0; 
}

int FixAnchorToSurface(Anchor *a, Surface *s){
    if (!a || !s) return -1;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return -2;
    
    AnchorConstraint constraintBackup = a->constraint;
    Anchor *refABackup = a->refA;
    Anchor *refBBackup = a->refB;
    Surface *refSurfaceBackup = a->refSurface;
    RestoreAnchorSurfaceRefPT1();
    float valueBackup = a->val;

    AnchorFreeConstraint(a);

    if (IsSurfaceRelated2Anchor(s, a)){
        a->constraint = constraintBackup;
        a->refA = refABackup;
        a->refB = refBBackup;
        a->val = valueBackup;
        a->refSurface = refSurfaceBackup;
        RestoreAnchorSurfaceRefPT2();
        return -4;
    }

    a->constraint = ANCHOR_CONSTRAINT_SURFACE;
    a->refA = NULL;
    a->refB = NULL;
    a->refSurface = s;
    a->val = 0.0f;

    for (Anchor *aa = activeTree->anchors; aa != NULL; aa = aa->next){
        if (aa == a) continue;
        if (aa->type == a->type && a->refSurface == s){
            if (FloatEquals(a->val, aa->val))
                a->val += 10.0f;
        }
    }

    return 0;
}

Anchor *AddAnchorOffset(Anchor *ref, char *name, float val){
    Anchor *rs = AddAnchor(ref->tree, name, ref->type, val);
    if (!rs) return NULL;
    SetAnchorAsOffset(rs, ref);
    rs->val = val;
    return rs;
}

Anchor *AddAnchorCenter(Anchor *refA, Anchor *refB, char *name){
    if (!refA || !refB) return NULL;
    if (refA->type != refB->type) return NULL;
    
    Anchor *rs = AddAnchor(refA->tree, name, refA->type, 0);
    if (!rs) return NULL;
    SetAnchorAsCenter(rs, refA, refB);
    return rs;
}

Anchor *AddAnchorMirror(Anchor *refA, Anchor *refB, char *name){
    if (!refA || !refB) return NULL;
    if (refA->type != refB->type) return NULL;
    
    Anchor *rs = AddAnchor(refA->tree, name, refA->type, 0);
    if (!rs) return NULL;
    SetAnchorAsMirror(rs, refA, refB);
    return rs;
}

// do NOT use this function manually, use "DeleteAnchor"
// this one is used only when clearing EVERY anchor
int DeleteAnchorForced(Anchor *a){
    if (a == NULL) return -1;
    if (a->tree == NULL) return -1;

    if (a->refSurface){
        // boundaries
        if (a == a->refSurface->boundariesAnchors.top)    a->refSurface->boundariesAnchors.top = NULL;
        if (a == a->refSurface->boundariesAnchors.bottom) a->refSurface->boundariesAnchors.bottom = NULL;
        if (a == a->refSurface->boundariesAnchors.left)   a->refSurface->boundariesAnchors.left = NULL;
        if (a == a->refSurface->boundariesAnchors.right)  a->refSurface->boundariesAnchors.right = NULL;

        // cutout
        if (a == a->refSurface->cutoutAnchors.top)    a->refSurface->cutoutAnchors.top = NULL;        
        if (a == a->refSurface->cutoutAnchors.bottom) a->refSurface->cutoutAnchors.bottom = NULL;
        if (a == a->refSurface->cutoutAnchors.left)   a->refSurface->cutoutAnchors.left = NULL;
        if (a == a->refSurface->cutoutAnchors.right)  a->refSurface->cutoutAnchors.right = NULL;
    }
    
    Annotation *ann = a->tree->annotations;
    while (ann != NULL){
        Annotation *next = ann->next;
        if (ann->A == a || ann->B == a)
            DeleteAnnotation(ann);
        ann = next;
    }

    if (a->tree->anchors == a){
        a->tree->anchorsQ--;
        a->tree->anchors = a->next;
        free(a->name);
        free(a);
        return 0;
    }
    
    Anchor *tmp = a->tree->anchors;
    while (tmp->next != NULL){
        if (tmp->next == a) 
            break;
        tmp = tmp->next;
    }

    if (tmp->next == a){
        a->tree->anchorsQ--;
        tmp->next = a->next;
        free(a->name);
        free(a);
    }

    return 0;
}

int DeleteAnchor(Anchor *a, SurfaceTree **treeList, int treeListQ){
    if (a == NULL) return -1;
    if (a->tree == NULL) return -1;
    if (IsAnchorReferencedByAnchor(a, treeList, treeListQ)) return -2;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return -3;
    if (IsAnchorReferencedBySurface(a)) return -4;
    return DeleteAnchorForced(a);
}

float GetAnchorAbsValue(Anchor *a){
    switch (a->constraint){

        case ANCHOR_CONSTRAINT_NONE:
            return a->val;

        case ANCHOR_CONSTRAINT_OFFSET:
            return GetAnchorAbsValue(a->refA) + a->val;

        case ANCHOR_CONSTRAINT_SURFACE:{
            Vector2 sPos = a->refSurface->globalPos;
            return (a->type == ANCHOR_VERTICAL ? sPos.x:sPos.y) + a->val;
        }
            
        // solver solves this ones
        case ANCHOR_CONSTRAINT_CENTER:              return a->val;
        case ANCHOR_CONSTRAINT_MIRROR:              return a->val;
        case ANCHOR_CONSTRAINT_PROJECTION:          return a->val;
        case ANCHOR_CONSTRAINT_INVERSED_PROJECTION: return a->val;
        
        break;

    }

    return 0.0f;
}

void SetAnchorAbsValue(Anchor *a, float val){
    switch (a->constraint){
        
        case ANCHOR_CONSTRAINT_NONE:
            a->val = val;
            return;

        case ANCHOR_CONSTRAINT_OFFSET:
            a->val = val - GetAnchorAbsValue(a->refA);
            return;

        case ANCHOR_CONSTRAINT_SURFACE:{
            Vector2 sPos = a->refSurface->globalPos;
            a->val = val - (a->type == ANCHOR_VERTICAL ? sPos.x:sPos.y);
            return;
        }
            
        // solver do this any way no?
        case ANCHOR_CONSTRAINT_CENTER: return;
        case ANCHOR_CONSTRAINT_MIRROR: return;
        case ANCHOR_CONSTRAINT_PROJECTION: return;
        case ANCHOR_CONSTRAINT_INVERSED_PROJECTION: return;
    }
}

bool IsAnchorReferencedByAnchor(Anchor *a, SurfaceTree **treeList, int treeListQ){
    for (int i=0; i<treeListQ; i++){
        for (Anchor *aa = treeList[i]->anchors; aa != NULL; aa = aa->next){
            if (aa->refA == a) return true;
            if (aa->refB == a) return true;
        }
    }
    
    return false;
}

bool IsAnchorReferencedBySurface(Anchor *a){
    for (int i=0; i<a->tree->surfacesQ; i++){
        for (int o=0; o<3; o++){
            if (a->tree->linearSurfaceTree[i]->anchors[o] == a)
                return true;
        }
    }

    return false;
}

bool IsAnchorLocked(Anchor *a){
    if (!a) return false;

    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return true;
    if (a->opts & ANCHOR_OPT_FIXED) return true;
    if (a->constraint != ANCHOR_CONSTRAINT_NONE && 
        a->constraint != ANCHOR_CONSTRAINT_OFFSET && 
        a->constraint != ANCHOR_CONSTRAINT_SURFACE )
            return true;
    return false;
}

void SelectAnchor(Anchor *a, bool aditive, bool canDeselect){
    if (aditive){
        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
            if (a->tree->anchorSelection[i] == a) break;
            if (a->tree->anchorSelection[i] == NULL || i == ANCHOR_SELECTION_BUF_LEN-1){
                a->tree->anchorSelection[i] = a;
                break;
            }
        }
    }

    else {
        
        if (a->tree->anchorSelection[0] == a && canDeselect){
            for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++)
                a->tree->anchorSelection[i] = NULL;
        }
        
        else {
            for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++)
                a->tree->anchorSelection[i] = NULL;
            a->tree->anchorSelection[0] = a;
        }
    }
    
}

bool IsAnchorSelected(Anchor *a){
    if (!a) return false;
    for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
        if (a->tree->anchorSelection[i] == a)
            return true;
    }
    
    return false;
}

bool IsAnchorBase(Anchor *a){
    if (strcmp(a->name, "TopMost") == 0)    return true;
    if (strcmp(a->name, "BottomMost") == 0) return true;
    if (strcmp(a->name, "LeftMost") == 0)   return true;
    if (strcmp(a->name, "RightMost") == 0)  return true;
    return false;
}

void AnchorToggleLock(Anchor *a){
    if (!a) return;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return;
    if (a->constraint != ANCHOR_CONSTRAINT_NONE && 
        a->constraint != ANCHOR_CONSTRAINT_OFFSET && 
        a->constraint != ANCHOR_CONSTRAINT_SURFACE ) 
            return;
    a->opts ^= ANCHOR_OPT_FIXED;
}

void AnchorToggleHidden(Anchor *a){
    if (!a) return;
    a->opts ^= ANCHOR_OPT_HIDDEN;
}

void AnchorFreeConstraint(Anchor *a){
    if (!a) return;
    a->val = GetAnchorAbsValue(a);
    a->constraint = ANCHOR_CONSTRAINT_NONE;
    a->refA = NULL;
    a->refB = NULL;
    if (a->refSurface){
        if (a->refSurface->cutoutAnchors.top == a) a->refSurface->cutoutAnchors.top = NULL;
        if (a->refSurface->cutoutAnchors.bottom == a) a->refSurface->cutoutAnchors.bottom = NULL;
        if (a->refSurface->cutoutAnchors.left == a) a->refSurface->cutoutAnchors.left = NULL;
        if (a->refSurface->cutoutAnchors.right == a) a->refSurface->cutoutAnchors.right = NULL;
        if (a->refSurface->boundariesAnchors.top == a) a->refSurface->boundariesAnchors.top = NULL;
        if (a->refSurface->boundariesAnchors.bottom == a) a->refSurface->boundariesAnchors.bottom = NULL;
        if (a->refSurface->boundariesAnchors.left == a) a->refSurface->boundariesAnchors.left = NULL;
        if (a->refSurface->boundariesAnchors.right == a) a->refSurface->boundariesAnchors.right = NULL;
    }

    a->refSurface = NULL;
}

bool CanAnchorValBeChanged(Anchor *a){
    if (a == NULL) return false;
    if (a->constraint == ANCHOR_CONSTRAINT_CENTER) return false;
    if (a->constraint == ANCHOR_CONSTRAINT_MIRROR) return false;
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE) return false;
    if (a->opts & ANCHOR_OPT_FIXED) return false;
    return true;
}

const char *GetAnchorConstraintStr(Anchor *a){
    if (!a) return NULL;
    
    switch (a->constraint){
        case ANCHOR_CONSTRAINT_NONE:                return "[NONE]";
        case ANCHOR_CONSTRAINT_OFFSET:              return "[OFFSET]";
        case ANCHOR_CONSTRAINT_CENTER:              return "[CENTER]";
        case ANCHOR_CONSTRAINT_MIRROR:              return "[MIRROR]";
        case ANCHOR_CONSTRAINT_PROJECTION:          return "[PROJECTION]";
        case ANCHOR_CONSTRAINT_INVERSED_PROJECTION: return "[INVERSED PROJECTION]";
        case ANCHOR_CONSTRAINT_SURFACE:             return "[SURFACE]";
    }

    return "[NONE]";
}

const char *GetAnchorAlignStr(Surface *s){
    if (!s) return NULL;
    if (s->type != SURFACE_EXTENSIBLE) return NULL;
    switch (s->anchorAlign){
        case ANCHOR_ALIGN_CENTER: return "CENTER";
        case ANCHOR_ALIGN_MIN:    return "MIN";
        case ANCHOR_ALIGN_MAX:    return "MAX";
    }
}

Anchor *GetAnchorAtLeft(Anchor *a, Anchor *b){
    if (GetAnchorAbsValue(a) < GetAnchorAbsValue(b)) return a;
    return b;
}

Anchor *GetAnchorAtTop(Anchor *a, Anchor *b){
    return GetAnchorAtLeft(a, b);
}

float GetDistanceBetweenAnchors(Anchor *a, Anchor *b){
    if (GetAnchorAbsValue(a) < GetAnchorAbsValue(b)) 
        return GetAnchorAbsValue(b) - GetAnchorAbsValue(a);
    return GetAnchorAbsValue(a) - GetAnchorAbsValue(b);
}

bool IsAnchorNameAvailable(SurfaceTree *t, char *n) {
    if (t->anchors == NULL)
        return true;

    Anchor *a = t->anchors;
    while (true) {
        if (strcmp(n, a->name) == 0)
            return false;
        
        a = a->next;
        if (a == NULL)
            break;
    }

    return true;
}

char *ResolveAnchorName(char *ogName, SurfaceTree *st){
    char *rs = calloc(strlen(ogName)+1, sizeof(char));
    sprintf(rs, "%s", ogName);

    while (true){
        if (!IsAnchorNameAvailable(st, rs)){
            
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
                        
                        sprintf(newName + underScoreIndex + 1, "%i", tmp);

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

void Sort3Anchors(Anchor **anchors){
    if (anchors[0]->type == anchors[1]->type) {
        Anchor *tmp = anchors[0];
        anchors[0] = anchors[2];
        anchors[2] = tmp;
    }
    else if (anchors[0]->type == anchors[2]->type) {
        Anchor *tmp = anchors[0];
        anchors[0] = anchors[1];
        anchors[1] = tmp;
    }
}

bool IsAnchorPartOfCutout(Anchor *a){
    if (!a->refSurface) return false;
    if (a->refSurface->cutoutAnchors.top == a)    return true;
    if (a->refSurface->cutoutAnchors.bottom == a) return true;
    if (a->refSurface->cutoutAnchors.left == a)   return true;
    if (a->refSurface->cutoutAnchors.right == a)  return true;
    return false;
}

bool IsAnchorPartOfBoundary(Anchor *a){
    if (!a->refSurface) return false;
    if (a->refSurface->boundariesAnchors.top == a)    return true;
    if (a->refSurface->boundariesAnchors.bottom == a) return true;
    if (a->refSurface->boundariesAnchors.left == a)   return true;
    if (a->refSurface->boundariesAnchors.right == a)  return true;
    return false;
}

LinePoints GetAnchorScreenPoints(OG_Viewport *v, Anchor *a){
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

/* <== Annotations =============================================> */

Annotation *AddAnnotation(SurfaceTree *tree, Anchor *A, Anchor *B){
    if (A->type != B->type) return NULL;

    Annotation *a = tree->annotations;
    while (a != NULL){
        if ((a->A == A && a->B == B) || (a->A == B && a->B == A))
            return a;
        a = a->next;
    }

    Annotation *new = calloc(1, sizeof(Annotation));
    new->tree = tree;
    new->A = A;
    new->B = B;

    tree->annotationsQ++;
    if (tree->annotations == NULL){
        tree->annotations = new;
        return new;
    }

    a = tree->annotations;
    while (a->next != NULL)
        a = a->next;
    
    a->next = new;
    return new;
}

void DeleteAnnotation(Annotation *a){
    if (!a) return;

    if (a->tree->selectedAnnotation == a)
        a->tree->selectedAnnotation = NULL;

    SurfaceTree *tree = a->tree;
    Annotation *cur = tree->annotations;
    Annotation *prev = NULL;

    while (cur != NULL){
        if (cur == a){
            if (prev == NULL) tree->annotations = cur->next;
            else prev->next = cur->next;
            tree->annotationsQ--;
            free(cur);
            return;
        }

        prev = cur;
        cur = cur->next;
    }
}

void DrawAnnotation(OG_Viewport *v, Annotation *a, Color c, float arrowSize, float fontSize, float scale, Vector2 drawOffset){
    if (!a) return;
    if (!a->A) return;
    if (!a->B) return;
    if (a->opts & ANNOTATION_OPT_HIDDEN) return;

    float A = GetAnchorAbsValue(a->A) * scale + (a->A->type == ANCHOR_VERTICAL ? drawOffset.x : drawOffset.y);
    float B = GetAnchorAbsValue(a->B) * scale + (a->A->type == ANCHOR_VERTICAL ? drawOffset.x : drawOffset.y);
    float offset = a->offset * scale + (a->A->type == ANCHOR_VERTICAL ? drawOffset.y : drawOffset.x);
    float extension = a->extensionLine * scale;

    float size = arrowSize * scale;
    float dir = (B > A) ? 1.0f : -1.0f;

    switch (a->A->type){
        case ANCHOR_VERTICAL:{
            Vector2 points[] = {
                { A, offset }, // start
                { B, offset }, // end
                { A + (size*dir), offset + size/2 },
                { A + (size*dir), offset - size/2 },
                { B + (size*(dir*-1)), offset + size/2 },
                { B + (size*(dir*-1)), offset - size/2 },
            };

            for (int i=0; i<sizeof(points)/sizeof(points[0]); i++){
                points[i] = GetWorldToScreen2D(points[i], v->camera);
            }
            
            DrawCadLine(points[0], points[1], 1.0f, c);
            
            // First Triangle (A)
            DrawCadLine( points[0], points[2], 1.0f, c);
            DrawCadLine( points[2], points[3], 1.0f, c);
            DrawCadLine( points[0], points[3], 1.0f, c);

            // Second Triangle (B)
            DrawCadLine( points[1], points[4], 1.0f, c);
            DrawCadLine( points[4], points[5], 1.0f, c);
            DrawCadLine( points[1], points[5], 1.0f, c);
            
            Vector2 tm = MeasureCadText(cadFont, a->lenStr, 0, fontSize, 0, 1.0f);
            DrawCadText(
                cadFont,
                a->lenStr, 
                Vector2Lerp(points[0], points[1], 0.5f),
                (Vector2){tm.x/2,(a->opts & ANNOTATION_OPT_FLIP_LABEL_SIDE) ? (-tm.y):0}, 
                0, 
                fontSize,
                0.0f,
                1.0f,
                CAD_ALIGN_CENTER,
                c
            );

            // Draw extension lines
            if (a->opts & ANNOTATION_OPT_EXTENSION_ENABLE){
                DrawCadLine(
                    GetWorldToScreen2D(
                        (Vector2){A, offset + (extension > 0 ? (size/2)*-1:(size/2))}, 
                        v->camera
                    ), 
                    GetWorldToScreen2D(
                        (Vector2){A, offset + extension}, 
                        v->camera
                    ), 
                    1.0f, 
                    c
                );

                DrawCadLine(
                    GetWorldToScreen2D(
                        (Vector2){B, offset + (extension > 0 ? (size/2)*-1:(size/2))}, 
                        v->camera
                    ), 
                    GetWorldToScreen2D(
                        (Vector2){B, offset + extension}, 
                        v->camera
                    ), 
                    1.0f, 
                    c
                );
            }

            break;
        }

        case ANCHOR_HORIZONTAL:{
            Vector2 points[] = {
                { offset, A }, // start
                { offset, B }, // end
                { offset + size/2, A + (size*dir) },
                { offset - size/2, A + (size*dir) },
                { offset + size/2, B + (size*(dir*-1)) },
                { offset - size/2, B + (size*(dir*-1)) },
            };

            for (int i = 0; i < sizeof(points)/sizeof(points[0]); i++){
                points[i] = GetWorldToScreen2D(points[i], v->camera);
            }

            DrawCadLine(points[0], points[1], 1.0f, c);

            // First Triangle (A)
            DrawCadLine(points[0], points[2], 1.0f, c);
            DrawCadLine(points[2], points[3], 1.0f, c);
            DrawCadLine(points[0], points[3], 1.0f, c);

            // Second Triangle (B)
            DrawCadLine(points[1], points[4], 1.0f, c);
            DrawCadLine(points[4], points[5], 1.0f, c);
            DrawCadLine(points[1], points[5], 1.0f, c);

            Vector2 tm = MeasureCadText(cadFont, a->lenStr, 0, fontSize, 0, 1.0f);
            DrawCadText(
                cadFont,
                a->lenStr, 
                Vector2Lerp(points[0], points[1], 0.5f),
                (Vector2){tm.x/2,(a->opts & ANNOTATION_OPT_FLIP_LABEL_SIDE) ? (-tm.y):0}, 
                90, 
                fontSize, 
                0.0f,
                1.0f,
                CAD_ALIGN_CENTER,
                c
            );

            // Draw extension lines
            if (a->opts & ANNOTATION_OPT_EXTENSION_ENABLE){
                DrawCadLine(
                    GetWorldToScreen2D(
                        (Vector2){offset + (extension > 0 ? (size/2)*-1:(size/2)), A}, 
                        v->camera
                    ), 
                    GetWorldToScreen2D(
                        (Vector2){offset + extension, A}, 
                        v->camera
                    ),
                    1.0f, 
                    c
                );

                DrawCadLine(
                    GetWorldToScreen2D(
                        (Vector2){offset + (extension > 0 ? (size/2)*-1:(size/2)), B}, 
                        v->camera
                    ), 
                    GetWorldToScreen2D(
                        (Vector2){offset + extension, B}, 
                        v->camera
                    ),
                    1.0f, 
                    c
                );
            }

            break;
        }
    }
}