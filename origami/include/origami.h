#ifndef origami_h
#define origami_h


#ifndef OG_DEFAULT_FONT_SIZE
#define OG_DEFAULT_FONT_SIZE 13
#endif

#ifndef OG_VIEWPORT_TITLE_H
#define OG_VIEWPORT_TITLE_H 20
#endif

#ifndef OG_VIEWPORT_CORNER_S
#define OG_VIEWPORT_CORNER_S 12
#endif

#ifndef OG_VIEWPORT_OUTLINE_T
#define OG_VIEWPORT_OUTLINE_T 1
#endif

#ifndef OG_VIEWPORT_PANEL_HANDLE_S
#define OG_VIEWPORT_PANEL_HANDLE_S 5
#endif

// Dirty workaround, this should be correctly removed
#ifndef OG_VIEWPORT_MIN_W
#define OG_VIEWPORT_MIN_W 1
#endif
#ifndef OG_VIEWPORT_MIN_H
#define OG_VIEWPORT_MIN_H 1
#endif

#ifndef MIN_LAYOUT_SIZE
#define MIN_LAYOUT_SIZE 10
#endif

#ifndef OG_ZOOM_SPEED
#define OG_ZOOM_SPEED 0.1f
#endif

#ifndef OG_SCROLL_SPEED
#define OG_SCROLL_SPEED 8
#endif

#ifndef OG_ICONS_Q
#define OG_ICONS_Q 7
#endif

#ifndef OG_BG_C
#define OG_BG_C CLITERAL(Color){ 40, 40, 40,255}
#endif

#ifndef OG_VIEWPORT_BG_C
#define OG_VIEWPORT_BG_C CLITERAL(Color){ 55, 55, 55,255}
#endif

#ifndef OG_VIEWPORT_TITLE_C
#define OG_VIEWPORT_TITLE_C CLITERAL(Color){  0,121,241,255}
#endif

#ifndef OG_MODAL_VIEWPORT_C
#define OG_MODAL_VIEWPORT_C CLITERAL(Color){241, 60, 60,255}
#endif

#ifndef OG_TEXT_C
#define OG_TEXT_C CLITERAL(Color){200,200,200,255}
#endif

#ifndef OG_HIGHLIGHT_C
#define OG_HIGHLIGHT_C CLITERAL(Color){  0,121,241,255}
#endif

#ifndef OG_VIEWPORT_OUTLINE_C
#define OG_VIEWPORT_OUTLINE_C OG_BG_C
#endif

#ifndef OG_CMD_BAR_C
#define OG_CMD_BAR_C CLITERAL(Color){ 30, 30, 30,255}
#endif

#ifndef OG_CMD_BAR_VIEWPORT_C
#define OG_CMD_BAR_VIEWPORT_C CLITERAL(Color){ 35, 60, 90,255}
#endif

#ifndef OG_LOGS_C
#define OG_LOGS_C CLITERAL(Color){255,255,  0,255}
#endif

#ifndef OG_MODKEY
#define OG_MODKEY KEY_LEFT_SHIFT
#endif

#ifndef OG_COMMAND_BAR_KEY
#define OG_COMMAND_BAR_KEY KEY_ENTER
#endif

#define OG_PATH_MAX 4096

#include <raylib.h>
#include "microui.h"
#include "origami_tools.h"

typedef struct OG_LayoutLine OG_LayoutLine;
typedef struct OG_LayoutContainer OG_LayoutContainer;
typedef struct OG_Layout OG_Layout;
typedef struct OG_Viewport OG_Viewport;
typedef struct OG_ViewportLinkedList OG_ViewportLinkedList;
typedef struct OG_Context OG_Context;

typedef enum {
    OG_LAYOUT_LINE_H,
    OG_LAYOUT_LINE_V
} OG_LayoutLineType;

typedef enum {
    OG_LAYOUT_LINE_TOP,
    OG_LAYOUT_LINE_BOTTOM,
    OG_LAYOUT_LINE_LEFT,
    OG_LAYOUT_LINE_RIGHT
} OG_LayoutFixedLine;

struct OG_LayoutLine{
    OG_LayoutLineType type;
    bool fixed;
    float t;
    OG_Layout *layout;
};

struct OG_LayoutContainer{
    OG_LayoutLine *lines[4];
    Rectangle r;
    OG_Viewport *v;
    OG_Layout *layout;
};

struct OG_Layout{
    Vector2 size;
    OG_LayoutLine lines[256];
    OG_LayoutContainer containers[256];
    int linesQ, containersQ;
    OG_Viewport *viewport;
};

struct OG_Viewport{
    char *title;
    char *header;
    Rectangle size;
    Vector2 pos;
    RenderTexture renderTexture;
    Camera2D camera;
    mu_Context ctx;

    float minZoom;
    float maxZoom;
    
    bool hidden;
    bool hideCmd;
    bool resizable;
    bool disableOffsetUpdate;
    bool isModal;
    bool noTitleBar;
    bool updateAlways;
    bool renderAlways;
    bool noBorder;

    bool needsRedraw;

    OG_Layout *layout;
    OG_LayoutContainer *container;

    void (*Init)(struct OG_Viewport*);
    void (*Update)(struct OG_Viewport*);
    void (*OnResize)(struct OG_Viewport*);
    const char** (*GetCmds)();
    void (*ExecCmd)(struct OG_Viewport*, int argc, char **argv);
    
    void (*UI)(struct OG_Viewport*, mu_Context*);
    void (*RightPanel)(struct OG_Viewport*, mu_Context*);
    void (*LeftPanel)(struct OG_Viewport*, mu_Context*);
    void (*TopPanel)(struct OG_Viewport*, mu_Context*);
    void (*BottomPanel)(struct OG_Viewport*, mu_Context*);
    
    void (*RenderUnderlay)(struct OG_Viewport*);
    void (*Render)(struct OG_Viewport*);
    void (*RenderOverlay)(struct OG_Viewport*);
    void (*RenderOnScreen)(struct OG_Viewport*, Vector2);

    struct OG_Viewport *prev;
    struct OG_Viewport *next;
};

struct OG_ViewportLinkedList{
    OG_Viewport *head;
    OG_Viewport *tail;
};

typedef enum{
    OG_STATE_IDLE,
    OG_STATE_MOVING_VIEWPORT,
    OG_STATE_RESIZING_LAYOUT,
    OG_STATE_RESIZING_LAYOUT_LINE,
    OG_STATE_RESIZING_VIEWPORT,
    OG_STATE_ON_COMMAND_BAR
} OG_State;

typedef enum{
    OG_LEFT_PANEL,
    OG_RIGHT_PANEL,
    OG_TOP_PANEL,
    OG_BOTTOM_PANEL
} OG_PanelType;

struct OG_Context{
    OG_State state;
    OG_ViewportLinkedList viewports;
    OG_Viewport *viewportsToShow[5];
    OG_LayoutContainer *targetContainer;
    OG_LayoutLine *targetLine;
    OG_Viewport *targetViewport;
    OG_Viewport *modalViewport;
    Texture icons;
    Vector2 grabOffset;
    int *targetPanelSize;
    int targetPanelSizePreview;
    OG_PanelType targetPanelType;
    
    // Font
    Font defaultFont;
    int defaultFontSize;
    
    // Logs
    int logsQ;
    char *logs[256];
    float logsTimer;

    // Command bar
    char cmdBuf[256];
    char *hintsBuf[256];
    int cmdCursor;
    int hintsQ;
    int selectedHint;

    // Cursor
    MouseCursor currentCursor;
    MouseCursor nextCursor;

    // Flags
    bool drawFps;
    bool viewportJustSwitched;

    void (*RenderBackground)(void);
};

extern OG_Context OG;

float GetRatio(int units, int maxSize);
void OG_PushLog(char *format, ...);
void OG_PushLogSimple(char *log);
OG_Layout *OG_InitLayout(char *name, Rectangle r);
OG_LayoutLine *OG_LayoutAddLine(OG_Layout *l, OG_LayoutLineType type, float t);
OG_LayoutContainer *OG_LayoutAddContainer(OG_Layout *layout, OG_LayoutLine *top, OG_LayoutLine *bottom, OG_LayoutLine *left, OG_LayoutLine *right);
void OG_LayoutAddViewport(OG_LayoutContainer *c, OG_Viewport *v);
Vector2 OG_ResizeViewport(OG_Viewport *v, int w, int h);
void OG_SetViewportOnTop(OG_Viewport *v);
bool OG_MouseInViewport(OG_Viewport* v, bool titleBar, bool resizeHandle, bool onlyViewport);
float OG_GetMouseWheelMove(OG_Viewport *v);
Vector2 OG_GetMouseOverlayPosition(OG_Viewport* v);
Vector2 OG_GetMouseViewportPosition(OG_Viewport* v);
bool OG_IsKeyPressed(OG_Viewport *v, int key);
bool OG_IsKeyReleased(OG_Viewport *v, int key);
bool OG_IsKeyDown(OG_Viewport *v, int key);
bool OG_IsKeyUp(OG_Viewport *v, int key);
bool OG_IsMouseButtonPressed(OG_Viewport* v, int button);
bool OG_IsMouseButtonReleased(OG_Viewport* v, int button);
bool OG_IsMouseButtonDown(OG_Viewport *v, int button);
bool OG_IsMouseButtonUp(OG_Viewport *v, int button);
void OG_ViewportUpdateZoom(OG_Viewport* v);
int OG_ViewportUpdatePan(OG_Viewport* v);
OG_Viewport *OG_GetViewportByName(char *name);
void OG_ToggleLayouts(OG_Viewport *host);
void OG_ToggleViewport(OG_Viewport *v);
void OG_ToggleViewportByName(char *name);
void OG_OpenViewportByName(char *name);
void OG_CloseViewportByName(char *name);
void OG_ChangeCursor(OG_Viewport *v, MouseCursor c);
void OG_UnsetHeader(OG_Viewport *v);
void OG_SetHeader(OG_Viewport *v, char *header);
OG_Viewport *OG_InitViewport(char* title, 
                    Rectangle rect,
                    float minZoom, float maxZoom,
                    bool hideCmd,
                    bool resizable,
                    bool isModal,
                    void (*Init)(OG_Viewport*), 
                    void (*Update)(OG_Viewport*), 
                    void (*OnResize)(OG_Viewport*),
                    void (*Render)(OG_Viewport*), 
                    void (*RenderOverlay)(OG_Viewport*),
                    void (*RenderUnderlay)(OG_Viewport*),
                    void (*RenderOnScreen)(OG_Viewport*, Vector2),
                    void (*UI)(struct OG_Viewport*, mu_Context*),
                    const char** (GetCmds)(),
                    void (*ExecCmd)(OG_Viewport*, int argc, char **argv)
                    );

int OG_Init(char* title, int fps);
bool OG_UpdateFrame();
bool OG_RenderFrame();
bool OG_Update();
void OG_Free();

#endif