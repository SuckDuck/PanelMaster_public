// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>
#include "cJSON.h"
#include "documentation.h"
#include "origami.h"
#include "main.h"
#include "surfaces.h"
#include "titleblock.h"
#include "saver.h"

/* <== Documentation ===========================================> */
static void SerializeEntityBlock(cJSON *j, PageEntityTitleBlock *b){
    cJSON_AddStringToObject(j, "block", GetFileName(b->block->sourcePath));
    cJSON *props = cJSON_AddArrayToObject(j, "properties");
    for (PageEntityTitleBlockProp *p = b->properties; p != NULL; p=p->next){
        cJSON *propJ = cJSON_CreateObject();
        cJSON_AddItemToArray(props, propJ);
        cJSON_AddStringToObject(propJ, "rect", p->r->label);
        cJSON_AddStringToObject(propJ, "content", p->content);
    }
}

static void SerializeEntityText(cJSON *j, PageEntityText *t){
    cJSON_AddStringToObject(j, "text", t->text);
    cJSON_AddNumberToObject(j, "fontSize", t->fontSize);
    cJSON *color = cJSON_AddObjectToObject(j, "color");
    cJSON_AddNumberToObject(color, "r", t->color.r);
    cJSON_AddNumberToObject(color, "g", t->color.g);
    cJSON_AddNumberToObject(color, "b", t->color.b);
    cJSON_AddNumberToObject(j, "alignment", t->alignment);
}

static void SerializeEmtitySurface(cJSON *j, PageEntitySurface *s){
    cJSON_AddStringToObject(j, "surface", s->surface->name);
    cJSON_AddNumberToObject(j, "scale", s->scale);
    cJSON_AddNumberToObject(j, "flags", s->flags);
    cJSON_AddNumberToObject(j, "annotationsFontSize", s->annotationsFontSize);
    cJSON_AddNumberToObject(j, "annotationsArrowSize", s->annotationsArrowSize);
    cJSON_AddNumberToObject(j, "lod", s->lod);
}

static void SerializeEntitySticker(cJSON *j, PageEntitySticker *s){
    cJSON_AddStringToObject(j, "srcPath", GetFileName(s->sticker->srcPath));
    cJSON_AddNumberToObject(j, "scale", s->scale);
}

static cJSON *SerializeEntity(PageEntity *pe){
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "type", pe->type);
    cJSON_AddStringToObject(j, "name", pe->name);
    cJSON *pos = cJSON_AddObjectToObject(j, "pos");
    cJSON_AddNumberToObject(pos, "x", pe->pos.x);
    cJSON_AddNumberToObject(pos, "y", pe->pos.y);
    
    switch (pe->type){
        case PAGE_ENTITY_SURFACE:    SerializeEmtitySurface(j, pe->surface); break;
        case PAGE_ENTITY_TEXT:       SerializeEntityText(j, pe->text);       break;
        case PAGE_ENTITY_TITLEBLOCK: SerializeEntityBlock(j, pe->block);     break;
        case PAGE_ENTITY_STICKER:    SerializeEntitySticker(j, pe->sticker); break;
    }

    return j;
}

static cJSON *SerializePage(Page *p){
    cJSON *j = cJSON_CreateObject();
    
    cJSON_AddStringToObject(j, "name", p->name);
    cJSON_AddBoolToObject(j, "draw_frame", p->drawFrame);
    
    // Frame
    cJSON *frame = cJSON_AddObjectToObject(j, "frame");
    cJSON_AddNumberToObject(frame, "top", p->frame.top);
    cJSON_AddNumberToObject(frame, "bottom", p->frame.bottom);
    cJSON_AddNumberToObject(frame, "left", p->frame.left);
    cJSON_AddNumberToObject(frame, "right", p->frame.right);

    cJSON *entities = cJSON_AddArrayToObject(j, "entities");
    for (PageEntity *pe = p->entities; pe != NULL; pe = pe->next)
        cJSON_AddItemToArray(entities, SerializeEntity(pe));
        
    return j;
}

static cJSON *SerializeDocument(Document *d){
    cJSON *j = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(j, "width", d->width);
    cJSON_AddNumberToObject(j, "height", d->height);
    
    cJSON *pages = cJSON_AddArrayToObject(j, "pages");
    for (Page *p = d->pages; p != NULL; p = p->next)
        cJSON_AddItemToArray(pages, SerializePage(p));

    return j;
}

/* <== Title Blocks ============================================> */
static cJSON *SerializeTitleBlockLine(TitleBlockLine *l){
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "t",l->t);
    cJSON_AddNumberToObject(j, "type",l->type);;
    return j;
}

static cJSON *SerializeTitleRect(TitleBlockRect *r){
    cJSON *j = cJSON_CreateObject();
    
    // Lines
    cJSON *linesJson = cJSON_CreateArray();
    TitleBlockLine *lines[] = {
        r->lines.top,
        r->lines.bottom,
        r->lines.left,
        r->lines.right
    };

    for (int i=0; i<4; i++){
        TitleBlockLine *l = lines[i];
        
        int o=0;
        for (TitleBlockLine *ll = r->titleBlock->lines; ll != NULL; ll=ll->next){
            if (l == ll){
                cJSON_AddItemToArray(linesJson, cJSON_CreateNumber(o));
                break;
            }
            
            o++;
        }
    }

    cJSON_AddItemToObject(j, "lines", linesJson);
    
    // Others
    cJSON_AddNumberToObject(j, "textSize", r->textSize);
    cJSON_AddNumberToObject(j, "textPos", r->textPos);
    cJSON_AddStringToObject(j, "label", r->label);
    cJSON_AddStringToObject(j, "content", r->content);
    cJSON_AddBoolToObject(j, "externalContent", r->externalContent>0);
    cJSON_AddBoolToObject(j, "noLabel", r->noLabel>0);
    return j;
}

/* <== Assemblies ==============================================> */

static cJSON *serializeAnchor(Anchor *a){
    // these are created by the engine when the template is loaded
    if (a->opts & ANCHOR_OPT_FIXED_BY_ENGINE)
        return NULL;
    
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "name", a->name);
    cJSON_AddNumberToObject(j, "type", a->type);
    cJSON_AddNumberToObject(j, "opts", a->opts);
    cJSON_AddNumberToObject(j ,"val", a->val);
    return j;
}

static cJSON *serializeAnchorConstraint(Anchor *a){
    // these are created by the engine when the template is loaded
    if (IsAnchorBase(a))
        return NULL;
    
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "anchor", a->name);
    cJSON_AddNumberToObject(j, "type", a->constraint);
    if (a->constraint == ANCHOR_CONSTRAINT_PROJECTION || a->constraint == ANCHOR_CONSTRAINT_INVERSED_PROJECTION){
        cJSON *refA = cJSON_AddObjectToObject(j, "A");
        cJSON_AddStringToObject(refA, "surface_tree", a->refA->tree->baseSurface.name);
        cJSON_AddStringToObject(refA, "name", a->refA->name);
    }

    else cJSON_AddStringToObject(j, "A", a->refA ? a->refA->name:""); 
    cJSON_AddStringToObject(j, "B", a->refB ? a->refB->name:"");
    if (a->refSurface){
        cJSON *rsj = cJSON_AddObjectToObject(j, "refSurface");
        cJSON_AddStringToObject(rsj, "surface", a->refSurface->name);
        
        // boundaries
        if (a->refSurface->boundariesAnchors.top == a)    cJSON_AddStringToObject(rsj, "type", "bTop");
        if (a->refSurface->boundariesAnchors.bottom == a) cJSON_AddStringToObject(rsj, "type", "bBottom");
        if (a->refSurface->boundariesAnchors.left == a)   cJSON_AddStringToObject(rsj, "type", "bLeft");
        if (a->refSurface->boundariesAnchors.right == a)  cJSON_AddStringToObject(rsj, "type", "bRight");

        // cutout
        if (a->refSurface->cutoutAnchors.top == a)    cJSON_AddStringToObject(rsj, "type", "coTop");
        if (a->refSurface->cutoutAnchors.bottom == a) cJSON_AddStringToObject(rsj, "type", "coBottom");
        if (a->refSurface->cutoutAnchors.left == a)   cJSON_AddStringToObject(rsj, "type", "coLeft");
        if (a->refSurface->cutoutAnchors.right == a)  cJSON_AddStringToObject(rsj, "type", "coRight");

    }
    return j;
}

static cJSON *serializeAnnotation(Annotation *a){
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "A", a->A->name);
    cJSON_AddStringToObject(j, "B", a->B->name);
    cJSON_AddNumberToObject(j, "offset", a->offset);
    cJSON_AddNumberToObject(j, "extension", a->extensionLine);
    cJSON_AddNumberToObject(j, "opts", a->opts);
    return j;
}

static cJSON *serializeSurface(Surface *s){
    // these are created by the engine when the template is loaded
    if (IsSurfaceBase(s))
        return NULL;

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "id", s->id);
    cJSON_AddStringToObject(j, "name", s->name);
    cJSON_AddNumberToObject(j, "type", s->type);

    cJSON *pos = cJSON_AddObjectToObject(j, "pos");
    cJSON_AddNumberToObject(pos, "x", s->pos.x);
    cJSON_AddNumberToObject(pos, "y", s->pos.y);

    cJSON_AddNumberToObject(j, "rotation", s->rotation);
    cJSON_AddNumberToObject(j, "opts", s->opts);

    cJSON *anchors = cJSON_AddArrayToObject(j, "anchors");
    for (int i=0; i<3; i++){
        Anchor *a = s->anchors[i];
        cJSON_AddItemToArray(
            anchors, 
            a ? cJSON_CreateString(a->name):cJSON_CreateNull()
        );
    }

    cJSON_AddNumberToObject(j, "anchor_align", s->anchorAlign);
    cJSON_AddNumberToObject(j, "left_cut", s->leftCut);
    cJSON_AddNumberToObject(j, "right_cut", s->rightCut);
    
    // -1 because 0 is actually the base surface, so i prefer base surface being -1 and
    // save the other index in real order
    cJSON_AddNumberToObject(j, "parent", GetSurfaceIndex(s->parent)-1);

    return j;
}

static cJSON *serializeTree(SurfaceTree *t){
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "name", t->baseSurface.id);
    
    // anchors / anchors constraints
    cJSON *anchors = cJSON_AddArrayToObject(j, "anchors");
    cJSON *anchorConstraints = cJSON_AddArrayToObject(j, "anchors_constraints");
    Anchor *a = t->anchors;
    while (a != NULL){
        cJSON_AddItemToArray(anchors, serializeAnchor(a));
        cJSON_AddItemToArray(anchorConstraints, serializeAnchorConstraint(a));
        a = a->next;
    }

    // anotations
    cJSON *annotations = cJSON_AddArrayToObject(j, "annotations");
    Annotation *ann = t->annotations;
    while (ann != NULL){
        cJSON_AddItemToArray(annotations, serializeAnnotation(ann));
        ann = ann->next;
    }

    // surfaces
    cJSON *surfaces = cJSON_AddArrayToObject(j, "surfaces");
    for (int i=0; i<t->surfacesQ; i++){
        Surface *s = t->linearSurfaceTree[i];
        cJSON_AddItemToArray(surfaces, serializeSurface(s));
    }


    return j;
}

/* <== Public API ==============================================> */

bool SaveTitleBlock(char *path, void *param){
    if (!activeTitleBlock) return false;
    if (!path && !activeTitleBlock->sourcePath) return false;
    char pathwExt[path ? strlen(path)+5:1];
    
    if (path){
        const char *ext = GetFileExtension(path);
        if (!ext || strcmp(ext, ".mwi"))
            sprintf(pathwExt, "%s.mwi", path);
        else sprintf(pathwExt, "%s", path);
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddItemToObject(j, "version", cJSON_CreateNumber(TITLEBLOCKS_VERSION));
    
    cJSON *size = cJSON_CreateObject();
    cJSON_AddNumberToObject(size, "w", activeTitleBlock->size.x);
    cJSON_AddNumberToObject(size, "h", activeTitleBlock->size.y);
    cJSON_AddItemToObject(j, "size", size);

    cJSON *lines = cJSON_AddArrayToObject(j, "lines");
    for (TitleBlockLine *l = activeTitleBlock->lines; l != NULL; l=l->next)
        cJSON_AddItemToArray(lines, SerializeTitleBlockLine(l));

    cJSON *rects = cJSON_AddArrayToObject(j, "rects");
    for (TitleBlockRect *r = activeTitleBlock->rects; r != NULL; r=r->next)
        cJSON_AddItemToArray(rects, SerializeTitleRect(r));

    char *jsonStr = cJSON_Print(j);
    SaveFileText(path ? pathwExt:activeTitleBlock->sourcePath, jsonStr);
    free(jsonStr);
    cJSON_Delete(j);

    // Saves the original path
    if (path){
        if (activeTitleBlock->sourcePath)
            free(activeTitleBlock->sourcePath);

        char outPath[OG_PATH_MAX] = {0};
        snprintf(outPath, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), pathwExt);
        activeTitleBlock->sourcePath = calloc(strlen(outPath)+1, sizeof(char));
        strcpy(activeTitleBlock->sourcePath, outPath);
    }

    if (path)
        OG_PushLog("Title block saved successfully: %s", pathwExt);
    return true;
}

bool SavePanelAssembly(char *path, void *param){
    if (treeListQ <= 0){
        OG_PushLog("No panel assembly currently loaded");
        return false;
    }

    for (int i=0; i<titleBlocksQ; i++){
        if (!titleBlocks[i]->sourcePath){
            OG_PushLog("Cannot save assembly, one or more title blocks have not been saved");
            return false;
        }
    }

    TitleBlock *tmpTB = activeTitleBlock;
    for (int i=0; i<titleBlocksQ; i++){
        if (titleBlocks[i]){
            activeTitleBlock = titleBlocks[i];
            SaveTitleBlock(NULL, NULL);
        }
    }
    activeTitleBlock = tmpTB;

    const char *ext = GetFileExtension(path);
    if (ext && strcmp(ext, ".mwa")){
        OG_PushLog("Expected a .mwa file or project folder name");
        return false;
    }

    char assemblyOutName[OG_PATH_MAX] = {0};
    
    if (IsFileExtension(path, ".mwa")){
        ChangeDirectory(GetDirectoryPath(path));
        sprintf(assemblyOutName, "%s", GetFileName(path));
    }

    else {
        if (MakeDirectory(path)){
            OG_PushLog("Assembly folder could not be created");
            return false;
        }

        ChangeDirectory(path);
        sprintf(assemblyOutName, "%s.mwa", GetFileName(path));
    }

    // Copy the source files
    char templateOutPath[OG_PATH_MAX] = {0};
    snprintf(templateOutPath, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), GetFileName(templateSourcePath));
    if (!FileCopy(templateSourcePath, templateOutPath)){
        OG_PushLog("Failed to copy the template source file into the assembly folder");
        return false;
    }

    int allSurfacesQ = 0;
    for (int i=0; i<treeListQ; i++)
        allSurfacesQ += treeList[i]->surfacesQ;
    
    char *copiedSurfaces[allSurfacesQ];
    for (int i=0; i<allSurfacesQ; i++)
        copiedSurfaces[i] = NULL;
    
    for (int i=0; i<treeListQ; i++){
        SurfaceTree *tree = treeList[i];
        
        for (int o=0; o<tree->surfacesQ; o++){
            Surface *s = tree->linearSurfaceTree[o];
            if (s->sourcePath[0] != '\0'){

                // Skip surfaces whose source file was already copied
                bool alreadyCopied = false;
                for (int u=0; u<allSurfacesQ; u++){
                    if (copiedSurfaces[u] && strcmp(copiedSurfaces[u], s->sourcePath) == 0){
                        alreadyCopied = true;
                        break;
                    }
                }

                if (alreadyCopied)
                    continue;

                char surfaceOutPath[OG_PATH_MAX] = {0};
                snprintf(surfaceOutPath, OG_PATH_MAX, "%s/%s.mwc", GetWorkingDirectory(), s->id);
                if (!FileCopy(s->sourcePath, surfaceOutPath)){
                    OG_PushLog("Failed to copy one or more surface source files into the assembly folder");
                    return false;
                }

                // Register this source file as copied
                for (int u=0; u<allSurfacesQ; u++){
                    if (copiedSurfaces[u] == NULL){
                        copiedSurfaces[u] = s->sourcePath;
                        break;
                    }
                }

            }
        }
    }

    // Copy the titleblocks too
    for (int i=0; i<titleBlocksQ; i++){
        TitleBlock *tb = titleBlocks[i];
        
        char titleBlockOut[OG_PATH_MAX] = {0};
        snprintf(titleBlockOut, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), GetFileName(tb->sourcePath));
        if (!FileCopy(tb->sourcePath, titleBlockOut)){
            OG_PushLog("Failed to copy one or more title blocks source files into the assembly folder");
            return false;
        }
    }

    // Copy the stickers too
    for (Sticker *s = stickers; s != NULL; s = s->next){
        char stickerOut[OG_PATH_MAX] = {0};
        snprintf(stickerOut, OG_PATH_MAX, "%s/%s", GetWorkingDirectory(), GetFileName(s->srcPath));
        if (!FileCopy(s->srcPath, stickerOut)){
            OG_PushLog("Failed to copy one or more sticker source files into the assembly folder");
            return false;
        }
        
        free(s->srcPath);
        s->srcPath = calloc(strlen(stickerOut)+1, sizeof(char));
        strcpy(s->srcPath, stickerOut);
    }
    
    // Start the serialization
    cJSON *j = cJSON_CreateObject();
    cJSON_AddItemToObject(j, "version", cJSON_CreateNumber(ASSEMBLIES_VERSION));
    
    cJSON *titleBlocksJson = cJSON_CreateArray();
    for (int i=0; i<titleBlocksQ; i++){
        cJSON_AddItemToArray(
            titleBlocksJson, 
            cJSON_CreateString(
                GetFileName(titleBlocks[i]->sourcePath)
            )
        );
    }
    cJSON_AddItemToObject(j, "title_blocks", titleBlocksJson);
    cJSON_AddItemToObject(j, "main_document", SerializeDocument(&mainDocument));

    cJSON_AddStringToObject(j, "template", GetFileName(templateSourcePath));
    cJSON *surface_trees = cJSON_CreateArray();
    cJSON_AddItemToObject(j, "surface_trees", surface_trees);
    for (int i=0; i<treeListQ; i++){
        SurfaceTree *tree = treeList[i];
        cJSON_AddItemToArray(surface_trees, serializeTree(tree));
    }

    // Save the project on disk
    snprintf(
        currentAssemblyPath, 
        OG_PATH_MAX, 
        "%s/%s", 
        GetWorkingDirectory(), 
        assemblyOutName
    );

    char *jsonStr = cJSON_Print(j);
    SaveFileText(assemblyOutName, jsonStr);
    free(jsonStr);
    cJSON_Delete(j);

    OG_PushLog("Panel assembly saved successfully: %s", currentAssemblyPath);
    return true;
}

bool SaveMainDoc(char *path, void *param){
    char pathwExt[strlen(path)+5];
    const char *ext = GetFileExtension(path);
    if (!ext || strcmp(ext, ".pdf"))
        sprintf(pathwExt, "%s.pdf", path);
    else sprintf(pathwExt, "%s", path);

    bool rs = ExportDoc2PDF(&mainDocument, pathwExt);
    if (!rs) OG_PushLog("Document could not be saved.");
    else OG_PushLog("Document was succesfully saved as '%s'", pathwExt);
    return rs;
}

bool SaveCutouts(char *path, void *param){
    char pathwExt[strlen(path)+5];
    const char *ext = GetFileExtension(path);
    if (!ext || strcmp(ext, ".dxf"))
        sprintf(pathwExt, "%s.dxf", path);
    else sprintf(pathwExt, "%s", path);

    bool rs = ExportSurfaceCutouts2DXF(activeTree, pathwExt);
    if (!rs) OG_PushLog("Cutouts could not be saved.");
    else OG_PushLog("Cutouts was succesfully saved as '%s'", pathwExt);
    return rs;
}

bool SaveSurfaceSVG(char *path, void *param){
    char pathwExt[strlen(path)+5];
    const char *ext = GetFileExtension(path);
    if (!ext || strcmp(ext, ".svg"))
        sprintf(pathwExt, "%s.svg", path);
    else sprintf(pathwExt, "%s", path);

    bool rs = ExportSurface2SVG(mainDocument.selectedPage->selectedEntity->surface, pathwExt);
    if (!rs) OG_PushLog("Surface Entity could not be exported.");
    else OG_PushLog("Surface Entity was succesfully exported as '%s'", pathwExt);
    return rs;
}