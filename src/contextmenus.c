// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include <raymath.h>
#include "cJSON.h"
#include "loader.h"
#include "origami.h"
#include "origami_tools.h"
#include "documentation.h"
#include "saver.h"
#include "schematics.h"
#include "stickers.h"
#include "surfaces.h"
#include "main.h"
#include "titleblock.h"
#include "utils.h"
#include "contextmenus.h"
#include "workspace.h"

static int alignCase = 0;
static TitleBlockRect *targetTitleBlockRect;

static void RenameAnchor(char *name){
    if (name == NULL || name[0] == '\0') {
        OG_PushLog("Anchor name is NULL or empty");
        return;
    }

    if (IsBlank(name)){
        OG_PushLog("Anchor name is blank");
        return;
    }
    
    if (SetAnchorName(activeTree->anchorSelection[0], name) == -1)
        OG_PushLog("Anchor with name '%s' already exists", name);
}

static void RenameSurface(char *name){
    if (name == NULL || name[0] == '\0') {
        OG_PushLog("Surface name is NULL or empty");
        return;
    }

    if (IsBlank(name)){
        OG_PushLog("Surface name is blank");
        return;
    }
    
    if (SetSurfaceName(activeTree->surfaceSelection[0], name) == -1)
        OG_PushLog("Surface with name '%s' already exists", name);
}

static void AlignSurfaces(char *offsetString){
    float val;
    if (!sscanf(offsetString, "%f", &val)){
        OG_PushLog("Offset should be a numeric value");
        return;
    }
    
    Surface **s = activeTree->surfaceSelection;
    switch (alignCase){
        case 0: AlignChildSurfaces(s[0], 0, val, false); break;
        case 1: AlignChildSurfaces(s[0], 0, val, true); break;
        case 2: AlignChildSurfaces(s[0], 1, val, false); break;
        case 3: AlignChildSurfaces(s[0], 1, val, true); break;
    }
}

static void DuplicateSurface(char *copiesString){
    int val;
    if (!sscanf(copiesString, "%d", &val)) {
        OG_PushLog("Copies should be a numeric value");
        return;
    }

    Surface **s = activeTree->surfaceSelection;
    Surface *srcSurface = s[0];
    char *srcPath = srcSurface->sourcePath;
    SelectSurface(srcSurface, false, false);
    for (int i=0; i<val; i++){
        LoadSurface(srcPath, NULL);
        Surface *newSurface = activeTree->lastInsertedSurface;
        memset(newSurface->sourcePath, '\0', sizeof(char) * OG_PATH_MAX);
        sprintf(newSurface->sourcePath, "%s", srcPath);

        char *ogName = srcSurface->name;
        char *newName = ResolveSurfaceName(ogName, activeTree);
        free(newSurface->name);
        newSurface->name = newName;
        newSurface->pos = (Vector2){10,10};
        newSurface->opts = srcSurface->opts;
        newSurface->size = srcSurface->size;
        SelectSurface(newSurface, false, false);
    }
}

static void NewMirrorAnchor(char *name){
    if (name == NULL || name[0] == '\0') {
        OG_PushLog("Anchor name is NULL or empty");
        return;
    }

    if (IsBlank(name)){
        OG_PushLog("Anchor name is blank");
        return;
    }

    Anchor *newAnchor = AddAnchorMirror(activeTree->anchorSelection[0], activeTree->anchorSelection[1], name);
    if (!newAnchor){
        OG_PushLog("Anchor with name '%s' already exists", name);
        return;
    }
}

static void NewCenterAnchor(char *name){
    if (name == NULL || name[0] == '\0') {
        OG_PushLog("Anchor name is NULL or empty");
        return;
    }

    if (IsBlank(name)){
        OG_PushLog("Anchor name is blank");
        return;
    }

    Anchor *newAnchor = AddAnchorCenter(activeTree->anchorSelection[0], activeTree->anchorSelection[1], name);
    if (!newAnchor){
        OG_PushLog("Anchor with name '%s' already exists", name);
        return;
    }
}

static void NewOffsetAnchor(char *name, Anchor *baseAnchor){
    if (name == NULL || name[0] == '\0') {
        OG_PushLog("Anchor name is NULL or empty");
        return;
    }

    if (IsBlank(name)){
        OG_PushLog("Anchor name is blank");
        return;
    }
    
    float val = 10.0;    
    Anchor *newAnchor = AddAnchorOffset(baseAnchor, name, val);
    if (!newAnchor){
        OG_PushLog("Anchor with name '%s' already exists", name);
        return;
    }

    if (GetAnchorAbsValue(baseAnchor) >= 0) newAnchor->val *= -1;
    float absVal = GetAnchorAbsValue(newAnchor);
    bool doneFlag = false;
    while (!doneFlag){
        doneFlag = true;
        Anchor *a = baseAnchor->tree->anchors;
        while (a){
            if (a != newAnchor && a->type == newAnchor->type && FloatEquals(absVal, GetAnchorAbsValue(a))){
                absVal += val;
                SetAnchorAbsValue(newAnchor, absVal);
                doneFlag = false;
            }
            a = a->next;
        }
    }

    SelectAnchor(newAnchor, false, false);
}

//___________________________________________

static void AnchorAddOffsetContextMenu(char *opt, void *param){
    Anchor **anchorSel = activeTree->anchorSelection;

    if (strcmp(opt, "Single") == 0){
        char *newName = ResolveAnchorName(anchorSel[0]->name, activeTree);
        NewOffsetAnchor(newName, anchorSel[0]);
        Anchor *newAnchor = GetAnchorByName(activeTree, newName);
        free(newName);

        // Deselect previus anchors and select new ones
        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++) anchorSel[i] = NULL;
        SelectAnchor(newAnchor, true, false);
        
        return;
    }

    if (strcmp(opt, "Multiple") == 0){
        int anchorsQ = 0;
        Anchor *baseSelection[ANCHOR_SELECTION_BUF_LEN];
        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
            baseSelection[i] = anchorSel[i];
            if (anchorSel[i]) anchorsQ++;
        }
        
        Anchor *newAnchors[anchorsQ];
        int aQ = 0;

        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
            if (!baseSelection[i]) continue;
            char *newName = ResolveAnchorName(baseSelection[i]->name, activeTree);
            NewOffsetAnchor(newName, baseSelection[i]);
            newAnchors[aQ++] = GetAnchorByName(activeTree, newName);
            free(newName);
        }

        // Deselect previus anchors and select new ones
        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++) anchorSel[i] = NULL;
        for (int i=0; i<aQ; i++) SelectAnchor(newAnchors[i], true, false);

        return;
    }
}

static void AnchorContextMenu(char *opt, void *param){
    Anchor **anchorSel = activeTree->anchorSelection;
    
    if (strcmp(opt, "Hide/Show Anchor") == 0){
        AnchorToggleHidden(anchorSel[0]);
        activeTree->anchorSelection[0] = NULL;
        activeTree->anchorSelection[1] = NULL;
        activeTree->anchorSelection[2] = NULL;
        activeTree->anchorSelection[3] = NULL;
        return;
    }

    if (strcmp(opt, "Rename Anchor") == 0){
        OG_OpenInputDialog(&RenameAnchor, NULL);
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
        inputDialog->pos = contextMenu->pos;
        return;
    }
    
    if (strcmp(opt, "Delete Anchor") == 0){
        int rs = DeleteAnchor(anchorSel[0], treeList, treeListQ);
        switch (rs){
            case -2: OG_PushLog("Can't delete '%s': it is referenced by another anchor", anchorSel[0]->name); break;
            case -3: OG_PushLog("Can't delete '%s': it is fixed by the engine", anchorSel[0]->name); break;
            case -4: OG_PushLog("Can't delete '%s': surfaces depend on it", anchorSel[0]->name); break;
            case 0:
            for (int i = 0; i < ANCHOR_SELECTION_BUF_LEN; i++) 
                anchorSel[i] = NULL;
            break;
        }
        
        return;
    }

    if (strcmp(opt, "Free Constraint") == 0){
        AnchorFreeConstraint(anchorSel[0]);
        return;
    }

    if (strcmp(opt, "Add Offset") == 0){
        int anchorsQ = 0;
        Anchor *baseSelection[ANCHOR_SELECTION_BUF_LEN];
        for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
            baseSelection[i] = anchorSel[i];
            if (anchorSel[i]) anchorsQ++;
        }

        if (anchorsQ == 1){
            Anchor *newAnchors[anchorsQ];
            int aQ = 0;

            for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++){
                if (!baseSelection[i]) continue;
                char *newName = ResolveAnchorName(baseSelection[i]->name, activeTree);
                NewOffsetAnchor(newName, baseSelection[i]);
                newAnchors[aQ++] = GetAnchorByName(activeTree, newName);
                free(newName);
            }

            // Deselect previus anchors and select new ones
            for (int i=0; i<ANCHOR_SELECTION_BUF_LEN; i++) anchorSel[i] = NULL;
            for (int i=0; i<aQ; i++) SelectAnchor(newAnchors[i], true, false);
        }

        else {
            char *opts[] = {
                "Single",
                "Multiple"
            };

            OG_OpenContextMenuV3(
                &AnchorAddOffsetContextMenu, 
                NULL,
                NULL, 
                sizeof(opts)/sizeof(opts[0]), 
                opts
            );
        }


        return;
    }

    if (strcmp(opt, "Add Mirror") == 0){
        if (anchorSel[0]->type != anchorSel[1]->type){
            OG_PushLog("Both anchors must have the same orientation");
            return;
        }

        
        char *newName = ResolveAnchorName(anchorSel[0]->name, activeTree);
        NewMirrorAnchor(newName);
        free(newName);

        return;
    }

    if (strcmp(opt, "Add Center") == 0){
        if (anchorSel[0]->type != anchorSel[1]->type){
            OG_PushLog("Both anchors must have the same orientation");
            return;
        }

        char *newName = ResolveAnchorName(anchorSel[0]->name, activeTree);
        NewCenterAnchor(newName);
        free(newName);

        return;
    }

    if (strcmp(opt, "Add Annotation") == 0){
        if (anchorSel[0]->type != anchorSel[1]->type){
            OG_PushLog("Both anchors must have the same orientation");
            return;
        }

        Annotation *a = AddAnnotation(activeTree, anchorSel[0], anchorSel[1]);
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *workspace = OG_GetViewportByName("Workspace_Main");
        
        Vector2 pos = (Vector2){
            (contextMenu->pos.x - workspace->pos.x - workspace->camera.offset.x)/workspace->camera.zoom + workspace->camera.target.x,
            (contextMenu->pos.y - (workspace->pos.y+(workspace->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)) - workspace->camera.offset.y)/workspace->camera.zoom + workspace->camera.target.y
        };
        
        if (a->A->type == ANCHOR_VERTICAL) a->offset = pos.y;
        if (a->B->type == ANCHOR_HORIZONTAL) a->offset = pos.x;

        if ((IsAnchorPartOfCutout(a->A) || IsAnchorPartOfCutout(a->B)) && !inspectorSettings.drawCutOuts)
            OG_PushLog("Annotation not visible: uses a cutout anchor, but cutout rendering is disabled");
        
        if ((IsAnchorPartOfBoundary(a->A) || IsAnchorPartOfBoundary(a->B)) && !inspectorSettings.drawSkins)
            OG_PushLog("Annotation not visible: uses a boundary anchor, but skin rendering is disabled");

        return;
    }

    if (strcmp(opt, "Set as Offset") == 0){
        int rs = SetAnchorAsOffset(anchorSel[0], anchorSel[1]);
        switch (rs){
            case -2: OG_PushLog("Cannot set as offset: anchors have different orientations"); break;
            case -3: OG_PushLog("Cannot set as offset: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot set as offset: circular dependency"); break;
        }
        
        return;
    }

    if (strcmp(opt, "Set as Center") == 0){
        int rs = SetAnchorAsCenter(anchorSel[0], anchorSel[1], anchorSel[2]);
        switch (rs){
            case -2: OG_PushLog("Cannot set as center: anchors have different orientations"); break;
            case -3: OG_PushLog("Cannot set as center: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot set as center: circular dependency"); break;
        }
        
        return;
    }

    if (strcmp(opt, "Set as Mirror") == 0){
        int rs = SetAnchorAsMirror(anchorSel[0], anchorSel[1], anchorSel[2]);
        switch (rs){
            case -2: OG_PushLog("Cannot set as mirror: anchors have different orientations"); break;
            case -3: OG_PushLog("Cannot set as mirror: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot set as mirror: circular dependency"); break;
        }
        
        return;
    }

    if (strcmp(opt, "Set as Projection") == 0){
        Anchor *projection[2] = { anchorSel[0], ghostTree->anchorSelection[0] };
        
        int rs = SetAnchorAsProjection(projection[0], projection[1], false);
        switch (rs){
            case -2: OG_PushLog("Cannot set as projection: anchors have different orientations"); break;
            case -3: OG_PushLog("Cannot set as projection: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot set as projection: circular dependency"); break;
        }
        
        return;
    }

    if (strcmp(opt, "Set as Inverted Projection") == 0){
        Anchor *projection[2] = { anchorSel[0], ghostTree->anchorSelection[0] };
        
        int rs = SetAnchorAsProjection(projection[0], projection[1], true);
        switch (rs){
            case -2: OG_PushLog("Cannot set as projection: anchors have different orientations"); break;
            case -3: OG_PushLog("Cannot set as projection: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot set as projection: circular dependency"); break;
        }
        
        return;
    }

    if (strcmp(opt, "Fix to Surface") == 0){
        int rs = FixAnchorToSurface(anchorSel[0], activeTree->surfaceSelection[0]);
        switch (rs){
            case -2: OG_PushLog("Cannot fix anchor to surface: anchor is fixed by engine"); break;
            case -4: OG_PushLog("Cannot fix anchor to surface: circular dependency"); break;
        }

        return;
    }
}

void OpenAchorContextMenu(){
    Anchor **a = activeTree->anchorSelection;
    if (!a[0]) return;
    
    bool canAddProjection = false;
    Anchor *projection[2] = {
        a[0], NULL
    };

    if (ghostTree && ghostTree != activeTree){
        if (ghostTree->anchorSelection[0]){
            projection[1] = ghostTree->anchorSelection[0];
            canAddProjection = true;
        }
    }

    char *opts[] = {
        "Hide/Show Anchor",
        a[0]->opts & ANCHOR_OPT_FIXED_BY_ENGINE ? NULL : "Rename Anchor",
        "Delete Anchor",
        a[0]->constraint != ANCHOR_CONSTRAINT_NONE ? "Free Constraint" : NULL,
        "Add Offset",
        a[1] ? "Add Mirror" : NULL,
        a[1] ? "Add Center" : NULL,
        a[1] ? "Add Annotation" : NULL,
        a[1] ? "Set as Offset" : NULL,
        (a[1] && a[2]) ? "Set as Center" : NULL,
        (a[1] && a[2]) ? "Set as Mirror" : NULL,
        (canAddProjection && projection[1]) ? "Set as Projection" : NULL,
        (canAddProjection && projection[1]) ? "Set as Inverted Projection" : NULL,
        activeTree->surfaceSelection[0] ? "Fix to Surface" : NULL
    };

    OG_OpenContextMenuV3(&AnchorContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
}

static void AnnotationContextMenu(char *opt, void *param){
    if (strcmp(opt, "Hide/Show Annotation") == 0){
        activeTree->selectedAnnotation->opts ^= ANNOTATION_OPT_HIDDEN;
        activeTree->selectedAnnotation = NULL;
        return;
    }
    
    if (strcmp(opt, "Delete Annotation") == 0){
        DeleteAnnotation(activeTree->selectedAnnotation);
        return;
    }

    if (strcmp(opt, "Flip Side") == 0){
        activeTree->selectedAnnotation->opts ^= ANNOTATION_OPT_FLIP_LABEL_SIDE;
        return;
    }

    if (strcmp(opt, "Disable Extension Lines") == 0){
        activeTree->selectedAnnotation->opts &= ~ANNOTATION_OPT_EXTENSION_ENABLE;
        return;
    }
    
}

void OpenAnnotationContextMenu(){
    Annotation *a = activeTree->selectedAnnotation;
    if (!a) return;

    char *opts[] = {
        "Hide/Show Annotation",
        "Delete Annotation",
        "Flip Side",
        (a->opts & ANNOTATION_OPT_EXTENSION_ENABLE) ? "Disable Extension Lines":NULL
    };

    OG_OpenContextMenuV3(&AnnotationContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
}

static void SurfaceAlignContextMenu(int opt, void *param){
    Surface **s = activeTree->surfaceSelection;

    alignCase = opt;
    OG_OpenInputDialog(AlignSurfaces, "Offset");
    OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
    OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
    inputDialog->pos = contextMenu->pos;
    
}

static void FreeSurfaceContextMenu(int opt, void *param){
    switch (opt){
        case 0:
        activeTree->surfaceSelection[0]->anchors[0] = NULL;
        break;

        case 1:
        activeTree->surfaceSelection[0]->anchors[1] = NULL;
        break;
    }
}

static void FlatSurfaceContextMenu(char *opt, void *param){

    if (strcmp(opt, "Flat From Father") == 0){
        FlatSurfaceFromFather(activeTree->surfaceSelection[0], false);
        return;
    }

    if (strcmp(opt, "Flat From Father (recursive)") == 0){
        FlatSurfaceFromFather(activeTree->surfaceSelection[0], true);
        return;
    }

    if (strcmp(opt, "Flat Children") == 0){
        FlatSurfaceChildren(activeTree->surfaceSelection[0]);
        return;
    }
}

static void ReparentSurfaceContextMenu(char *opt, void *param){
    if (strcmp(opt, "Keep Current Position") == 0){
        return;
    }

    if (strcmp(opt, "Center in New Parent") == 0){
        activeTree->surfaceSelection[0]->pos = Vector2Zero();
        Surface *s = activeTree->surfaceSelection[0];
        Surface *p = s->parent;
        if (s->type == SURFACE_COMPONENT && (p->type == SURFACE_EXTENSIBLE || p->opts & SURFACE_OPT_FIXED_Y) )
            s->opts |= SURFACE_OPT_FIXED_Y;
        return;
    }
}

static void SurfaceContextMenu(char *opt, void *param){
    Surface **s = activeTree->surfaceSelection;
    
    if (strcmp(opt, "Show/Hide Surface") == 0){
        ShowHideSurface(s[0]);
        s[0] = NULL;
        s[1] = NULL;
        return;
    }


    if (strcmp(opt, "Delete Surface") == 0){
        int rs = DeleteSurface(s[0]);
        switch (rs){
            case -2: OG_PushLog("Can't delete '%s': can't delete base surface", s[0]->id); break;
            case -4: OG_PushLog("Can't delete '%s': an anchor depends on it!", s[0]->id); break;
            case 0:
            for (int i = 0; i < SURFACE_SELECTION_BUF_LEN; i++) 
                s[i] = NULL;
            break;
        }
        
        return;
    }


    if (strcmp(opt, "Rename Surface") == 0){
        OG_OpenInputDialog(&RenameSurface, NULL);
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
        inputDialog->pos = contextMenu->pos;
        return;
    }


    if (strcmp(opt, "Add Child Surface") == 0){
        OG_OpenFileDialog(
            &LoadSurface, 
            param, 
            NULL,
            OG_FD_MODE_SELECT_FILE, 
            ".mwc",
            "Select a component [.mwc]"
        );

        return;
    }

    if (strcmp(opt, "Add Generic") == 0){
        const float baseSize = 40.0f;

        Surface *s = AddSurface(activeTree->surfaceSelection[0], "generic", ResolveSurfaceName("generic", activeTree), SURFACE_COMPONENT);
        s->size = (Vector2){baseSize, baseSize};
        s->ogSize = (Vector2){baseSize, baseSize};
        s->skin.tint = (Color){255, 255, 255, 255};
        s->opts |= SURFACE_OPT_GENERIC;
    }


    if (strcmp(opt, "Align Children") == 0){
        OG_OpenContextMenu(
            &SurfaceAlignContextMenu, 
            NULL,
            NULL, 
            4, 
            "Align Ahead",
            "Align Ahead (recursive)",
            "Align Behind",
            "Align Behind (recursive)"
        );

        return;
    }


    if (strcmp(opt, "Flat Surface") == 0){
        char *opts[] = {
            !IsSurfaceBase(s[0]) ? "Flat From Father" : NULL,
            !IsSurfaceBase(s[0]) ? "Flat From Father (recursive)" : NULL,
            "Flat Children"
        };

        OG_OpenContextMenuV3(&FlatSurfaceContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
        return;
    }


    if (strcmp(opt, "Shift Up") == 0){
        ShiftSurfaceUp(s[0]);
    }


    if (strcmp(opt, "Shift Down") == 0){
        ShiftSurfaceDown(s[0]);
    }


    if (strcmp(opt, "Reparent Surface") == 0){
        Surface **ss = activeTree->surfaceSelection;
        int rs = ReparentSurface(ss[0], ss[1]);

        switch (rs){
            case -2: OG_PushLog("2 surfaces must be selected"); return;;
            case -3: OG_PushLog("Base surface can't be reparented"); return;;
            case -4: OG_PushLog("Surface is already parented to target surface"); return;;
            case -5: OG_PushLog("Cannot reparent surface: cyclic hierarchy"); return;;
        }

        char *opts[] = {
            "Keep Current Position",
            "Center in New Parent"
        };

        OG_OpenContextMenuV3(&ReparentSurfaceContextMenu, NULL, NULL, sizeof(opts)/sizeof(opts[0]), opts);
        return;
    }


    if (strcmp(opt, "Duplicate Surface") == 0){
        OG_OpenInputDialog(&DuplicateSurface, "Copies");
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
        inputDialog->pos = contextMenu->pos;

        return;
    }


    if (strcmp(opt, "Reset Anchors") == 0){
       int rs = CanAddExtensible(activeTree, NULL);
        switch (rs){
            case -1: OG_PushLog("3 anchors required"); return;
            case -2:
            case -3: OG_PushLog("3 anchors required (a perpendicular is missing)"); return;
        }

        Sort3Anchors(activeTree->anchorSelection);
        s[0]->anchors[0] = activeTree->anchorSelection[0];
        s[0]->anchors[1] = activeTree->anchorSelection[1];
        s[0]->anchors[2] = activeTree->anchorSelection[2];

        if (activeTree->anchorSelection[0]->type == ANCHOR_VERTICAL && s[0]->rotation == 0) s[0]->rotation = 90;
        if (activeTree->anchorSelection[0]->type == ANCHOR_VERTICAL && s[0]->rotation == 180) s[0]->rotation = 270;
        if (activeTree->anchorSelection[0]->type == ANCHOR_HORIZONTAL && s[0]->rotation == 90) s[0]->rotation = 0;
        if (activeTree->anchorSelection[0]->type == ANCHOR_HORIZONTAL && s[0]->rotation == 270) s[0]->rotation = 180;

        return;
    }


    if (strcmp(opt, "Fix to Anchor") == 0){
        if (IsSurfaceBase(activeTree->surfaceSelection[0])){
            OG_PushLog("Base Surface can't be fixed to an anchor");
            return;
        }
        
        if (!activeTree || !activeTree->anchorSelection[0]){
            OG_PushLog("An anchor must be selected");
            return;
        }

        switch (activeTree->anchorSelection[0]->type){
            case ANCHOR_HORIZONTAL:
            activeTree->surfaceSelection[0]->anchors[1] = activeTree->anchorSelection[0];
            
            return;
            
            case ANCHOR_VERTICAL: 
            activeTree->surfaceSelection[0]->anchors[0] = activeTree->anchorSelection[0];
            return;
        }

        return;
    }


    if (strcmp(opt, "Free from Anchor") == 0){
        if (activeTree->surfaceSelection[0]->anchors[0] && !activeTree->surfaceSelection[0]->anchors[1])
            activeTree->surfaceSelection[0]->anchors[0] = NULL;
        
        else if (activeTree->surfaceSelection[0]->anchors[1] && !activeTree->surfaceSelection[0]->anchors[0])
            activeTree->surfaceSelection[0]->anchors[1] = NULL;

        else {
            OG_OpenContextMenu(
                &FreeSurfaceContextMenu,
                NULL,
                NULL,
                2,
                "Free Horizontal",
                "Free Vertical"
            );
        }

        return;
    }


    if (strcmp(opt, "Create Boundary Anchors") == 0){
        CreateSurfaceBoundarieAnchors(s[0]);
        return;
    }


    if (strcmp(opt, "Create CutOut Anchors") == 0){
        // Get bounding box
        Rectangle polyRect = {0};
        int pQ = s[0]->cutout.len;
        Vector2 surfaceScale = Vector2Divide(s[0]->size, s[0]->ogSize);
        Vector2 p[pQ];
        for (int i=0; i< pQ; i++){
            p[i] = Vector2Multiply(s[0]->cutout.points[i], surfaceScale);
        }
        
        float minX = p[0].x;
        float minY = p[0].y;
        float maxX = p[0].x;
        float maxY = p[0].y;

        for (int i = 1; i < pQ; i++){
            if (p[i].x < minX) minX = p[i].x;
            if (p[i].y < minY) minY = p[i].y;
            if (p[i].x > maxX) maxX = p[i].x;
            if (p[i].y > maxY) maxY = p[i].y;
        }

        polyRect.x = minX;
        polyRect.y = minY;
        polyRect.width = maxX - minX;
        polyRect.height = maxY - minY;

        if (s[0]->globalRotation != 0 && s[0]->globalRotation != 180){
            float tmp = polyRect.width;
            polyRect.width = polyRect.height;
            polyRect.height = tmp;
        }

        // done, now with the anchor thing
        char name[2048];

        //TOP
        if (!s[0]->cutoutAnchors.top){
            sprintf(name, "%s_co_top", s[0]->name);
            Anchor *top = AddAnchor(
                s[0]->tree, 
                ResolveAnchorName(name, s[0]->tree),
                ANCHOR_HORIZONTAL, 
                0
            );
            FixAnchorToSurface(top, s[0]);
            s[0]->cutoutAnchors.top = top;
        }
        s[0]->cutoutAnchors.top->val = polyRect.height/2 *-1;
        s[0]->cutoutAnchors.top->opts |= ANCHOR_OPT_FIXED;

        //BOTTOM
        if (!s[0]->cutoutAnchors.bottom){
            sprintf(name, "%s_co_bottom", s[0]->name);
            Anchor *bottom = AddAnchor(
                s[0]->tree, 
                ResolveAnchorName(name, s[0]->tree),
                ANCHOR_HORIZONTAL, 
                0
            );
            FixAnchorToSurface(bottom, s[0]);
            s[0]->cutoutAnchors.bottom = bottom;
        }
        s[0]->cutoutAnchors.bottom->val = polyRect.height/2;
        s[0]->cutoutAnchors.bottom->opts |= ANCHOR_OPT_FIXED;

        //LEFT
        if (!s[0]->cutoutAnchors.left){
            sprintf(name, "%s_co_left", s[0]->name);
            Anchor *left = AddAnchor(
                s[0]->tree, 
                ResolveAnchorName(name, s[0]->tree),
                ANCHOR_VERTICAL, 
                0
            );
            FixAnchorToSurface(left, s[0]);
            s[0]->cutoutAnchors.left = left;
        }
        s[0]->cutoutAnchors.left->val = polyRect.width/2 *-1;
        s[0]->cutoutAnchors.left->opts |= ANCHOR_OPT_FIXED;

        //RIGHT
        if (!s[0]->cutoutAnchors.right){
            sprintf(name, "%s_co_right", s[0]->name);
            Anchor *right = AddAnchor(
                s[0]->tree, 
                ResolveAnchorName(name, s[0]->tree),
                ANCHOR_VERTICAL, 
                0
            );
            FixAnchorToSurface(right, s[0]);
            s[0]->cutoutAnchors.right = right;
        }
        s[0]->cutoutAnchors.right->val = polyRect.width/2;
        s[0]->cutoutAnchors.right->opts |= ANCHOR_OPT_FIXED;
    }

}

void OpenSurfaceContextMenu(void *param){
    Surface **s = activeTree->surfaceSelection;
    Anchor **a = activeTree->anchorSelection;
    if (!s[0]) return;

    char *opts[] = {
        !IsSurfaceBase(s[0]) ? "Show/Hide Surface":NULL,
        !IsSurfaceBase(s[0]) ? "Delete Surface":NULL,
        !IsSurfaceBase(s[0]) ? "Rename Surface":NULL,
        !IsSurfaceBase(s[0]) ? "Reparent Surface":NULL,
        !IsSurfaceBase(s[0]) ? "Duplicate Surface":NULL,
        "Add Child Surface",
        "Add Generic",
        !IsSurfaceBase(s[0]) ? "Align Children":NULL,
        "Flat Surface",
        s[0] != (s[0]->parent ? s[0]->parent->children:s[0]) ? "Shift Up":NULL,
        s[0]->next != NULL ? "Shift Down" : NULL,
        s[0]->type == SURFACE_EXTENSIBLE ? "Reset Anchors":NULL,
        (s[0]->type == SURFACE_COMPONENT && a[0]) ? "Fix to Anchor":NULL,
        (s[0]->type == SURFACE_COMPONENT && IsSurfaceAnchored(s[0])) ? "Free from Anchor":NULL,
        !IsSurfaceBase(s[0]) ? "Create Boundary Anchors":NULL,
        (s[0]->type == SURFACE_COMPONENT && s[0]->cutout.points) ? "Create CutOut Anchors":NULL
    };

    OG_OpenContextMenuV3(&SurfaceContextMenu, param, NULL, sizeof(opts)/sizeof(opts[0]), opts);
}

static void TreeListContextMenu(int opt, void *param){
    if (!ghostTree){
        OG_PushLog("There is no SurfaceTree selected as ghost");
        return;
    }

    switch (opt){
        case 0: ghostTreeFlipX = !ghostTreeFlipX; break;
        case 1: ghostTreeFlipY = !ghostTreeFlipY; break;
    }
}

void OpenTreeListContextMenu(){
    OG_OpenContextMenu(
        &TreeListContextMenu, NULL, NULL, 2, 
        "Flip Ghost on X",
        "Flip Ghost on Y"
    );
}

static void SurfaceTreeContextMenu(char *opt, void *param){
    if (strcmp(opt, "Export Cutouts (dxf)") == 0){
        OG_OpenFileDialog(
            &SaveCutouts, 
            NULL, 
            NULL, 
            OG_FD_MODE_SELECT_FILE, 
            NULL,
            "Export current surface tree cutout [.dxf]"
        );
        return;
    }
}

void OpenSurfaceTreeContextMenu(){
    if (!activeTree) return;
    char *opts[] = {
        "Export Cutouts (dxf)"
    };

    OG_OpenContextMenuV3( 
        &SurfaceTreeContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]), 
        opts 
    );
}

static void SurfaceEntityContextMenu(char *opt, void *param){
    Surface *s;
    for (int i=0; i<treeListQ; i++){
        if (strcmp(treeList[i]->baseSurface.name, opt) == 0){
            s = &treeList[i]->baseSurface;
            break;
        }
    }

    AddSurfaceEntity(mainDocument.selectedPage, s);
}

static void TitleBlockEntityContextMenu(int opt, void *param){
    int opti = 0;
    for (int i=0; i<titleBlocksQ; i++){
        if (!titleBlocks[i]->sourcePath) continue;
        if (opt == opti) break;
        opti++;
    }

    AddTitleBlockEntity(mainDocument.selectedPage, titleBlocks[opti]);
}

static void AddStickerCloseCallback(bool rs){
    if (!rs) return;
    AddStickerEntity(mainDocument.selectedPage, lastAddedSticker);
}

static void PageListContextMenu(char *opt, void *param){
    if (strcmp(opt, "New Page") == 0){
        NewPage(&mainDocument, NULL);
        return;
    }

    if (strcmp(opt, "Delete Page") == 0){
        DeletePage(mainDocument.selectedPage);
        return;
    }

    if (strcmp(opt, "Save Document as") == 0){
        if (mainDocument.pagesQ <= 0){
            OG_PushLog("Cannot save document, document is empty");
            return;
        }

        OG_OpenFileDialog(
            &SaveMainDoc, 
            NULL, 
            NULL, 
            OG_FD_MODE_SELECT_FILE, 
            NULL,
            "Save current open document [.pdf]"
        );
        
        return;
    }

    if (strcmp(opt, "Duplicate Page") == 0){
        Page *currentPage = mainDocument.selectedPage;
        Page *newPage = NewPage(&mainDocument, NULL);
        SelectPage(currentPage);
        
        newPage->drawFrame = currentPage->drawFrame;
        newPage->frame.top = currentPage->frame.top;
        newPage->frame.bottom = currentPage->frame.bottom;
        newPage->frame.left = currentPage->frame.left;
        newPage->frame.right = currentPage->frame.right;

        for (PageEntity *pe=currentPage->entities; pe != NULL; pe = pe->next){
            switch (pe->type){
                case PAGE_ENTITY_SURFACE:{
                    PageEntitySurface *e = AddSurfaceEntity(newPage, pe->surface->surface);
                    e->entity->pos = pe->pos;
                    e->annotationsArrowSize = pe->surface->annotationsArrowSize;
                    e->annotationsFontSize = pe->surface->annotationsFontSize;
                    e->flags = pe->surface->flags;
                    e->lod = pe->surface->lod;
                    e->scale = pe->surface->scale;
                } break;

                case PAGE_ENTITY_TEXT:{
                    PageEntityText *e = AddTextEntity(newPage, pe->text->text);
                    e->entity->pos = pe->pos;
                    e->alignment = pe->text->alignment;
                    e->color = pe->text->color;
                    e->fontSize = pe->text->fontSize;
                } break;

                case PAGE_ENTITY_TITLEBLOCK:{
                    PageEntityTitleBlock *e = AddTitleBlockEntity(newPage, pe->block->block);
                    e->entity->pos = pe->pos;

                    for (PageEntityTitleBlockProp *p = e->properties; p != NULL; p = p->next){
                        for (PageEntityTitleBlockProp *pp = pe->block->properties; pp != NULL; pp = pp->next){
                            if (pp->r == p->r){
                                if (p->content)
                                    free(p->content);
                                p->content = strdup(pp->content);
                                break;
                            }
                        }
                        
                    }

                } break;

                case PAGE_ENTITY_STICKER:{
                    PageEntitySticker *e = AddStickerEntity(newPage, pe->sticker->sticker);
                    e->entity->pos = pe->pos;
                    e->scale = pe->sticker->scale;
                } break;
                
            }
        }

        SelectPage(newPage);

        return;
    }

    if (strcmp(opt, "Add Text") == 0){
        AddTextEntity(mainDocument.selectedPage, "Text");
        return;
    }

    if (strcmp(opt, "Add Surface") == 0){
        char *opts[treeListQ];
        for (int i=0; i<treeListQ; i++)
            opts[i] = treeList[i]->baseSurface.name;

        OG_OpenContextMenuV3(
            &SurfaceEntityContextMenu, 
            NULL, 
            NULL, 
            treeListQ, 
            opts
        );

        return;
    }

    if (strcmp(opt, "Add TitleBlock") == 0){
        
        int len = 0;
        for (int i=0; i<titleBlocksQ; i++){
            if (!titleBlocks[i]->sourcePath) continue;
            len++;
        }
        
        char *opts[len];
        int opti = 0;
        for (int i=0; i<titleBlocksQ; i++){
            if (!titleBlocks[i]->sourcePath) continue;
            opts[opti++] = (char*)GetFileName(titleBlocks[i]->sourcePath);
        }
            
        OG_OpenContextMenuV2(
            &TitleBlockEntityContextMenu, 
            NULL, 
            NULL, 
            len, 
            opts
        );

        return;
    }

    if (strcmp(opt, "Add Sticker") == 0){
        OG_OpenFileDialog(
            &LoadSticker, 
            NULL, 
            &AddStickerCloseCallback, 
            OG_FD_MODE_SELECT_FILE, 
            NULL,
            "Select Image File"
        );
    }

}

void OpenPageListContextMenu(){
    bool canShowBlocks = false;
    if (mainDocument.selectedPage){
        int blocksWName = 0;
        for (int i=0; i<titleBlocksQ; i++){
            if (titleBlocks[i]->sourcePath) blocksWName++;
        }
        if (blocksWName > 0)
            canShowBlocks = true;
    }
    
    char *opts[] = {
        activeTree ? "New Page" : NULL,
        mainDocument.selectedPage ? "Delete Page" : NULL,
        (!IS_WASM && mainDocument.pagesQ > 0) ? "Save Document as":NULL,
        (mainDocument.selectedPage) ? "Duplicate Page" : NULL,
        (mainDocument.selectedPage) ? "Add Text" : NULL,
        (mainDocument.selectedPage && treeListQ > 0) ? "Add Surface" : NULL,
        canShowBlocks ? "Add TitleBlock" : NULL,
        mainDocument.selectedPage ? "Add Sticker" : NULL
        
    };

    OG_OpenContextMenuV3( 
        &PageListContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]), 
        opts 
    );
}

static void EntityContextMenu(char *opt, void *param){
    PageEntity *e = mainDocument.selectedPage->selectedEntity;
    if (strcmp(opt, "Delete Entity") == 0){
        DeleteEntity(mainDocument.selectedPage->selectedEntity);
        return;
    }

    if (strcmp(opt, "Shift Up") == 0){
        ShiftEntityUp(e);
        return;
    }
    
    if (strcmp(opt, "Shift Down") == 0){
        ShiftEntityDown(e);
        return;
    }

    if (strcmp(opt, "Export to SVG") == 0){
        if (e->type != PAGE_ENTITY_SURFACE) return;
        OG_OpenFileDialog(
            &SaveSurfaceSVG, 
            NULL, 
            NULL, 
            OG_FD_MODE_SELECT_FILE, 
            NULL,
            "Export surface entity [.svg]"
        );
        return;
    }

}

void OpenEntityContextMenu(){
    PageEntity *e = mainDocument.selectedPage->selectedEntity;
    char *opts[] = {
        "Delete Entity",
        e->page->entities != e ? "Shift Up":NULL,
        e->next ? "Shift Down" : NULL,
        (!IS_WASM && e->type == PAGE_ENTITY_SURFACE) ? "Export to SVG" : NULL
    };

    OG_OpenContextMenuV3( 
        &EntityContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]), 
        opts 
    );
}

static void SplitTitleBlockRect(char *divisionsStr, int c){
    int divisions;
    if (!sscanf(divisionsStr, "%d", &divisions)){
        OG_PushLog("Divisions should be an number value");
        return;
    }

    AddTitleBlockDivisions(
        targetTitleBlockRect, 
        c, 
        divisions, 
        NULL
    );
}

static void SplitTitleBlockRectV(char *divisionsStr){ SplitTitleBlockRect(divisionsStr, 0); }
static void SplitTitleBlockRectH(char *divisionsStr){ SplitTitleBlockRect(divisionsStr, 1); }

static void TitleBlockRectContextMenu(char *opt, void *param){
    if (strcmp(opt, "Split Horizontally") == 0){
        OG_OpenInputDialog(SplitTitleBlockRectH, "Divisions");
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
        inputDialog->pos = contextMenu->pos;
        return;
    }

    if (strcmp(opt, "Split Vertically") == 0){
        OG_OpenInputDialog(SplitTitleBlockRectV, "Divisions");
        OG_Viewport *contextMenu = OG_GetViewportByName("ContextMenu");
        OG_Viewport *inputDialog = OG_GetViewportByName("InputDialog");
        inputDialog->pos = contextMenu->pos;
        return;
    }
}

void OpenTitleBlockRectContextMenu(TitleBlockRect *r){
    targetTitleBlockRect = r;
    char *opts[] = {
        "Split Horizontally",
        "Split Vertically"
    };
    
    OG_OpenContextMenuV3(
        &TitleBlockRectContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]),
        opts
    );
}

static void TitleBlockContextMenu(char *opt, void *param){
    if (strcmp(opt, "Delete") == 0){
        if (!activeTitleBlock) return;
        
        int ti;
        for (ti=0; ti<titleBlocksQ; ti++){
            if (titleBlocks[ti] == activeTitleBlock)
                break;
        }

        FreeTitleBlock(activeTitleBlock);
        for (int i = ti; i + 1 < titleBlocksQ; i++)
            titleBlocks[i] = titleBlocks[i + 1];
        titleBlocks[--titleBlocksQ] = NULL;
        activeTitleBlock = NULL;

        return;
    }

    if (strcmp(opt, "Save Title Block as") == 0){
        if (activeTitleBlock)
            OG_OpenFileDialog( &SaveTitleBlock, NULL, NULL, OG_FD_MODE_SELECT_FILE, NULL, "Save current title block" );
        return; 
    }
}

void OpenTitleBlockContextMenu(){
    char *opts[] = {
        "Delete",
        "Save Title Block as"
    };

    OG_OpenContextMenuV3(
        &TitleBlockContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]),
        opts
    );
}

static void TitleBlockEditorContextMenu(char *opt, void *param){
    if (strcmp(opt, "New Title Block") == 0){
        titleBlocks[titleBlocksQ] = NewTitleBlock(NULL);
        activeTitleBlock = titleBlocks[titleBlocksQ++];
        return;
    }

    if (strcmp(opt, "Open Title Block") == 0){
        OG_OpenFileDialog(
            &LoadTitleBlock, 
            NULL, 
            NULL, 
            OG_FD_MODE_SELECT_FILE, 
            ".mwi", 
            "Select a title block file [.mwi]"
        );
        return;
    }
}

void OpenTitleBlockEditorContextMenu(){
    char *opts[] = {
        "New Title Block",
        "Open Title Block"
    };

    OG_OpenContextMenuV3(
        &TitleBlockEditorContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]),
        opts
    );
}

static void SchematicEditorContextMenu(char *opt, void *param){
    if (strcmp(opt, "Add Symbol") == 0){
        Symbol *s = AddSymbol(activeSchematic);
        return;
    }
}

void OpenSchematicEditorContextMenu(){
    char *opts[] = {
        "Add Symbol",
    };

    OG_OpenContextMenuV3(
        &SchematicEditorContextMenu, 
        NULL, 
        NULL, 
        sizeof(opts)/sizeof(opts[0]),
        opts
    );
}