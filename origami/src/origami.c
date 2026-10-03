#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include "origami.h"
#include "IBMPlexMono-Medium.h"
#include "icons.h"
#include "microui.h"

OG_Context OG;
extern void OG_FileDialog();
extern void OG_InputDialog();
extern void OG_ContextMenu();

/* <== Utils ===================================================> */

static int text_width(mu_Font font, const char *str, int len){
    return MeasureTextEx(*(Font*)font,str,OG.defaultFontSize,2).x;
}

static int text_height(mu_Font font){
    return OG.defaultFontSize;
}

static RenderTexture2D LoadCustomRenderTexture(int width, int height){
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(NULL, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19; //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

static bool IsPointOnRect(Vector2 p, Rectangle r){
    return p.x > r.x && p.x < r.x+r.width && p.y > r.y && p.y < r.y+r.height;
}

static Color InvertColor(Color color){
    return (Color){
        255 - color.r,
        255 - color.g,
        255 - color.b,
        255
    };
}

static Rectangle GetViewportRect(OG_Viewport *v){
    if (v->layout){
        if (OG.viewports.tail == v){}
        else if (OG.viewports.tail->container && OG.viewports.tail->container->layout->viewport == v){}
        else {
            Rectangle viewportArea = {
                v->pos.x, v->pos.y,
                v->layout->size.x,
                v->layout->size.y + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)
            };

            return viewportArea;
        }
    }
    
    
    Rectangle viewportArea = {
        v->pos.x,v->pos.y,
        v->size.width,
        (v->size.height*-1) + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)
    };

    return viewportArea;
}

static bool IsViewportFullyVisible(OG_Viewport *v){
    if (!v) return false;
    if (v->hidden) return false;

    Rectangle vRect = GetViewportRect(v);
    for (OG_Viewport *vv = v->next; vv != NULL; vv = vv->next){ // for each viewport on front v
        if (vv->hidden) continue;
        Rectangle vvRect = GetViewportRect(vv);
        if (CheckCollisionRecs(vRect, vvRect))
            return false;
    }

    return true;
}

float GetRatio(int units, int maxSize){
    if (maxSize == 0)
        return 0.0f;
    return (float)units / maxSize;
}

/* <== Logs Section ============================================> */

void OG_PushLog(char *format, ...){
    va_list args;
    
    // PUSH THE LOG TO BE PRINTED IN SCREEN
    if (OG.logsQ < 256){
        va_start(args, format);
        OG.logs[OG.logsQ] = (char*) calloc(256, sizeof(char));
        vsnprintf(OG.logs[OG.logsQ++], 256, format, args);
        va_end(args);
    }
    
    // PUSH THE LOG TO BE PRINTED IN STDOUT
    va_start(args, format);
    int logLen = strlen(format);
    char logFormat[logLen+2];
    strcpy(logFormat,format);
    logFormat[logLen] = '\n';
    logFormat[logLen + 1] = '\0';
    vprintf(logFormat,args);
    va_end(args);

    if (OG.logsQ == 1)
        OG.logsTimer = 0;
}

void OG_PushLogSimple(char *log){
    OG_PushLog("%s", log);
}

static void PopLog(){
    if (OG.logsQ <= 0) return; 
    free(OG.logs[0]);
    for (int i=0; i<OG.logsQ-1; i++){
        OG.logs[i] = OG.logs[i+1];
    }
    OG.logsQ--;
    OG.logsTimer = 0.0;
}

/* <== UI Section ==============================================> */

void UpdateViewportUIInput(OG_Viewport *v){
    Vector2 mousePosition = OG_GetMouseOverlayPosition(v);
    
    mu_input_mousemove(&v->ctx, mousePosition.x, mousePosition.y);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) 
            mu_input_mouseup(&v->ctx,mousePosition.x,mousePosition.y,MU_MOUSE_LEFT);
    if (IsKeyReleased(KEY_LEFT_SHIFT)) mu_input_keyup(&v->ctx, MU_KEY_SHIFT); 
    if (IsKeyReleased(KEY_ENTER)) mu_input_keyup(&v->ctx, MU_KEY_RETURN);    
    if (IsKeyReleased(KEY_BACKSPACE)) mu_input_keyup(&v->ctx, MU_KEY_BACKSPACE);

    if (v == OG.viewports.tail){
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) 
            mu_input_mousedown(&v->ctx, mousePosition.x,mousePosition.y,MU_MOUSE_LEFT);
        mu_input_scroll(&v->ctx, 0, GetMouseWheelMove()*OG_SCROLL_SPEED*-1);
        if (IsKeyPressed(KEY_LEFT_SHIFT)) mu_input_keydown(&v->ctx, MU_KEY_SHIFT);    
        if (IsKeyPressed(KEY_ENTER)) mu_input_keydown(&v->ctx, MU_KEY_RETURN);
        if (IsKeyPressed(KEY_BACKSPACE)) mu_input_keydown(&v->ctx, MU_KEY_BACKSPACE);
        
        while (true){
            char p[] = {GetCharPressed(), 0};
            if (p[0] == 0) break;
            mu_input_text(&v->ctx, p);
        }
    }
}

void CleanViewportUIInput(OG_Viewport *v){
    mu_input_mousemove(&v->ctx, 0, 0);
    mu_input_mouseup(&v->ctx, 0, 0, MU_MOUSE_LEFT);
    mu_input_mouseup(&v->ctx, 0, 0, MU_MOUSE_RIGHT);
    mu_input_scroll(&v->ctx, 0, 0);
}

void ProcessViewportUI(OG_Viewport *v){
    mu_begin(&v->ctx);
    if (v->UI != NULL){
        mu_Rect rect = mu_rect(
            0, 0, 
            v->size.width, 
            v->size.height*-1
        );

        mu_begin_window_ex(&v->ctx, "UI", rect, MU_OPT_NOCLOSE | MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOFRAME);
        mu_Container *cnt = mu_get_current_container(&v->ctx);
        cnt->rect = rect;
        
        v->UI(v,&v->ctx);
        mu_end_window(&v->ctx);
    }
    mu_end(&v->ctx);
}

void RenderViewportUI(OG_Viewport *v){
    mu_Command *cmd = NULL;
    while (mu_next_command(&v->ctx, &cmd)){
        switch(cmd->type){
            case MU_COMMAND_TEXT: {
                const char *start = cmd->text.str;
                const char *end = start;
                float y = cmd->text.pos.y;

                while (1) {
                    while (*end && *end != '\n')
                        end++;

                    DrawTextEx(
                        *(Font*)cmd->text.font,
                        TextSubtext(start, 0, (int)(end - start)),
                        (Vector2){ cmd->text.pos.x, y },
                        OG.defaultFontSize,
                        2,
                        *(Color*)&cmd->text.color
                    );

                    if (*end == '\0')
                        break;

                    y += OG.defaultFontSize;
                    start = ++end;
                }

                break;
            }

            case MU_COMMAND_RECT: {
                DrawRectangle(
                    cmd->rect.rect.x, 
                    cmd->rect.rect.y, 
                    cmd->rect.rect.w, 
                    cmd->rect.rect.h, 
                    *(Color*)&cmd->rect.color
                );
                break;
            }

            case MU_COMMAND_ICON: {
                Texture2D icons = OG.icons;
                int iconsWidth = icons.width/OG_ICONS_Q;
                DrawTexturePro(
                    OG.icons,
                    (Rectangle){iconsWidth*(cmd->icon.id-1),0,iconsWidth,icons.height}, 
                    (Rectangle){cmd->icon.rect.x, cmd->icon.rect.y, cmd->icon.rect.w, cmd->icon.rect.h}, 
                    (Vector2){0,0}, 
                    0.0, 
                    OG_TEXT_C
                );
                break;
            }
            
            case MU_COMMAND_CLIP: {
                mu_Rect r = cmd->clip.rect;    
                if (r.x == 0 && r.y == 0){
                    EndScissorMode();
                    break;
                }
                
                BeginScissorMode(r.x, r.y, r.w, r.h);
                break;
            }

        }
    }
}

/* <== Layouts Section =========================================> */

static void CalcLayout(OG_Viewport *v){
    OG_Layout *l = v->layout;
    for (int i=0; i<l->containersQ; i++){
        OG_LayoutContainer *container = &l->containers[i];
        
        Vector2 p = {
            (int)Lerp(
                v->pos.x, 
                v->pos.x + l->size.x, 
                container->lines[OG_LAYOUT_LINE_LEFT]->t
            ),

            (int)Lerp(
                v->pos.y + (v->noTitleBar?0:OG_VIEWPORT_TITLE_H), 
                v->pos.y + (v->noTitleBar?0:OG_VIEWPORT_TITLE_H) + l->size.y,
                container->lines[OG_LAYOUT_LINE_TOP]->t
            )
        };
        
        container->r = (Rectangle){
            p.x, p.y,
            (int)(Lerp(
                v->pos.x, 
                v->pos.x + l->size.x, 
                container->lines[OG_LAYOUT_LINE_RIGHT]->t
            ) - p.x),
            
            (int)(Lerp(
                v->pos.y + (v->noTitleBar?0:OG_VIEWPORT_TITLE_H), 
                v->pos.y + (v->noTitleBar?0:OG_VIEWPORT_TITLE_H) + l->size.y, 
                container->lines[OG_LAYOUT_LINE_BOTTOM]->t
            ) - p.y)
        };
    }
}

static float GetLineMinLimit(OG_LayoutContainer *c, OG_LayoutLine *line, int minPixels){
    float min = 0.0f;
    float margin;

    // LEFT
    if (line == c->lines[OG_LAYOUT_LINE_LEFT]){
        margin = GetRatio(minPixels, c->layout->size.x);
        for (int i=0; i<c->layout->containersQ; i++){
            OG_LayoutContainer *ci = &c->layout->containers[i];
            if (line == ci->lines[OG_LAYOUT_LINE_RIGHT] && ci->lines[OG_LAYOUT_LINE_LEFT]->t > min)
                min = ci->lines[OG_LAYOUT_LINE_LEFT]->t;
        }
    }

    // RIGHT
    if (line == c->lines[OG_LAYOUT_LINE_RIGHT]){
        margin = GetRatio(minPixels, c->layout->size.x);
        min = c->lines[OG_LAYOUT_LINE_LEFT]->t;
    }
        
    
    // TOP
    if (line == c->lines[OG_LAYOUT_LINE_TOP]){
        margin = GetRatio(minPixels, c->layout->size.y);
        for (int i=0; i<c->layout->containersQ; i++){
            OG_LayoutContainer *ci = &c->layout->containers[i];
            if (line == ci->lines[OG_LAYOUT_LINE_BOTTOM] && ci->lines[OG_LAYOUT_LINE_TOP]->t > min)
                min = ci->lines[OG_LAYOUT_LINE_TOP]->t;
        }
    }

    // BOTTOM
    if (line == c->lines[OG_LAYOUT_LINE_BOTTOM]){
        margin = GetRatio(minPixels, c->layout->size.y);
        min = c->lines[OG_LAYOUT_LINE_TOP]->t;
    }
        
    return min + margin;
}

static float GetLineMaxLimit(OG_LayoutContainer *c, OG_LayoutLine *line, int minPixels){
    float max = 1.0f;
    float margin;
    
    // LEFT
    if (line == c->lines[OG_LAYOUT_LINE_LEFT]){
        margin = GetRatio(minPixels, c->layout->size.x);
        max = c->lines[OG_LAYOUT_LINE_RIGHT]->t;
    }
        
    // RIGHT
    if (line == c->lines[OG_LAYOUT_LINE_RIGHT]){
        margin = GetRatio(minPixels, c->layout->size.x);
        for (int i=0; i<c->layout->containersQ; i++){
            OG_LayoutContainer *ci = &c->layout->containers[i];
            if (line == ci->lines[OG_LAYOUT_LINE_LEFT] && ci->lines[OG_LAYOUT_LINE_RIGHT]->t < max)
                max = ci->lines[OG_LAYOUT_LINE_RIGHT]->t;
        }
    }

    // TOP
    if (line == c->lines[OG_LAYOUT_LINE_TOP]){
        margin = GetRatio(minPixels, c->layout->size.y);
        max = c->lines[OG_LAYOUT_LINE_BOTTOM]->t;
    }

    // BOTTOM
    if (line == c->lines[OG_LAYOUT_LINE_BOTTOM]){
        margin = GetRatio(minPixels, c->layout->size.y);
        for (int i=0; i<c->layout->containersQ; i++){
            OG_LayoutContainer *ci = &c->layout->containers[i];
            if (line == ci->lines[OG_LAYOUT_LINE_TOP] && ci->lines[OG_LAYOUT_LINE_BOTTOM]->t < max) 
                max = ci->lines[OG_LAYOUT_LINE_BOTTOM]->t;
        }
    }
    
    return max - margin;
}

static void ApplyLayout(OG_Layout *l){
    for (int i=0; i<l->containersQ; i++){
        OG_LayoutContainer *container = &l->containers[i];
        OG_Viewport *v = container->v;
        if (!v) continue;

        if (l->viewport->hidden != v->hidden)
            OG_ToggleViewport(v);
        
        v->pos = (Vector2){container->r.x, container->r.y};
        if (v->size.width != container->r.width || (v->size.height*-1) != container->r.height)
            OG_ResizeViewport(v, container->r.width, container->r.height);
        
    }
}

/* <== Render Section ==========================================> */

static void RenderViewport(OG_Viewport *v){
    BeginTextureMode(v->renderTexture);
    ClearBackground(OG_VIEWPORT_BG_C);
    if (v->RenderUnderlay != NULL) v->RenderUnderlay(v);
    BeginMode2D(v->camera);
    if (v->Render != NULL) v->Render(v);
    EndMode2D();
    if (v->RenderOverlay != NULL) v->RenderOverlay(v);
    RenderViewportUI(v);
    EndTextureMode();
}

static void DrawViewport(OG_Viewport *v){
    Vector2 pos = (Vector2){
        v->pos.x +1,
        v->pos.y + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)
    };

    DrawTextureRec(v->renderTexture.texture,v->size,pos,WHITE);

    // DRAW BORDER
    if (!v->noBorder){
        Rectangle border = (Rectangle){
            v->pos.x, v->pos.y,
            v->size.width + 1,
            (v->size.height*-1) + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)
        };

        DrawRectangleLinesEx(
            border,
            OG_VIEWPORT_OUTLINE_T,
            v->isModal ? OG_MODAL_VIEWPORT_C : (v == OG.viewports.tail ? OG_VIEWPORT_TITLE_C : OG_VIEWPORT_OUTLINE_C)
        );
    }
    
    // DRAW TITLEBAR
    DrawRectangle(
        v->pos.x,v->pos.y, 
        v->size.width, 
        (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H), 
        v->isModal ? OG_MODAL_VIEWPORT_C : OG_VIEWPORT_TITLE_C );
    
    if (!v->noTitleBar){
        DrawTextEx(
            OG.defaultFont, 
            v->header ? v->header : v->title, 
            (Vector2){ v->pos.x+OG_VIEWPORT_OUTLINE_T, v->pos.y+OG_VIEWPORT_OUTLINE_T }, 
            (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H), 
            1, 
            OG_TEXT_C );
    }

    if (v->RenderOnScreen)
        v->RenderOnScreen(v, pos);

}

static void DrawCommandBar(){
    if (OG.state != OG_STATE_ON_COMMAND_BAR) return;
    int screenWidth = GetScreenWidth();
    DrawRectangle(0, 0, screenWidth, OG_VIEWPORT_TITLE_H, OG_CMD_BAR_C);
    if (OG.selectedHint == 0)
        DrawRectangle(0, 0, MeasureTextEx(OG.defaultFont, OG.cmdBuf, OG.defaultFontSize, 2).x, OG_VIEWPORT_TITLE_H, OG_CMD_BAR_VIEWPORT_C);
    DrawTextEx(OG.defaultFont, OG.cmdBuf, (Vector2){0,0}, OG.defaultFontSize, 2, OG_TEXT_C);
    
    int hintsOffset = screenWidth/2;
    for (int i=1; i<OG.hintsQ; i++){
        int hintWidth = MeasureTextEx(OG.defaultFont,OG.hintsBuf[i],OG.defaultFontSize,2).x; 
        if (i == OG.selectedHint)
            DrawRectangle(hintsOffset, 0, hintWidth, OG_VIEWPORT_TITLE_H, OG_CMD_BAR_VIEWPORT_C);
        DrawTextEx(OG.defaultFont, OG.hintsBuf[i], (Vector2){hintsOffset,0}, OG.defaultFontSize, 2, OG_TEXT_C);
        hintsOffset += hintWidth + 20;
    }
}

static void DrawLogs(){
    int logsYOffset = 0;
    for (int i=OG.logsQ-1; i>=0; i--){
        logsYOffset += OG.defaultFontSize + 2;/* line spacing */
        Vector2 logPosition = (Vector2){0,GetScreenHeight()-logsYOffset};
        DrawTextEx(OG.defaultFont, OG.logs[i], logPosition, OG.defaultFontSize, 2, OG_LOGS_C);
    }
}

/* <== State Machine ===========================================> */

static void IdleState(){
    Vector2 mousePosition = GetMousePosition();
    if (OG.viewportJustSwitched && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        OG.viewportJustSwitched = false;
    }

    OG_ChangeCursor(NULL, MOUSE_CURSOR_DEFAULT);

    // SHOW VIEWPORTS
    for (int i=0; i<5; i++){
        OG_Viewport *v = OG.viewportsToShow[i];
        if (v != NULL){
            if (v->hidden) OG_ToggleViewport(v);
            else OG_SetViewportOnTop(v);
            OG.viewportsToShow[i] = NULL;
            return;
        }
    }

    if (IsKeyDown(OG_MODKEY)){
        
        // COMMAND BAR TOGGLE
        if (IsKeyPressed(OG_COMMAND_BAR_KEY)){
            memset(OG.cmdBuf, '\0', 256);
            OG.cmdCursor = 0;
            OG.selectedHint = 0;
            OG.state = OG_STATE_ON_COMMAND_BAR;
            return;
        }

    }
    
    // VIEWPORTS FOCUS
    if (!OG_MouseInViewport(OG.viewports.tail,false,false,false)){
        for (OG_Viewport *v = OG.viewports.tail->prev; v != NULL; v = v->prev){ // for each viewport except the last-one
            if (v->hidden || !OG_MouseInViewport(v, false, false,false)) continue;
            if (IsViewportFullyVisible(v) && 
                !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && 
                !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && 
                !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){
                    OG_SetViewportOnTop(v);
                    break;
            }
            
            else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                OG.viewportJustSwitched = true;
                OG_SetViewportOnTop(v);
                break;
            }
        }
    }

    if (OG.viewports.tail->hidden) return;
    OG_Viewport *v = OG.viewports.tail;

    // VIEWPORT MOVEMENT
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        if (OG_MouseInViewport(v, true, false, false)){
            OG.state = OG_STATE_MOVING_VIEWPORT;
            OG.targetViewport = v;
            OG.grabOffset = (Vector2){mousePosition.x-OG.targetViewport->pos.x,mousePosition.y-OG.targetViewport->pos.y};
            return;
        }
    }

    // LAYOUT RESIZE
    OG_Layout *l = v->layout;
    if (v->container) l = v->container->layout;
    if (l){
        if (l->viewport->resizable){
            Rectangle r = { 
                l->viewport->pos.x + l->size.x - OG_VIEWPORT_CORNER_S, 
                l->viewport->pos.y + l->size.y - OG_VIEWPORT_CORNER_S + (l->viewport->noTitleBar ? 0:OG_VIEWPORT_TITLE_H),
                OG_VIEWPORT_CORNER_S, 
                OG_VIEWPORT_CORNER_S 
            };

            if (IsPointOnRect(mousePosition, r)){
                #if __linux__
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_ALL);
                #else
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_NWSE);
                #endif    

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    OG.targetViewport = l->viewport;
                    OG.state = OG_STATE_RESIZING_LAYOUT;
                    return;
                }

            }
        }
    }

    // LAYOUT LINE RESIZE
    if (l){
        for (int i=0; i<l->containersQ; i++){
            OG_LayoutContainer *container = &l->containers[i];
            if (!IsPointOnRect(mousePosition, container->r)) continue;
            OG.targetViewport = l->viewport;

            if (!container->lines[2]->fixed){
                Rectangle leftHandle = {
                    container->r.x, container->r.y,
                    OG_VIEWPORT_PANEL_HANDLE_S,
                    container->r.height
                };

                if (IsPointOnRect(mousePosition, leftHandle)){
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_EW);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        OG.targetContainer = container;
                        OG.targetLine = container->lines[OG_LAYOUT_LINE_LEFT];
                        OG.state = OG_STATE_RESIZING_LAYOUT_LINE;
                        return;
                    }
                    goto out;
                }
            }

            if (!container->lines[3]->fixed){
                Rectangle rightHandle = {
                    container->r.x + container->r.width - OG_VIEWPORT_PANEL_HANDLE_S, 
                    container->r.y, OG_VIEWPORT_PANEL_HANDLE_S,
                    container->r.height
                };
                
                if (IsPointOnRect(mousePosition, rightHandle)){
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_EW);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        OG.targetContainer = container;
                        OG.targetLine = container->lines[OG_LAYOUT_LINE_RIGHT];
                        OG.state = OG_STATE_RESIZING_LAYOUT_LINE;
                        return;
                    }
                    goto out;
                };
            }

            if (!container->lines[0]->fixed){
                Rectangle topHandle = {
                    container->r.x, container->r.y,
                    container->r.width,
                    OG_VIEWPORT_PANEL_HANDLE_S
                };

                if (IsPointOnRect(mousePosition, topHandle)){
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_NS);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        OG.targetContainer = container;
                        OG.targetLine = container->lines[OG_LAYOUT_LINE_TOP];
                        OG.state = OG_STATE_RESIZING_LAYOUT_LINE;
                        return;
                    }
                    goto out;
                };
            }

            if (!container->lines[1]->fixed){
                Rectangle bottomHandle = {
                    container->r.x,
                    container->r.y + container->r.height - OG_VIEWPORT_PANEL_HANDLE_S,
                    container->r.width,
                    OG_VIEWPORT_PANEL_HANDLE_S
                };

                if (IsPointOnRect(mousePosition, bottomHandle)){
                    OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_NS);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        OG.targetContainer = container;
                        OG.targetLine = container->lines[OG_LAYOUT_LINE_BOTTOM];
                        OG.state = OG_STATE_RESIZING_LAYOUT_LINE;
                        return;
                    }
                };
            }

            out:{}
        }
    }

    // VIEWPORT RESIZE
    if (v->resizable && OG_MouseInViewport(v, false, true, false)){
        
        #if __linux__
            OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_ALL);
        #else
            OG_ChangeCursor(NULL, MOUSE_CURSOR_RESIZE_NWSE);
        #endif
        
    
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            OG.state = OG_STATE_RESIZING_VIEWPORT;
            OG.targetViewport = v;
            OG.grabOffset = (Vector2){
                mousePosition.x-OG.targetViewport->pos.x,
                mousePosition.y-OG.targetViewport->pos.y };
            return;
        }
    }

    // VIEWPORT EXIT
    if (IsKeyPressed(KEY_ESCAPE)){
        if (OG_MouseInViewport(v, false, false, false)){
            if (v->container)
                OG_ToggleViewport(v->container->layout->viewport);
            else OG_ToggleViewport(v);
            return;
        }
    }

    // VIEWPORT UPDATE
    if (OG_MouseInViewport(v, false, false, true) && v->Update != NULL && !v->updateAlways )
        v->Update(v);
    
}

static void MovingViewportState(){
    Vector2 mousePosition = GetMousePosition();
    OG.targetViewport->pos = (Vector2){mousePosition.x-OG.grabOffset.x,mousePosition.y-OG.grabOffset.y};
    
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        OG.state = OG_STATE_IDLE;
        return;
    }
}

static void ResizingLayoutState(){
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        Vector2 mousePosition = GetMousePosition();
        OG_Viewport *v = OG.targetViewport;
        float w = mousePosition.x - v->pos.x;
        float h = mousePosition.y - v->pos.y - (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H);
        
        v->size.width = w;
        v->layout->size.x = w;
        v->layout->size.y = h;

        OG.state = OG_STATE_IDLE;
        return;
    }
}

static void ResizingLayoutLineState(){
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        Vector2 p = GetMousePosition();
        OG_LayoutContainer *c = OG.targetContainer;
        OG_LayoutLine *l = OG.targetLine;

        float t;
        switch (l->type){
            case OG_LAYOUT_LINE_V:{ 
                t = Clamp(
                    (p.x - l->layout->viewport->pos.x) / l->layout->size.x, 
                    GetLineMinLimit(c, l, MIN_LAYOUT_SIZE), 
                    GetLineMaxLimit(c, l, MIN_LAYOUT_SIZE)
                );
            } break;

            case OG_LAYOUT_LINE_H:{
                t = Clamp(
                    (p.y - l->layout->viewport->pos.y - (l->layout->viewport->noTitleBar?0:OG_VIEWPORT_TITLE_H) ) / l->layout->size.y, 
                    GetLineMinLimit(c, l, MIN_LAYOUT_SIZE), 
                    GetLineMaxLimit(c, l, MIN_LAYOUT_SIZE)
                );
            } break;
        }

        l->t = t;
        OG.state = OG_STATE_IDLE;
        return;
    }
}

static void ResizingViewportState(){
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        Vector2 mousePosition = GetMousePosition();
        OG_Viewport *v = OG.targetViewport;
        float w = mousePosition.x - v->pos.x;
        float h = mousePosition.y - v->pos.y - (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H);
        
        OG_ResizeViewport(v, w, h);
        if (v->OnResize != NULL)
            v->OnResize(v);
        OG.state = OG_STATE_IDLE;
        return;
    }

}

static void OnCommandBarState(){
    // DELETE CHARACTERS
    if (OG.cmdBuf[0] != '\0' && IsKeyPressed(KEY_BACKSPACE)){
        OG.cmdBuf[OG.cmdCursor-1] = '\0';
        OG.cmdCursor--;
    }

    // CANCEL COMMAND
    if (IsKeyPressed(KEY_ESCAPE)){
        OG.state = OG_STATE_IDLE;
        return;
    }

    // ACCEPT COMMAND
    if (IsKeyPressed(KEY_ENTER)){
        OG.state = OG_STATE_IDLE;
        if (strcmp(OG.hintsBuf[OG.selectedHint], "fps") == 0){
            OG.drawFps = !OG.drawFps;
            return;
        }
        
        for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
            if (strcmp(OG.hintsBuf[OG.selectedHint],v->title) == 0){
                if (v->hidden){
                    OG_ToggleViewport(v);
                }

                else {
                    OG_SetViewportOnTop(v);
                }

                return;
            }
        }
        
        OG_Viewport *top = OG.viewports.tail;
        if (!top->hidden){
            char *argv[32];
            int argc = 0;
            while (true){
                argv[argc] = strtok(argc > 0 ? NULL:OG.hintsBuf[OG.selectedHint]," ");
                if (argv[argc] == NULL) break;
                argc++;
            }
            if (top->ExecCmd)
                top->ExecCmd(top,argc,argv);
        }
        
        return;
    }

    // AUTOCOMPLETE COMMAND (TAB)
    if (IsKeyPressed(KEY_TAB) && OG.selectedHint > 0){
        memset(OG.cmdBuf,'\0', sizeof(char)*256);
        strcpy(OG.cmdBuf,OG.hintsBuf[OG.selectedHint]);
        OG.selectedHint = 0;
        OG.cmdCursor = strlen(OG.cmdBuf);
    }

    // ADD CHARACTERS
    char pressedChar;
    while (true) {
        pressedChar = GetCharPressed();
        if (pressedChar == 0) break;
        OG.cmdBuf[OG.cmdCursor++] = pressedChar;
    }

    // HINTS
    memset(OG.hintsBuf+1, '\0', 256);
    OG.hintsQ = 1;
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (!v->hideCmd && strstr(v->title, OG.cmdBuf) != NULL)
            OG.hintsBuf[OG.hintsQ++] = v->title;
    }

    if (!OG.viewports.tail->hidden){ //focus window hints
        OG_Viewport *v = OG.viewports.tail;
        static const char *emptyCmds[] = { NULL };
        const char **vCommands = v->GetCmds ? v->GetCmds():emptyCmds;
        int o=0;
        while (true){
            if (vCommands[o] == NULL) break;
            if (strstr(vCommands[o], OG.cmdBuf) != NULL)
                OG.hintsBuf[OG.hintsQ++] = vCommands[o];
            o++;
        }
    }

    if (IsKeyPressed(KEY_RIGHT)) OG.selectedHint++;
    if (IsKeyPressed(KEY_LEFT)) OG.selectedHint--;
    if (OG.selectedHint >= OG.hintsQ) OG.selectedHint = OG.hintsQ-1;
    if (OG.selectedHint < 0) OG.selectedHint = 0;
}

/* <== Public API ==============================================> */

OG_Layout *OG_InitLayout(char *name, Rectangle r){ 
    OG_Viewport *v = OG_InitViewport(
        name, (Rectangle){r.x, r.y, r.width, 0},
        1.0f, 1.0f, 
        false, true, false, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL, 
        NULL
    );

    OG_Layout *l = calloc(1, sizeof(OG_Layout));
    l->size = (Vector2){r.width, r.height};
    v->layout = l;
    l->viewport = v;

    // UPPER
    l->lines[0].layout = l;
    l->lines[0].type = OG_LAYOUT_LINE_H;
    l->lines[0].t = 0;
    l->lines[0].fixed = true;

    // BOTTOM
    l->lines[1].layout = l;
    l->lines[1].type = OG_LAYOUT_LINE_H;
    l->lines[1].t = 1;
    l->lines[1].fixed = true;

    // LEFT
    l->lines[2].layout = l;
    l->lines[2].type = OG_LAYOUT_LINE_V;
    l->lines[2].t = 0;
    l->lines[2].fixed = true;

    // RIGHT
    l->lines[3].layout = l;
    l->lines[3].type = OG_LAYOUT_LINE_V;
    l->lines[3].t = 1;
    l->lines[3].fixed = true;

    l->linesQ = 4;
    
    return l;
}

OG_LayoutLine *OG_LayoutAddLine(OG_Layout *l, OG_LayoutLineType type, float t){
    OG_LayoutLine *line = &l->lines[l->linesQ++];
    line->fixed = false;
    line->type = type;
    line->t = t;
    line->layout = l;
    return line;
}

OG_LayoutContainer *OG_LayoutAddContainer(OG_Layout *layout, OG_LayoutLine *top, OG_LayoutLine *bottom, OG_LayoutLine *left, OG_LayoutLine *right){
    OG_LayoutContainer *container = &layout->containers[layout->containersQ++];
    container->layout = layout;
    container->lines[OG_LAYOUT_LINE_TOP] = top;
    container->lines[OG_LAYOUT_LINE_BOTTOM] = bottom;
    container->lines[OG_LAYOUT_LINE_LEFT] = left;
    container->lines[OG_LAYOUT_LINE_RIGHT] = right;
    return container;
}

void OG_LayoutAddViewport(OG_LayoutContainer *c, OG_Viewport *v){
    v->hideCmd = true;
    v->noTitleBar = true;
    c->v = v;
    v->container = c;
    v->resizable = false;
    v->isModal = false;
}

Vector2 OG_ResizeViewport(OG_Viewport *v, int w, int h){
    if (w < 0) w = v->size.width;
    else if (w < OG_VIEWPORT_MIN_W) w = OG_VIEWPORT_MIN_W;

    if (h < 0) h = v->size.height;
    else if (h < OG_VIEWPORT_MIN_H) h = OG_VIEWPORT_MIN_H;
    
    if (w >= OG_VIEWPORT_MIN_W) v->size.width = w;
    if (h >= OG_VIEWPORT_MIN_H) v->size.height = h*-1;
    
    if (!v->disableOffsetUpdate){
        v->camera.offset = (Vector2){
            v->size.width/2,
            (v->size.height*-1)/2
        };
    }

    UnloadRenderTexture(v->renderTexture);
    v->renderTexture = LoadCustomRenderTexture(v->size.width, v->size.height*-1);
    
    return (Vector2){v->size.width, v->size.height};
}

void OG_SetViewportOnTop(OG_Viewport *v){
    static bool zOrderWorkaround = false;
    if (!zOrderWorkaround){
        zOrderWorkaround = true;    
        OG_LayoutContainer *c = NULL;
        if (!OG.viewports.tail->hidden && OG.viewports.tail->container) 
            c = OG.viewports.tail->container;
        if (c && (!v->container || v->container->layout != c->layout))
            OG_SetViewportOnTop(OG.viewports.tail->container->layout->viewport);
        zOrderWorkaround = false;
    }
    
    if (OG.modalViewport && OG.modalViewport != v){
        if (!v->container || v->container->layout->viewport != OG.modalViewport){
            return;
        }
    }
    
    static bool nestedCalls = false;
    OG.targetViewport = v;
    if (v == OG.viewports.tail) return;
    
    if (v->layout){
        for (int i=0; i<v->layout->containersQ; i++){
            if (!v->layout->containers[i].v) continue;
            OG_SetViewportOnTop(v->layout->containers[i].v);
        }
    }

    if (v->container && !nestedCalls){
        nestedCalls = true;
        OG_SetViewportOnTop(v->container->layout->viewport);
        nestedCalls = false;
    }
    
    if (v->prev != NULL) v->prev->next = v->next;
    else OG.viewports.head = v->next;
        
    v->next->prev = v->prev;
    OG.viewports.tail->next = v;
    v->prev = OG.viewports.tail;
    v->next = NULL;
    OG.viewports.tail = v;
    CleanViewportUIInput(v);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        OG.viewportJustSwitched = true;
}

// this function sucks, I can do it better
bool OG_MouseInViewport(OG_Viewport* v, bool titleBar, bool resizeHandle, bool onlyViewport){
    Vector2 mousePosition = GetMousePosition();
    
    if (titleBar){
        Rectangle title = (Rectangle){
            v->pos.x, 
            v->pos.y, 
            v->size.width, 
            (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)
        };
        
        if (IsPointOnRect(mousePosition, title)) return true;
        return false;
    }
    
    if (resizeHandle){
        Rectangle corner = (Rectangle){
            v->pos.x+v->size.width-OG_VIEWPORT_CORNER_S,
            v->pos.y+(v->size.height*-1)+(v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)-OG_VIEWPORT_CORNER_S,
            OG_VIEWPORT_CORNER_S,OG_VIEWPORT_CORNER_S
        };
        if (IsPointOnRect(mousePosition, corner)) return true;
        return false;
    }

    if (onlyViewport){
        Rectangle onlyViewportArea = (Rectangle){
            v->pos.x,
            v->pos.y+(v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H),
            v->size.width,
            (v->size.height*-1)
        };
        if (IsPointOnRect(mousePosition, onlyViewportArea)) return true;
        return false;
    }

    Rectangle viewportArea = GetViewportRect(v);
    if (IsPointOnRect(mousePosition, viewportArea)) return true;
    return false;
}

float OG_GetMouseWheelMove(OG_Viewport *v){
    if (OG.viewports.tail != v) return 0.0f;
    if (!OG_MouseInViewport(v, false, false, true)) return 0.0f;
    return GetMouseWheelMove();
}

Vector2 OG_GetMouseOverlayPosition(OG_Viewport* v){
    Vector2 mousePosition = GetMousePosition();
    return (Vector2){
        mousePosition.x - (v->pos.x + OG_VIEWPORT_OUTLINE_T),
        mousePosition.y - (v->pos.y + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H))
    };
}

Vector2 OG_GetMouseViewportPosition(OG_Viewport* v){
    Vector2 mousePosition = GetMousePosition();
    return (Vector2){
        (mousePosition.x - v->pos.x - v->camera.offset.x)/v->camera.zoom + v->camera.target.x,
        (mousePosition.y - (v->pos.y+(v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H)) - v->camera.offset.y)/v->camera.zoom + v->camera.target.y
    };
}

bool OG_IsKeyPressed(OG_Viewport *v, int key){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsKeyPressed(key);
}

bool OG_IsKeyReleased(OG_Viewport *v, int key){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsKeyReleased(key);
}

bool OG_IsKeyDown(OG_Viewport *v, int key){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsKeyDown(key);
}

bool OG_IsKeyUp(OG_Viewport *v, int key){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsKeyUp(key);
}

bool OG_IsMouseButtonPressed(OG_Viewport *v, int button){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsMouseButtonPressed(button);
}

bool OG_IsMouseButtonReleased(OG_Viewport *v, int button){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsMouseButtonReleased(button);
}

bool OG_IsMouseButtonDown(OG_Viewport *v, int button){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsMouseButtonDown(button);
}

bool OG_IsMouseButtonUp(OG_Viewport *v, int button){
    if (OG.viewportJustSwitched) return false;
    if (OG.state != OG_STATE_IDLE) return false;
    if (v != OG.viewports.tail) return false;
    if (!OG_MouseInViewport(v, false, false, true))
        return false;
    return IsMouseButtonUp(button);
}

void OG_ViewportUpdateZoom(OG_Viewport* v){
    if (OG.viewports.tail != v) return;
    if (!OG_MouseInViewport(v, false, false, true)) return;
    float wheel = GetMouseWheelMove();
    float factor = 1.0f + wheel * OG_ZOOM_SPEED;
    v->camera.zoom *= factor;
    if (v->camera.zoom < v->minZoom) v->camera.zoom = v->minZoom;
    if (v->camera.zoom > v->maxZoom) v->camera.zoom = v->maxZoom;
}

int OG_ViewportUpdatePan(OG_Viewport* v){
    if (OG.viewports.tail != v) return 1;
    if (!OG_MouseInViewport(v, false, false, true)) return 1;
    Vector2 d = GetMouseDelta();
    v->camera.target = (Vector2){v->camera.target.x - (d.x / v->camera.zoom),v->camera.target.y - (d.y / v->camera.zoom)};
    return 0;
}

OG_Viewport *OG_GetViewportByName(char *name){
    if (!name) return NULL;

    OG_Viewport *v = OG.viewports.head;
    while (v != NULL) {
        if (v->title && strcmp(v->title, name) == 0)
            return v;
        v = v->next;
    }

    return NULL;
}

void OG_ToggleViewport(OG_Viewport *v){
    if (OG.modalViewport && OG.modalViewport != v){
        if (!v->container || v->container->layout->viewport != OG.modalViewport){
            OG_PushLog("Can't toggle '%s': there is a modal viewport already open", v->title);
            return;
        }
    }
        
    v->hidden = !v->hidden;
    if (!v->hidden){
        OG_ResizeViewport(v, -1, -1);
        OG_SetViewportOnTop(v);
        if (v->isModal)
            OG.modalViewport = v;
    }

    else {
        if (OG.modalViewport){
            OG.modalViewport = NULL;
        }
            
        if (v == OG.viewports.tail){
            while (true){
                if (v == NULL) return;
                if (!v->hidden){
                    OG_SetViewportOnTop(v);
                    return;
                }
                
                v = v->prev;    
            }
        }
    }
}

void OG_ToggleViewportByName(char *name){
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            OG_ToggleViewport(v);
            return;
        }
    }
}

void OG_OpenViewportByName(char *name){
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            for (int i=0; i<5; i++){
                if (OG.viewportsToShow[i] == NULL){
                    OG.viewportsToShow[i] = v;
                    return;
                }
            }
        }
    }
}

void OG_CloseViewportByName(char *name){
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            if (!v->hidden) OG_ToggleViewport(v);
            return;
        }
    }
}

void OG_ChangeCursor(OG_Viewport *v, MouseCursor c){
    if (v != NULL && v != OG.viewports.tail) return;
    
    if (v != NULL){
        if (!OG_MouseInViewport(v, false, false, true))
            return;
    }
    
    OG.nextCursor = c;
}

void OG_UnsetHeader(OG_Viewport *v){
    if (v->header) 
        free(v->header);
    v->header = NULL;
}

void OG_SetHeader(OG_Viewport *v, char *header){
    OG_UnsetHeader(v);
    if (!header) return;
    v->header = calloc(strlen(header)+1, sizeof(char));
    strcpy(v->header, header);
}

OG_Viewport *OG_InitViewport(char* title, 
                    Rectangle rect,
                    float minZoom, float maxZoom,
                    
                    // Flags
                    bool hideCmd,
                    bool resizable,
                    bool isModal,

                    // Main Callbacks
                    void (*Init)(OG_Viewport*), 
                    void (*Update)(OG_Viewport*), 
                    void (*OnResize)(OG_Viewport*),
                    
                    // Render callbacks
                    void (*Render)(OG_Viewport*), 
                    void (*RenderOverlay)(OG_Viewport*),
                    void (*RenderUnderlay)(OG_Viewport*),
                    void (*RenderOnScreen)(OG_Viewport*, Vector2),

                    // UI Callback
                    void (*UI)(struct OG_Viewport*, mu_Context*),
                    
                    // Command callbacks
                    const char** (GetCmds)(),
                    void (*ExecCmd)(OG_Viewport*, int argc, char **argv)
                    ){
    OG_Viewport *v = calloc(1, sizeof(OG_Viewport));
    
    // Meta
    v->title = calloc(strlen(title)+1, sizeof(char));
    strcpy(v->title, title);
    
    v->size = (Rectangle){0,0,rect.width,rect.height};
    v->pos = (Vector2){rect.x,rect.y};
    v->minZoom = minZoom;
    v->maxZoom = maxZoom;
    v->hidden = true;

    // Camera
    v->camera.target = (Vector2){0,0};
    v->camera.offset = (Vector2){rect.width/2,rect.height/2};
    v->camera.rotation = 0.0f;
    v->camera.zoom = 1.0f;
    v->renderTexture = LoadCustomRenderTexture(rect.width,rect.height);

    // UI Init
    mu_init(&v->ctx);    
    v->ctx.text_width = text_width;
    v->ctx.text_height = text_height;
    v->ctx.style->title_height = OG_VIEWPORT_TITLE_H;
    v->ctx.style->font = (void*) &(OG.defaultFont);
    v->ctx.style->colors[MU_COLOR_WINDOWBG]  = *(mu_Color*) &OG_VIEWPORT_BG_C;
    v->ctx.style->colors[MU_COLOR_TITLEBG]   = *(mu_Color*) &OG_VIEWPORT_TITLE_C;
    v->ctx.style->colors[MU_COLOR_TITLETEXT] = *(mu_Color*) &OG_TEXT_C;
    v->ctx.style->colors[MU_COLOR_BORDER]    = *(mu_Color*) &OG_VIEWPORT_OUTLINE_C;
    v->ctx.style->colors[MU_COLOR_SCROLLTHUMB] = (mu_Color){35, 35, 35, 255};
    v->UI = UI;

    // Flags
    v->hideCmd = hideCmd;
    v->resizable = resizable;
    v->isModal = isModal;
    

    // Main callbacks
    v->Init = Init;
    v->Update = Update;
    v->OnResize = OnResize;


    // Render callbacks
    v->Render = Render;
    v->RenderOverlay = RenderOverlay;
    v->RenderUnderlay = RenderUnderlay;
    v->RenderOnScreen = RenderOnScreen;

    // Command callbacks
    v->GetCmds = GetCmds;
    v->ExecCmd = ExecCmd;

    // LINK THE LIST
    if (OG.viewports.tail == NULL){
        OG.viewports.head = v;
    }

    else{
        OG.viewports.tail->next = v;
        v->prev = OG.viewports.tail;
    }

    OG.viewports.tail = v;
    return v;
}

int OG_Init(char* title, int fps){
    OG_FileDialog();
    OG_InputDialog();
    OG_ContextMenu();
    
    //Raylib init
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(500, 500, title); // width and height cannot be 0 because of wasm
    MaximizeWindow();
    SetExitKey(0);
    SetTargetFPS(fps);

    const char *chars =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    " !\"#$%&'()*+,-./:;<=>∧⋀⌃?@[\\]^_`{|}~θ°"
    "│─├└┘┌┐"
    "⬅⮌←⟵↑";

    int count = 0;
    int *codepoints = LoadCodepoints(chars, &count);

    OG.defaultFont = LoadFontFromMemory(
        ".ttf",
        IBMPlexMono_Medium_ttf,
        IBMPlexMono_Medium_ttf_len,
        32,
        codepoints,
        count
    );

    UnloadCodepoints(codepoints);
    OG.defaultFontSize = OG_DEFAULT_FONT_SIZE;

    Image iconsImage = LoadImageFromMemory(".png", icons_png, icons_png_len);
    OG.icons = LoadTextureFromImage(iconsImage);
    UnloadImage(iconsImage);

    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (v->Init != NULL)
            v->Init(v);
    }

    OG.hintsBuf[0] = OG.cmdBuf;

    return 0;
}

bool OG_UpdateFrame(){
    switch (OG.state){
        case OG_STATE_IDLE:                 IdleState();               break;
        case OG_STATE_MOVING_VIEWPORT:      MovingViewportState();     break;
        case OG_STATE_RESIZING_LAYOUT:      ResizingLayoutState();     break;
        case OG_STATE_RESIZING_LAYOUT_LINE: ResizingLayoutLineState(); break;
        case OG_STATE_RESIZING_VIEWPORT:    ResizingViewportState();   break;
        case OG_STATE_ON_COMMAND_BAR:       OnCommandBarState();       break;
    }

    // EXECUTE THE 'ALWAYS' LOGIC
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (v->layout){
            CalcLayout(v);
            ApplyLayout(v->layout);
        }
        if (v->updateAlways){
            if (v->Update) v->Update(v);
        }
    }

    for (OG_Viewport *v=OG.viewports.head; v != NULL; v = v->next){
        if (v->hidden) continue;
        UpdateViewportUIInput(v);
        if (v->container && (OG.viewports.tail == v->container->layout->viewport || OG.viewports.tail->prev == v->container->layout->viewport)){
            for (int i=0; i<3; i++) // weird workaround
                ProcessViewportUI(v);
        }
        else ProcessViewportUI(v);
    }

    // EXECUTE LOGS LOGIC
    if (OG.logsQ > 0){
        OG.logsTimer += GetFrameTime();
        if (OG.logsTimer >= 4 /*seconds*/){
            PopLog();
        }
    }
    
    // SET CURSOR PER FRAME
    if (OG.nextCursor != OG.currentCursor){
        OG.currentCursor = OG.nextCursor;
        SetMouseCursor(OG.currentCursor);
    }

    return WindowShouldClose();
}

bool OG_RenderFrame(){
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (v == OG.viewports.tail || v->renderAlways || v->needsRedraw){
            if (v->layout){
                for (int i=0; i<v->layout->containersQ; i++){
                    if (v->layout->containers[i].v) {
                        RenderViewport(v->layout->containers[i].v);
                        v->layout->containers[i].v->needsRedraw = false;
                    }   
                }
                    
                RenderViewport(v);
                v->needsRedraw = false;
            }

            else if (v->container){
                for (int i=0; i<v->container->layout->containersQ; i++){
                    if (v->container->layout->containers[i].v){
                        RenderViewport(v->container->layout->containers[i].v);
                        v->container->layout->containers[i].v->needsRedraw = false;
                    }
                }
            }
            
            else {
                RenderViewport(v);
                v->needsRedraw = false;
            }
            
        }
    }
    
    BeginDrawing();
    ClearBackground(OG_BG_C);
    
    if (OG.RenderBackground)
        OG.RenderBackground();
    
    // FOR EACH VIEWPORT
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (v->hidden) continue;
        DrawViewport(v);
    }

    // DRAW RESIZE LAYOUT GUIDELINE
    if (OG.state == OG_STATE_RESIZING_LAYOUT){
        OG_Viewport *v = OG.targetViewport;
        Vector2 mousePos = GetMousePosition();
        Rectangle rect = (Rectangle){
            v->pos.x,
            v->pos.y,
            mousePos.x - v->pos.x,
            mousePos.y - v->pos.y
        };

        DrawRectangleLinesEx(
            rect,
            OG_VIEWPORT_OUTLINE_T,
            InvertColor(OG_BG_C)
        );
    }

    // DRAW RESIZE LAYOUT LINE GUIDELINE
    if (OG.state == OG_STATE_RESIZING_LAYOUT_LINE){
        Vector2 p = GetMousePosition();
        OG_LayoutContainer *c = OG.targetContainer;
        OG_LayoutLine *l = OG.targetLine;
        OG_Layout *layout = c->layout;
        OG_Viewport *v = layout->viewport;

        switch (l->type){
            case OG_LAYOUT_LINE_V:{ 
                float t = Clamp(
                    (p.x - l->layout->viewport->pos.x) / l->layout->size.x, 
                    GetLineMinLimit(c, l, MIN_LAYOUT_SIZE), 
                    GetLineMaxLimit(c, l, MIN_LAYOUT_SIZE)
                );

                DrawLineEx(
                    (Vector2){Lerp(v->pos.x, v->pos.x + layout->size.x, t), c->r.y},
                    (Vector2){Lerp(v->pos.x, v->pos.x + layout->size.x, t), c->r.y + c->r.height},
                    1.0f, 
                    InvertColor(OG_BG_C)
                );

            } break;

            case OG_LAYOUT_LINE_H:{
                float t = Clamp(
                    (p.y - l->layout->viewport->pos.y - (l->layout->viewport->noTitleBar?0:OG_VIEWPORT_TITLE_H) ) / l->layout->size.y, 
                    GetLineMinLimit(c, l, MIN_LAYOUT_SIZE), 
                    GetLineMaxLimit(c, l, MIN_LAYOUT_SIZE)
                );

                int tOffset = (v->noTitleBar?0:OG_VIEWPORT_TITLE_H);
                DrawLineEx(
                    (Vector2){c->r.x, Lerp( v->pos.y + tOffset, v->pos.y + layout->size.y + tOffset, t)},
                    (Vector2){c->r.x + c->r.width, Lerp( v->pos.y + tOffset, v->pos.y + layout->size.y + tOffset, t)}, 
                    1.0f, 
                    InvertColor(OG_BG_C)
                );

            } break;
        }
    }

    // DRAW RESIZE VIEWPORT GUIDELINE
    if (OG.state == OG_STATE_RESIZING_VIEWPORT){
        OG_Viewport *v = OG.targetViewport;
        Vector2 mousePos = GetMousePosition();
        Rectangle rect = (Rectangle){
            v->pos.x,
            v->pos.y,
            mousePos.x - v->pos.x,
            mousePos.y - v->pos.y
        };

        float minW = OG_VIEWPORT_MIN_W;
        if (rect.width < minW) rect.width = minW;
            
        float minH = OG_VIEWPORT_MIN_H + (v->noTitleBar ? 0:OG_VIEWPORT_TITLE_H);
        if (rect.height < minH) rect.height = minH;
        
        DrawRectangleLinesEx(
            rect,
            OG_VIEWPORT_OUTLINE_T,
            InvertColor(OG_BG_C)
        );
    }

    DrawCommandBar();
    DrawLogs();
    if (OG.drawFps) 
        DrawFPS(10,10);
    EndDrawing();

    return WindowShouldClose();
}

bool OG_Update(){
    OG_UpdateFrame();
    OG_RenderFrame();
    return WindowShouldClose();
}

void OG_Free(){
    CloseWindow();

    // Free resources
    UnloadFont(OG.defaultFont);
    UnloadTexture(OG.icons);
    
    // Free viewports resources
    for (OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        free(v->title);
        OG_UnsetHeader(v);
        if (v->header) free(v->header);
        UnloadRenderTexture(v->renderTexture);
        if (v->layout) free(v->layout);
        
    }

    // Free logs
    for (int i = 0; i < OG.logsQ; ++i) {
        if (OG.logs[i]) free(OG.logs[i]);
    }
    OG.logsQ = 0;

    // Free viewports linked list
    for(OG_Viewport *v = OG.viewports.head; v != NULL; v = v->next){
        if (v->prev != NULL)
            free(v->prev);
    }
    
    free(OG.viewports.tail);
    OG.viewports.head = NULL;
    OG.viewports.tail = NULL;
}