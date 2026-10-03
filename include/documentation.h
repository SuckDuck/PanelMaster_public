#ifndef DOCUMENTATION_H
#define DOCUMENTATION_H

#include "caddraw.h"
#include "stickers.h"
#include "surfaces.h"
#include "titleblock.h"
#include <raylib.h>

// A3 by default
#define DEFAULT_PAGE_W 420
#define DEFAULT_PAGE_H 297

typedef struct Document Document;
typedef struct Page Page;
typedef struct PageEntity PageEntity;
typedef struct PageEntitySurface PageEntitySurface;
typedef struct PageEntityText PageEntityText;
typedef struct PageEntityTitleBlockProp PageEntityTitleBlockProp;
typedef struct PageEntityTitleBlock PageEntityTitleBlock;
typedef struct PageEntitySticker PageEntitySticker;

typedef enum PageEntityType {
    PAGE_ENTITY_SURFACE,
    PAGE_ENTITY_TEXT,
    PAGE_ENTITY_TITLEBLOCK,
    PAGE_ENTITY_STICKER,
} PageEntityType;

typedef enum PageEntitySurfaceFlags {
    PESF_DRAW_ANNOTATIONS = 1 << 0,
    PESF_DRAW_SKINS       = 1 << 1,
    PESF_DRAW_CUTOUTS     = 1 << 2,
} PageEntitySurfaceFlags;

struct PageEntitySurface {
    PageEntity *entity;
    Surface *surface;
    float scale;
    uint32_t flags;
    float annotationsFontSize;
    float annotationsArrowSize;
    int lod;
};

struct PageEntityText {
    PageEntity *entity;
    char *text;
    float fontSize;
    Color color;
    CadTextAlign alignment;
};

struct PageEntityTitleBlockProp {
    PageEntityTitleBlock *parent;
    TitleBlockRect *r;
    char *content;
    PageEntityTitleBlockProp *next;
};

struct PageEntityTitleBlock {
    PageEntity *entity;
    TitleBlock *block;
    PageEntityTitleBlockProp *properties;
};

struct PageEntitySticker {
    PageEntity *entity;
    Sticker *sticker;
    float scale;
};

struct PageEntity {
    PageEntityType type;
    char *name;
    Vector2 pos;

    PageEntitySurface *surface;
    PageEntityText *text;
    PageEntityTitleBlock *block;
    PageEntitySticker *sticker;
    
    PageEntity *next;
    Page *page;
};

struct Page {
    char *name;
    
    PageEntity *entities;
    PageEntity *selectedEntity;
    int entitiesQ;

    bool drawFrame;
    struct {
        float top;  float bottom;
        float left; float right;
    } frame;    
    
    Document *document;
    Page *next;
};

struct Document {
    int width;
    int height;

    Page *pages;
    Page *lastInsertedPage;
    Page *selectedPage;
    int pagesQ;

};

Page *NewPage(Document *document, char *name);
void DeletePage(Page *p);
void SelectPage(Page *p);
void SelectEntity(PageEntity *e);
PageEntitySurface *AddSurfaceEntity(Page *p, Surface *s);
PageEntityText *AddTextEntity(Page *p, char *text);
PageEntityTitleBlock *AddTitleBlockEntity(Page *p, TitleBlock *tb);
PageEntityTitleBlockProp *AddTitleBlockEntityProp(PageEntityTitleBlock *tbe, TitleBlockRect *r);
PageEntitySticker *AddStickerEntity(Page *p, Sticker *s);
void DeleteTitleBlockEntityProp(PageEntityTitleBlockProp *prop);
void DeleteEntity(PageEntity *e);
bool ShiftEntityUp(PageEntity *e);
bool ShiftEntityDown(PageEntity *e);
bool ExportDoc2PDF(Document *doc, char *path);
bool ExportSurface2SVG(PageEntitySurface *s, char *path);
bool ExportSurfaceCutouts2DXF(SurfaceTree *t, char *path);

#endif