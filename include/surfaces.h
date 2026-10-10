// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef SURFACES_H
#define SURFACES_H

#include <raylib.h>
#include <stdint.h>
#include "origami.h"
#include "utils.h"

#define SURFACE_SELECTION_BUF_LEN 2
#define ANCHOR_SELECTION_BUF_LEN 4

typedef struct SurfaceTree SurfaceTree;
typedef struct Surface Surface;

typedef enum SurfaceOptions {
    SURFACE_OPT_FIXED_X   = 1 << 0,
    SURFACE_OPT_FIXED_Y   = 1 << 1,
    SURFACE_OPT_HIDDEN    = 1 << 2
} SurfaceOptions;

typedef enum AnchorType {
    ANCHOR_HORIZONTAL,
    ANCHOR_VERTICAL
} AnchorType;

// CHECK THESE FUNCTIONS WHEN ADDING A NEW CONSTRAINT

// GetAnchorAbsValue
// SetAnchorAbsValue
// IsAnchorLocked
// AnchorToggleLock
// GetAnchorConstraintStr
// Solver -> SolveAnchors
typedef enum AnchorConstraint {
    ANCHOR_CONSTRAINT_NONE,
    ANCHOR_CONSTRAINT_OFFSET,
    ANCHOR_CONSTRAINT_CENTER,
    ANCHOR_CONSTRAINT_MIRROR,
    ANCHOR_CONSTRAINT_PROJECTION,
    ANCHOR_CONSTRAINT_INVERSED_PROJECTION,
    ANCHOR_CONSTRAINT_SURFACE
} AnchorConstraint;

typedef enum AnchorOpts {
    ANCHOR_OPT_FIXED           = 1 << 0,
    ANCHOR_OPT_FIXED_BY_ENGINE = 1 << 1,
    ANCHOR_OPT_HIDDEN          = 1 << 2
} AnchorOpts;

typedef enum AnchorAlign {
    ANCHOR_ALIGN_CENTER,
    ANCHOR_ALIGN_MIN,
    ANCHOR_ALIGN_MAX
} AnchorAlign;

typedef struct Anchor {
    char *name;
    SurfaceTree *tree;
    AnchorType type;
    AnchorConstraint constraint;
    AnchorOpts opts;
    Surface *refSurface;
    struct Anchor *refA;
    struct Anchor *refB;
    float val;
    struct Anchor *next;
} Anchor;

typedef enum AnnotationOpts {
    ANNOTATION_OPT_FLIP_LABEL_SIDE  = 1 << 0,
    ANNOTATION_OPT_EXTENSION_ENABLE = 1 << 1,
    ANNOTATION_OPT_HIDDEN           = 1 << 2
} AnnotationOpts;

typedef struct Annotation {
    SurfaceTree *tree;
    Anchor *A;
    Anchor *B;
    char lenStr[15];
    float offset;
    float extensionLine;
    uint32_t opts;
    struct Annotation *next;
} Annotation;

typedef struct SkinLine {
    int A;
    int B;
} SkinLine;

typedef struct SkinLod {
    SkinLine *lines;
    int linesQ;
    float zoom;
} SkinLod;

typedef struct Skin {
    Surface *surface;
    Vector2 *points;
    int pointsQ;
    SkinLod *lods;
    int lodsQ;
    float len;
    Color tint;
} Skin;

typedef enum SurfaceType {
    SURFACE_COMPONENT,
    SURFACE_EXTENSIBLE
} SurfaceType;

typedef struct Surface {
    char sourcePath[OG_PATH_MAX];
    char *id;
    char *name;
    SurfaceType type;
    Vector2 pos;
    Vector2 globalPos;
    Vector2 origin;
    float rotation;
    float globalRotation;
    Vector2 size;
    int treeDepth;
    uint32_t opts;

    /* set the limits for extensibles */
    /* and fix surfaces to anchors for the others */
    Anchor *anchors[3];
    AnchorAlign anchorAlign;
    struct {
        Anchor *top;
        Anchor *bottom;
        Anchor *left;
        Anchor *right;
    } boundariesAnchors;

    struct {
        Anchor *top;
        Anchor *bottom;
        Anchor *left;
        Anchor *right;
    } cutoutAnchors;

    float leftCut;
    float rightCut;

    Skin skin;
    struct {
        Vector2 *points;
        int len;
    } cutout;

    struct SurfaceTree *tree;
    struct Surface *parent;
    struct Surface *children;
    struct Surface *next;
} Surface;

typedef struct SurfaceTree {
    Surface baseSurface;
    Surface *lastInsertedSurface;
    Surface **linearSurfaceTree;
    int surfacesQ;
    Surface *surfaceSelection[SURFACE_SELECTION_BUF_LEN];

    Anchor *anchors;
    Anchor *anchorSelection[ANCHOR_SELECTION_BUF_LEN];
    int anchorsQ;

    Annotation *annotations;
    Annotation *selectedAnnotation;
    int annotationsQ;
} SurfaceTree;

// Surfaces
const char *GetSurfaceTypeStr(SurfaceType t);
void UpdateSurfaceTree(SurfaceTree *tree);
Surface *AddSurface(Surface *parent, char *id, char *name, SurfaceType type);
bool ClearSurfaceSkin(Skin *s);
int DeleteSurface(Surface *s);
int ReparentSurface(Surface *s, Surface *parent);
void ClearSurfaceTree(SurfaceTree *t);

void DrawSurfaceTree(
    OG_Viewport *v, 
    SurfaceTree *tree, 
    int lod, 
    Texture *bg, 
    Vector2 offset, 
    float scale, 
    Color lineColor, 
    Color cutoutColor, 
    float hatchLineSpace, 
    bool flipX, 
    bool flipY, 
    bool drawBounds, 
    bool noSelectionLines, 
    bool drawSkins, 
    bool drawcutouts,
    bool omitBaseSkin
);

Surface *GetSurfaceAt(Vector2 point, SurfaceTree *tree);
void SetSurfaceGlobalPos(Surface *s, Vector2 pos);
void SetSurfaceGlobalRotation(Surface *s, float rot);

void SelectSurface(Surface *s, bool aditive, bool canDeselect);
bool IsSurfaceSelected(Surface *s);
bool IsSurfaceBase(Surface *s);
int CanAddExtensible(SurfaceTree *tree, Vector2 *p);
int GetSurfaceIndex(Surface *s);
Surface *GetSurfaceByName(SurfaceTree *st, char *name);
void AlignChildSurfaces(Surface *s, int dir, float offset, bool recursive);
bool IsSurfaceAnchored(Surface *s);
bool ShiftSurfaceUp(Surface *s);
bool ShiftSurfaceDown(Surface *s);
void FlatSurfaceFromFather(Surface *s, bool recursive);
void FlatSurfaceChildren(Surface *s);
bool IsSurfaceNameAvailable(SurfaceTree *tree, char *name);
char *ResolveSurfaceName(char *ogName, SurfaceTree *st);
int SetSurfaceName(Surface *s, char *name);
bool IsSurfaceHidden(Surface *s);
void HideSurface(Surface *s);
void ShowSurface(Surface *s);
void ShowHideSurface(Surface *s);
void CreateSurfaceBoundarieAnchors(Surface *s);

// Anchors
const char *GetAnchorTypeStr(AnchorType t);
Anchor *GetAnchorByIndex(SurfaceTree *tree, int index);
Anchor *GetAnchorByName(SurfaceTree *tree, char *name);
int SetAnchorName(Anchor *a, char *name);
Anchor *AddAnchor(SurfaceTree *tree, char *name, AnchorType type, float val);
int SetAnchorAsOffset(Anchor *a, Anchor *b);
int SetAnchorAsCenter(Anchor *a, Anchor *b, Anchor *c);
int SetAnchorAsMirror(Anchor *a, Anchor *b, Anchor *c);
int SetAnchorAsProjection(Anchor *a, Anchor *b, bool inverted);
int FixAnchorToSurface(Anchor *a, Surface *s);
Anchor *AddAnchorOffset(Anchor *ref, char *name, float val);
Anchor *AddAnchorCenter(Anchor *refA, Anchor *refB, char *name);
Anchor *AddAnchorMirror(Anchor *refA, Anchor *refB, char *name);
int DeleteAnchorForced(Anchor *a);
int DeleteAnchor(Anchor *a, SurfaceTree **treeList, int treeListQ);
float GetAnchorAbsValue(Anchor *a);
void SetAnchorAbsValue(Anchor *a, float val);
bool IsAnchorReferencedByAnchor(Anchor *a, SurfaceTree **treeList, int treeListQ);
bool IsAnchorReferencedBySurface(Anchor *a);
bool IsAnchorLocked(Anchor *a);
bool IsAnchorSelected(Anchor *a);
bool IsAnchorBase(Anchor *a);
void SelectAnchor(Anchor *a, bool aditive, bool canDeselect);
void AnchorToggleLock(Anchor *a);
void AnchorToggleHidden(Anchor *a);
void AnchorFreeConstraint(Anchor *a);
bool CanAnchorValBeChanged(Anchor *a);
const char *GetAnchorConstraintStr(Anchor *a);
const char *GetAnchorAlignStr(Surface *s);
Anchor *GetAnchorAtLeft(Anchor *a, Anchor *b);
Anchor *GetAnchorAtTop(Anchor *a, Anchor *b);
float GetDistanceBetweenAnchors(Anchor *a, Anchor *b);
bool IsAnchorNameAvailable(SurfaceTree *t, char *n);
char *ResolveAnchorName(char *ogName, SurfaceTree *st);
void Sort3Anchors(Anchor **anchors);
bool IsAnchorPartOfCutout(Anchor *a);
bool IsAnchorPartOfBoundary(Anchor *a);
LinePoints GetAnchorScreenPoints(OG_Viewport *v, Anchor *a);

// Annotations
Annotation *AddAnnotation(SurfaceTree *tree, Anchor *A, Anchor *B);
void DeleteAnnotation(Annotation *a);
void DrawAnnotation(OG_Viewport *v, Annotation *a, Color c, float arrowSize, float fontSize, float scale, Vector2 drawOffset);

#endif