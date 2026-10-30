#include <raylib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "origami.h"
#include "microui.h"
#include "origami_tools.h"

typedef struct FileEntrie {
    char *path;
    char *filename;
    bool isDir;
} FileEntrie;

typedef struct FileList {
    unsigned int count;
    FileEntrie *entries;
} FileList;

static const char *commands[] = {
    NULL
};

static OG_Viewport *this;
static FileList pwdFiles;
static FileEntrie *selectedFile;
static char *filter;
static char filename[OG_PATH_MAX];
static float contentSize = 0.0f;
static float scroll = 0.0f;

static OG_FileDialogMode currentMode;
static bool (*Ok)(char*, void*);
void *okParams;
static void (*OnClose)(bool ok);

static void LowerCaseStr(char *str){
    for(int i = 0; str[i]; i++)
        str[i] = tolower(str[i]);
}

static void SortFileList(FileList *fl){
    if (fl->count <= 0) return;
    bool repeat = false;
    while (true){
        repeat = false;

        for (int i=0; i<fl->count-1; i++){
            if (strcmp(fl->entries[i].filename, fl->entries[i+1].filename) > 0){
                FileEntrie tmp = fl->entries[i];
                fl->entries[i] = fl->entries[i+1];
                fl->entries[i+1] = tmp;
                repeat = true;
            }
        }
        
        if (!repeat) break;
    }
}

static FileList InitFileList(char *dir){
    FilePathList f = LoadDirectoryFiles(dir);
    FileList fl = {0};

    for (int i=0; i<f.count; i++){
        if (!filter){
            fl.count++;
            continue;
        }
        const char *ext = GetFileExtension(f.paths[i]);
        if (!IsPathFile(f.paths[i]) || (ext && strcmp(ext, filter) == 0))
            fl.count++;
    }

    fl.entries = calloc(fl.count, sizeof(FileEntrie));
    int ii=0;
    for (int i=0; i<f.count; i++){
        bool isDir = !IsPathFile(f.paths[i]);
        if (filter){
            const char *ext = GetFileExtension(f.paths[i]);
            if (!isDir && (!ext || strcmp(ext, filter) != 0))
                continue;
        }

        FileEntrie *e = &fl.entries[ii++];

        e->isDir = isDir;
        e->path = calloc(strlen(f.paths[i])+1, sizeof(char));
        strcpy(e->path, f.paths[i]);

        // +2 because of \0 and / in case of path being a dir        
        e->filename = calloc(strlen(GetFileName(e->path))+2, sizeof(char));
        strcpy(e->filename, GetFileName(e->path));

        if (e->isDir)
            e->filename[strlen(GetFileName(e->path))] = '/';
        LowerCaseStr(e->filename);
    }

    SortFileList(&fl);
    UnloadDirectoryFiles(f);
    return fl;
}

static void FreeFileList(FileList fl){
    for (int i=0; i<fl.count; i++){
        FileEntrie *e = &fl.entries[i];
        free(e->path);
        free(e->filename);
    }
        
    free(fl.entries);
    fl.count = 0;
}

bool ChangeDirectoryHelper(const char *path){
    char *before = strdup(GetWorkingDirectory());
    ChangeDirectory(path);
    bool changed = strcmp(before, GetWorkingDirectory()) != 0;
    free(before);
    return changed;
}

static void Init(OG_Viewport *v){
    pwdFiles = InitFileList(".");
    v->updateAlways = true;
    v->renderAlways = true;
    this = v;
}

static void Update(OG_Viewport *v){
    if (v->hidden) return;
    // Scroll logic
    contentSize = (OG.defaultFontSize+2) * pwdFiles.count;
    if (contentSize <= v->size.height*-1)
        scroll = 0;
    
    else {
        scroll += OG_GetMouseWheelMove(v) * OG_SCROLL_SPEED;
        if (scroll > 0) scroll = 0;
        if (scroll < v->size.height*-1 - contentSize){
            scroll = v->size.height*-1 - contentSize;
        }
    }

    // Select entrie
    float verticalOffset = 0;
    for (int i=0; i<pwdFiles.count; i++){
        FileEntrie *e = &pwdFiles.entries[i];
        Rectangle r = {
            0, scroll+verticalOffset, 
            v->size.width, 
            OG.defaultFontSize+2
        };

        if (OG_IsMouseButtonPressed(v, MOUSE_LEFT_BUTTON)){
            if (CheckCollisionPointRec(OG_GetMouseOverlayPosition(v), r)){
                if (selectedFile == e){
                    if (e->isDir){
                        if (ChangeDirectoryHelper(e->path)){
                            selectedFile = NULL;
                            scroll = 0;
                            FreeFileList(pwdFiles);
                            pwdFiles = InitFileList(".");
                            return;
                        }

                        else{
                            OG_PushLog("Dir could not be changed");
                        }
                    }
                    
                    else{
                        if (Ok != NULL && Ok(filename, okParams)){
                            OG_CloseViewportByName(v->container->layout->viewport->title);
                            if (OnClose) OnClose(true);
                        }
                            
                            
                    }
                }
                    
                else {
                    selectedFile = e;
                    strcpy(filename, e->path);
                }

            }
        }

        verticalOffset += OG.defaultFontSize+2;
    }

}

static const char **GetCmds(){
    return commands;
}

static void RenderOverlay(OG_Viewport *v){
    if (v->hidden) return;
    float verticalOffset = 0.0f;
    for (int i=0; i<pwdFiles.count; i++){
        FileEntrie *e = &pwdFiles.entries[i];
        DrawTextEx(
            OG.defaultFont, 
            e->filename, 
            (Vector2){0, scroll + verticalOffset},
            OG.defaultFontSize, 2, selectedFile == e ? OG_HIGHLIGHT_C:OG_TEXT_C
        );

        verticalOffset += OG.defaultFontSize+2;
    }

    // RENDER SCROLL BAR
    if (contentSize > v->size.height*-1){
        DrawRectangle( //bg
            v->size.width - (OG.defaultFontSize+2), 0,
            (OG.defaultFontSize+2), v->size.height*-1,
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLBASE]
        );

        float scrollPercent = scroll*-1/contentSize;
        DrawRectangle( //thumb
            v->size.width - (OG.defaultFontSize+2), v->size.height*-1*scrollPercent, 
            (OG.defaultFontSize+2), 
            (v->size.height*-1/contentSize)*(v->size.height*-1),
            *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLTHUMB]
        );
    }

}

static void AdressBar(OG_Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 2, (const int[]){-40, -1}, 25);
    mu_textbox_ex(ctx, (char*) GetWorkingDirectory(), OG_PATH_MAX, MU_OPT_NOINTERACT);
    if(mu_button(ctx, "Up")){
        if (ChangeDirectoryHelper("..")){
            scroll = 0;
            selectedFile = NULL;
            FreeFileList(pwdFiles);
            pwdFiles = InitFileList(".");
        }
    }
}

static void Footer(OG_Viewport *v, mu_Context *ctx){
    
    mu_layout_row(ctx, 4, (const int[]){80, -150, 70, -1}, 25);
    
    if (currentMode != OG_FD_MODE_SELECT_DIR){
        mu_label(ctx, "FileName:");
        if (mu_textbox(ctx, filename, OG_PATH_MAX) && MU_RES_CHANGE){
            selectedFile = NULL;
        }
    }
    
    else{
        mu_Rect r = mu_layout_next(ctx);
        mu_draw_rect(ctx, r, ctx->style->colors[MU_COLOR_WINDOWBG]);

        r = mu_layout_next(ctx);
        mu_draw_rect(ctx, r, ctx->style->colors[MU_COLOR_WINDOWBG]);
    }
    
    if (currentMode == OG_FD_MODE_SELECT_DIR){
        if (selectedFile && selectedFile->isDir){
            if (mu_button(ctx, "Ok") && Ok && Ok(filename, okParams)){
                OG_CloseViewportByName(v->container->layout->viewport->title);
                if (OnClose) OnClose(true);
            }
                
        }
        
        else {
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, ctx->style->colors[MU_COLOR_WINDOWBG]);
        }
    }
    
    else if (mu_button(ctx, "Ok")){
        if (selectedFile && selectedFile->isDir){
            if (ChangeDirectoryHelper(selectedFile->path)){
                selectedFile = NULL;
                scroll = 0;
                FreeFileList(pwdFiles);
                pwdFiles = InitFileList(".");
            }
        }
        
        else if (Ok != NULL && Ok(filename, okParams)){
            OG_CloseViewportByName(v->container->layout->viewport->title);
            if (OnClose) OnClose(true);
        }
            
    }

    if (mu_button(ctx, "Cancel")){
        if (selectedFile)
            selectedFile = NULL;
        else {
            OG_CloseViewportByName(v->container->layout->viewport->title);
            if (OnClose) OnClose(false);
        }
    }

}

void OG_FileDialog(){
    const int width = 500;
    const int height = 300;

    OG_Layout *l = OG_InitLayout("FileDialog", (Rectangle){0,0, width, height});
    l->viewport->hideCmd = true;
    l->viewport->isModal = true;
    l->viewport->resizable = false;
    
    // LINES
    OG_LayoutLine *topLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_H, GetRatio(38, height));
    topLine->fixed = true;
    OG_LayoutLine *bottomLine = OG_LayoutAddLine(l, OG_LAYOUT_LINE_H, GetRatio(height-38, height));
    bottomLine->fixed = true;

    // ADDRESS BAR
    OG_LayoutContainer *addressBar = OG_LayoutAddContainer(
        l, 
        &l->lines[OG_LAYOUT_LINE_TOP], topLine, 
        &l->lines[OG_LAYOUT_LINE_LEFT], &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutAddViewport(
        addressBar,
        OG_InitViewport(
            "FileDialog_AddressBar", 
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
            &AdressBar, 
            NULL, 
            NULL
        )
    );

    // FileList
    OG_LayoutContainer *fileList = OG_LayoutAddContainer(
        l, 
        topLine, bottomLine,
        &l->lines[OG_LAYOUT_LINE_LEFT], &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutAddViewport(
        fileList,
        OG_InitViewport(
            "FileDialog_FileList", 
            (Rectangle){0,0,500,300}, 
            1.0f, 1.0f, 
            true, false, false, 
            &Init, 
            &Update, 
            NULL, 
            NULL, 
            &RenderOverlay, 
            NULL, 
            NULL,
            NULL, 
            &GetCmds, 
            NULL
        )
    );

    // Footer
    OG_LayoutContainer *footer = OG_LayoutAddContainer(
        l, 
        bottomLine, &l->lines[OG_LAYOUT_LINE_BOTTOM],
        &l->lines[OG_LAYOUT_LINE_LEFT], &l->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutAddViewport(
        footer, 
        OG_InitViewport(
            "FileDialog_Footer", 
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
            &Footer, 
            NULL, 
            NULL
        )
    );

}

void OG_OpenFileDialog(
    bool (*ok_callback)(char*, void*), 
    void *okparam, 
    void (*close_callback)(bool ok), 
    OG_FileDialogMode mode, 
    char *ext, 
    char *msg){
        
        Ok = ok_callback;
        okParams = okparam;
        OnClose = close_callback;
        currentMode = mode;
        selectedFile = NULL;
        if (filter)
            free(filter);
        filter = NULL;
        if (ext) filter = strdup(ext);

        OG_Viewport *v = OG_GetViewportByName("FileDialog");
        
        OG_SetHeader(v, msg);
        OG_ToggleViewportByName("FileDialog");
        if (pwdFiles.count > 0 && pwdFiles.entries)
            FreeFileList(pwdFiles);
        pwdFiles = InitFileList(".");
}