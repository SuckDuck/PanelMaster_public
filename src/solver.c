// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "surfaces.h"
#include "main.h"
#include "utils.h"

#define SOLVER_PASSES 2

static void SolveBaseSurfaces(SurfaceTree *tree){
    tree->baseSurface.pos = (Vector2){ 0, 0 };
}

static void SolveSurfaces(Surface *s, Vector2 offset, float rotationOffset){
    Vector2 tmpOffset = offset;
    float tmpRotationOffset = rotationOffset;

    s->globalPos = Vector2RotateDegrees(s->pos, tmpRotationOffset);
    s->globalPos.x += tmpOffset.x;
    s->globalPos.y += tmpOffset.y;
    s->globalRotation = (int)(s->rotation + tmpRotationOffset);
    
    for (Surface *c = s->children; c != NULL; c = c->next)
        SolveSurfaces(c, s->globalPos, s->globalRotation);
}

static void SolveAnchors(SurfaceTree *tree){
    Anchor *a = tree->anchors;
    if (a){
        while (true){

            switch (a->constraint){
                case ANCHOR_CONSTRAINT_NONE: 
                case ANCHOR_CONSTRAINT_OFFSET: break;
                case ANCHOR_CONSTRAINT_SURFACE: break;

                case ANCHOR_CONSTRAINT_CENTER:{
                    a->val = (GetAnchorAbsValue(a->refA) + GetAnchorAbsValue(a->refB))/2;
                } break;

                case ANCHOR_CONSTRAINT_MIRROR:{
                    float d = GetAnchorAbsValue(a->refA) - GetAnchorAbsValue(a->refB);
                    a->val = GetAnchorAbsValue(a->refB) - d;
                } break;

                case ANCHOR_CONSTRAINT_PROJECTION:{
                    a->val = GetAnchorAbsValue(a->refA);
                } break;

                case ANCHOR_CONSTRAINT_INVERSED_PROJECTION:{
                    a->val = GetAnchorAbsValue(a->refA)*-1;
                } break;

            }

            // Base Anchors
            if (strcmp(a->name, "TopMost") == 0) a->val = tree->baseSurface.size.y/2 *-1;            
            if (strcmp(a->name, "BottomMost") == 0) a->val = a->tree->baseSurface.size.y/2;                    
            if (strcmp(a->name, "LeftMost") == 0) a->val = a->tree->baseSurface.size.x/2 *-1;
            if (strcmp(a->name, "RightMost") == 0) a->val = a->tree->baseSurface.size.x/2;
            
            a = a->next;
            if (a== NULL)
                break;        
        }
    }
}

static void SolveComponents(SurfaceTree *tree){
    for (int i=0; i<tree->surfacesQ; i++){
        Surface *s = tree->linearSurfaceTree[i];
        if (s->type != SURFACE_COMPONENT)
            continue;
            
        // fix to X
        if (s->anchors[0]){
            float val = GetAnchorAbsValue(s->anchors[0]);
            Vector2 pos = s->globalPos;
            pos.x = val;
            SetSurfaceGlobalPos(s, pos);
        }

        // fix to Y
        if (s->anchors[1]){
            float val = GetAnchorAbsValue(s->anchors[1]);
            Vector2 pos = s->globalPos;
            pos.y = val;
            SetSurfaceGlobalPos(s, pos);
        }

    }
}

static void SolveExtensibles(SurfaceTree *tree){
    for (int i=0; i<tree->surfacesQ; i++){
        Surface *s = tree->linearSurfaceTree[i];
        if (s->type != SURFACE_EXTENSIBLE)
            continue;
            
        Vector2 p;
        if (s->anchors[0]->type == ANCHOR_HORIZONTAL){
            switch (s->anchorAlign) {
                case ANCHOR_ALIGN_CENTER:{
                    p.y = GetAnchorAbsValue(s->anchors[0]);
                    break;
                }

                case ANCHOR_ALIGN_MIN:{
                    p.y = GetAnchorAbsValue(s->anchors[0]) - s->size.y/2;
                    break;
                }

                case ANCHOR_ALIGN_MAX:{
                    p.y = GetAnchorAbsValue(s->anchors[0]) + s->size.y/2;
                    break;
                }
            }

            
            p.x = (GetAnchorAbsValue(s->anchors[1]) + GetAnchorAbsValue(s->anchors[2]))/2;
            SetSurfaceGlobalPos(s, p);
            s->size.x = fabsf(GetAnchorAbsValue(s->anchors[1]) - GetAnchorAbsValue(s->anchors[2]));
        }

        if (s->anchors[0]->type == ANCHOR_VERTICAL){
            switch (s->anchorAlign) {
                case ANCHOR_ALIGN_CENTER:{
                    p.x = GetAnchorAbsValue(s->anchors[0]);
                    break;
                }

                case ANCHOR_ALIGN_MIN:{
                    p.x = GetAnchorAbsValue(s->anchors[0]) - s->size.y/2;
                    break;
                }

                case ANCHOR_ALIGN_MAX:{
                    p.x = GetAnchorAbsValue(s->anchors[0]) + s->size.y/2;
                    break;
                }
            }
            
            p.y = (GetAnchorAbsValue(s->anchors[1]) + GetAnchorAbsValue(s->anchors[2]))/2;
            SetSurfaceGlobalPos(s, p);
            s->size.x = fabsf(GetAnchorAbsValue(s->anchors[1]) - GetAnchorAbsValue(s->anchors[2]));
        }
        
    }
}

static void SolveAnnotations(SurfaceTree *tree){
    Annotation *ann = tree->annotations;
    while (ann != NULL){
        float len = GetAnchorAbsValue(ann->B) - GetAnchorAbsValue(ann->A);
        if (len<0) len *= -1;
        snprintf(ann->lenStr, sizeof(ann->lenStr), "%.0f mm", len);

        ann = ann->next;
    }
}

void Solver(){
    for (int i=0; i<treeListQ; i++){
        SurfaceTree *tree = treeList[i];
        if (tree){
            for (int i=0; i<SOLVER_PASSES; i++){
                SolveBaseSurfaces(tree);
                SolveAnchors(tree);
                SolveAnnotations(tree);
                SolveSurfaces(
                    &tree->baseSurface, 
                    Vector2Zero(), 
                    0.0f
                );
                SolveComponents(tree);
                SolveExtensibles(tree);
            }
        }

    }
}