// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "caddraw.h"
#include "microui.h"
#include "origami.h"

void DrawRecFromCenter(Rectangle rec, float lineThick, Color color){
	DrawRectangleLinesEx(
		(Rectangle){
			rec.x - rec.width/2,
			rec.y - rec.height/2,
			rec.width,
			rec.height
		}, 
		lineThick, 
		color
	);
}

void DrawViewportGrid(OG_Viewport *v, float spacing, Color color, bool only00){
	if (spacing <= 0) return;

	spacing *= v->camera.zoom;
	Vector2 offset = (Vector2){
		v->camera.target.x*v->camera.zoom,
		v->camera.target.y*v->camera.zoom
	};

	Vector2 homeLines = (Vector2){
		v->size.width/2 - offset.x,
		(v->size.height*-1)/2 - offset.y,
	};

	if (!only00){
	
		while (offset.x > spacing)  offset.x = offset.x - spacing;
		while (offset.x < -spacing) offset.x = offset.x + spacing;
		while (offset.y > spacing)  offset.y = offset.y - spacing;
		while (offset.y < -spacing) offset.y = offset.y + spacing;
			
		// vertical lines - right
		float lineStart = v->size.width/2 - offset.x;
		while (lineStart < v->size.width){
			if (!FloatEquals(lineStart, homeLines.x)){
				DrawLineEx(
					(Vector2){lineStart, 0}, 
					(Vector2){lineStart, v->size.height*-1}, 
					1, color
				);
			}

			lineStart += spacing;
		}

		// vertical lines - left
		lineStart = v->size.width/2 - offset.x;
		bool firstLine = true;
		while (lineStart > 0){
			if (!firstLine && !FloatEquals(lineStart, homeLines.x)){
				DrawLineEx(
					(Vector2){lineStart, 0}, 
					(Vector2){lineStart, v->size.height*-1}, 
					1, color
				);
			}

			lineStart -= spacing;
			firstLine = false;
		}

		// horizontal lines - bottom
		lineStart = (v->size.height*-1)/2 - offset.y;
		while (lineStart < v->size.height*-1){
			if (!FloatEquals(lineStart, homeLines.y)){
				DrawLineEx(
					(Vector2){0, lineStart}, 
					(Vector2){v->size.width, lineStart}, 
					1, color
				);
			}

			lineStart += spacing;
		}

		// horizontal lines - top
		lineStart = (v->size.height*-1)/2 - offset.y;
		firstLine = true;
		while (lineStart > 0){
			if (!firstLine && !FloatEquals(lineStart, homeLines.y)){
				DrawLineEx(
					(Vector2){0, lineStart}, 
					(Vector2){v->size.width, lineStart}, 
					1, color
				);
			}

			lineStart -= spacing;
			firstLine = false;
		}
	}

	
	// Draw (0,0) lines
	DrawLineEx(
		(Vector2){homeLines.x, 0}, 
		(Vector2){homeLines.x, v->size.height*-1}, 
		1, (Color){0,255*color.g,0, 255}
	);

	DrawLineEx(
		(Vector2){0, homeLines.y}, 
		(Vector2){v->size.width, homeLines.y}, 
		1, (Color){255*color.r,0,0,255}
	);
}

Vector2 Vector2RotateDegrees(Vector2 v, float angle){
	float rad = angle * (M_PI / 180.0f);
	float cosA = cos(rad);
	float sinA = sin(rad);
	return (Vector2){
		v.x * cosA - v.y * sinA,
		v.x * sinA + v.y * cosA
	};
}

RectCorners GetRectCorners(Rectangle rect, Vector2 center, float scale, float rotation){
	RectCorners corners;
	
	corners.points[0] = Vector2RotateDegrees((Vector2){
		center.x*scale*-1,
		center.y*scale*-1
	},rotation);
	corners.points[0] = Vector2Add((Vector2){rect.x,rect.y}, corners.points[0]);

	corners.points[1] = Vector2RotateDegrees((Vector2){
		(rect.width-center.x)*scale,
		center.y*scale*-1
	},rotation);
	corners.points[1] = Vector2Add((Vector2){rect.x,rect.y}, corners.points[1]);

	corners.points[2] = Vector2RotateDegrees((Vector2){
		(rect.width-center.x)*scale,
		(rect.height-center.y)*scale
	},rotation);
	corners.points[2] = Vector2Add((Vector2){rect.x,rect.y}, corners.points[2]);

	corners.points[3] = Vector2RotateDegrees((Vector2){
		center.x*scale*-1,
		(rect.height-center.y)*scale
	},rotation);
	corners.points[3] = Vector2Add((Vector2){rect.x,rect.y}, corners.points[3]);
	corners.points[4] = corners.points[0];
	return corners;
}

Rectangle RectFromCorners(RectCorners rc){
	float minX = rc.points[0].x;
	float maxX = rc.points[0].x;
	float minY = rc.points[0].y;
	float maxY = rc.points[0].y;

	for (int i = 1; i < 4; i++) {
		if (rc.points[i].x < minX) minX = rc.points[i].x;
		if (rc.points[i].x > maxX) maxX = rc.points[i].x;
		if (rc.points[i].y < minY) minY = rc.points[i].y;
		if (rc.points[i].y > maxY) maxY = rc.points[i].y;
	}

	Rectangle rect = {
		minX,
		minY,
		maxX - minX,
		maxY - minY
	};

	return rect;
}

void DrawRectCorners(RectCorners rectCorners, Color color, int thickness){
	DrawLineStrip(rectCorners.points, 5, color);
}

void DrawLineDashedEx(Vector2 A, Vector2 B, float thickness, float dashSize, float gapSize, Color color){
	Vector2 delta = { B.x - A.x, B.y - A.y };
	float length = sqrtf(delta.x*delta.x + delta.y*delta.y);
	if (length <= 0.0f) return;

	Vector2 dir = { delta.x/length, delta.y/length };
	float progress = 0.0f;

	while (progress < length)
	{
		Vector2 dashStart = {
			A.x + dir.x * progress,
			A.y + dir.y * progress
		};

		float dashEndDist = progress + dashSize;
		if (dashEndDist > length) dashEndDist = length;

		Vector2 dashEnd = {
			A.x + dir.x * dashEndDist,
			A.y + dir.y * dashEndDist
		};

		DrawLineEx(dashStart, dashEnd, thickness, color);

		progress += dashSize + gapSize;
	}
}

void DrawDashedRect(Rectangle r, float thickness, float dashSize, float gapSize, Color color){
	Vector2 tl = { r.x, r.y };
	Vector2 tr = { r.x + r.width, r.y };
	Vector2 br = { r.x + r.width, r.y + r.height };
	Vector2 bl = { r.x, r.y + r.height };

	DrawLineDashedEx(tl, tr, thickness, dashSize, gapSize, color);
	DrawLineDashedEx(tr, br, thickness, dashSize, gapSize, color);
	DrawLineDashedEx(br, bl, thickness, dashSize, gapSize, color);
	DrawLineDashedEx(bl, tl, thickness, dashSize, gapSize, color);   
}

static void DrawHatch(Rectangle rect, float spacing, Color c){
    if (spacing <= 0)
		return;
	float h = rect.height;
    float w = rect.width;

    for (float d = -h; d < w; d += spacing){
        Vector2 p1 = { rect.x + d, rect.y + h };
        Vector2 p2 = { rect.x + d + h, rect.y };
        DrawCadLine(p1, p2, 1.0f, c);
    }
}

void DrawHatchPoly(Vector2 *p, int pQ, float space, Color c){
	if (pQ <= 0) return;

	// Get bounding box
	Rectangle polyRect = {0};
	
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

	SetCadMask(p, pQ);
	if (space > 0)
		DrawHatch(polyRect, space, c);
	UnsetCadMask();

	OpenCadPath();
	for (int i=0; i<pQ-1; i++)
        DrawCadLine(p[i], p[i+1], 1.0f, c);
    DrawCadLine(p[pQ-1], p[0], 1.0f, c);
	CloseCadPath();
}

Vector2 BlenderToRaylib(Vector2 v){
	return (Vector2){
		v.x,
		-v.y
	};
}

void DrawTextureFromScreenSpace(Texture2D *texture, Vector2 *points, int pointCount, Color tint){
	if (!texture) return;
	if (pointCount < 3) return;
	rlSetTexture(texture->id);
	rlBegin(RL_TRIANGLES);
	
	// not really, but it works as
	Vector2 center = Vector2Lerp(
		Vector2Lerp(points[0], points[1], 0.5f), 
		Vector2Lerp(points[1], points[2], 0.5f), 
		0.5f
	);

	rlColor4ub(tint.r, tint.g, tint.b, tint.a);
	Vector2 centerUV = {
		center.x / texture->width,
		(center.y / texture->height)*-1
	};

	for (int i = 0; i < pointCount - 1; i++) {
		float uvs[] = {
			points[i].x / texture->width,
			(points[i].y / texture->height)*-1,
			points[i+1].x / texture->width,
			(points[i+1].y / texture->height)*-1,
		};

		// CW
		rlTexCoord2f(centerUV.x, centerUV.y);
		rlVertex2f(center.x, center.y);

		rlTexCoord2f(uvs[0], uvs[1]);
		rlVertex2f(points[i].x, points[i].y);

		rlTexCoord2f(uvs[2], uvs[3]);
		rlVertex2f(points[i+1].x, points[i+1].y);

		// CCW
		rlTexCoord2f(centerUV.x, centerUV.y);
		rlVertex2f(center.x, center.y);

		rlTexCoord2f(uvs[2], uvs[3]);
		rlVertex2f(points[i+1].x, points[i+1].y);

		rlTexCoord2f(uvs[0], uvs[1]);
		rlVertex2f(points[i].x, points[i].y);

	}

	rlEnd();
	rlSetTexture(rlGetTextureIdDefault());
}

float NormalizeAngle(float angle){
	angle = fmodf(angle, 360.0f);
	if (angle < 0.0f)
		angle += 360.0f;
	return angle;
}

bool IsBlank(const char *str){
	if (str == NULL) return true;
	while (*str){
		if (!isspace((unsigned char)*str)){
			return false;
		}
		str++;
	}
	return true;
}

Color ColorWithAlpha(Color c, int alpha){
	return (Color){
		c.r, c.g, c.b,
		alpha
	};
}

bool IsHorizontal(float rotation){
	if (NormalizeAngle(rotation) == 0) return true;
	if (NormalizeAngle(rotation) == 180) return true;
	return false;
}

bool IsVertical(float rotation){
	return !IsHorizontal(rotation);
}

int mu_number_custom(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt){
	float _;
	int rs = mu_number_ex(ctx, value ? value:&_, step, value ? fmt:"n/a", opt | (value ? 0:MU_OPT_NOINTERACT));
	return rs;
}

void mu_space(mu_Context *ctx){
  mu_Rect r = mu_layout_next(ctx);
}

void mu_vertical_space(mu_Context *ctx, int height){
  mu_layout_row(ctx, 1, (int[]) { -1 }, height);
  mu_space(ctx);
}

static bool mu_tab(mu_Context *ctx, const char *label, bool state, int opt){
	int res = false;
	mu_Id id = mu_get_id(ctx, label, strlen(label));
	mu_Rect r = mu_layout_next(ctx);
	mu_update_control(ctx, id, r, opt);

	/* handle click */
	if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id)
		res = true;

	/* draw */
	if (!(opt & MU_OPT_NOFRAME)){
		int colorid = MU_COLOR_WINDOWBG;
		if (res || state || ctx->focus == id) colorid = MU_COLOR_BUTTON +1;
		else if (ctx->hover == id) colorid = MU_COLOR_BUTTON;
		ctx->draw_frame(ctx, r, colorid);
	}
	
	if (label) mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt);
	return res;
}

void mu_tabs(mu_Context *ctx, int tab_count, const char **labels, int *state, int opt){
	for (int i=0; i<tab_count; i++){
		if (mu_tab(ctx, labels[i], *state == i, opt))
			*state = i;
	}
}

void mu_equal_layout_l(mu_Context *ctx, int q, const int *labels_widths){
	mu_Container *cnt = mu_get_current_container(ctx);

	int spacing = ctx->style->spacing;
	int padding = ctx->style->padding;
	int inner_w = cnt->body.w - 2 * padding;
	int cols = q * 2;

	int labels_total_w = 0;
	for (int i = 0; i < q; i++) {
		labels_total_w += labels_widths[i];
	}

	int total_spacing = (cols - 1) * spacing;
	int remaining_w = inner_w - labels_total_w - total_spacing;

	if (remaining_w < 0)
		remaining_w = 0;

	int widget_w = remaining_w / q;
	int widths[64];

	for (int i = 0; i < q; i++) {
		widths[i * 2]     = labels_widths[i];
		widths[i * 2 + 1] = widget_w;
	}

	mu_layout_row(ctx, cols, widths, 0);
}

void mu_equal_layout_l_auto(mu_Context *ctx, const char **labels, int height){
	mu_Container *cnt = mu_get_current_container(ctx);

	int spacing = ctx->style->spacing;
	int padding = ctx->style->padding;

	int q = 0;
	while (labels[q]) q++;

	int cols = q * 2;

	int inner_w = cnt->body.w - 2 * padding;

	int labels_total_w = 0;
	int label_w[64];
	int space_w = ctx->text_width(ctx->style->font, " ", 1);
	for (int i = 0; i < q; i++) {
		int len = 0;
		while (labels[i][len]) len++;
		label_w[i] = ctx->text_width(ctx->style->font, labels[i], len) + space_w;
		labels_total_w += label_w[i];
	}

	int total_spacing = (cols - 1) * spacing;
	int remaining_w = inner_w - labels_total_w - total_spacing;

	if (remaining_w < 0) remaining_w = 0;

	int widget_w = remaining_w / q;

	int widths[64];

	for (int i = 0; i < q; i++) {
		widths[i * 2] = label_w[i];
		widths[i * 2 + 1] = widget_w;
	}

	mu_layout_row(ctx, cols, widths, height);
}


static mu_Vec2 mu_get_bufD(mu_Context *ctx, char *buf){
	mu_Font font = ctx->style->font;
	int textw = ctx->text_width(font, buf, -1);
	int texth = ctx->text_height(font);
	
	char *cursor = buf;
	float sizeY = texth;
	float sizeX = 0.0f;
	for (char *c = buf; *c != '\0'; c++ ){
		if (*c == '\n'){
			sizeY += texth;
			cursor = c+1;
		}

		float w = ctx->text_width(font, cursor, -1);
		if (sizeX < w) sizeX = w;
	}
  	
	return mu_vec2(sizeX, sizeY);
}

static void mu_draw_multiline_text(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Vec2 size, mu_Color color){
	mu_Command *cmd;
	mu_Rect rect = mu_rect(pos.x, pos.y, size.x, size.y);
	int clipped = mu_check_clip(ctx, rect);
	if (clipped == MU_CLIP_ALL ) { return; }
	if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
	/* add command */
	if (len < 0) { len = strlen(str); }
	cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
	memcpy(cmd->text.str, str, len);
	cmd->text.str[len] = '\0';
	cmd->text.pos = pos;
	cmd->text.color = color;
	cmd->text.font = font;
	/* reset clipping if it was set */
	if (clipped) { mu_set_clip(ctx, (mu_Rect){0, 0, 0x1000000, 0x1000000}); }
}

static int mu_big_textbox(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt){
	int res = 0;
	mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);

	if (ctx->focus == id) {
		/* handle text input */
		int len = strlen(buf);
		char *input_text = ctx->input_text;
		if (ctx->key_pressed & MU_KEY_RETURN){
			input_text = "\n";
		}
			
		int n = mu_min(bufsz - len - 1, (int) strlen(input_text));
		if (n > 0) {
			memcpy(buf + len, input_text, n);
			len += n;
			buf[len] = '\0';
			res |= MU_RES_CHANGE;
		}
		/* handle backspace */
		if (ctx->key_pressed & MU_KEY_BACKSPACE && len > 0) {
			/* skip utf-8 continuation bytes */
			while ((buf[--len] & 0xc0) == 0x80 && len > 0);
			buf[len] = '\0';
			res |= MU_RES_CHANGE;
		}
	}

	/* draw */
	mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
	mu_Font font = ctx->style->font;
	int textw = ctx->text_width(font, buf, -1);
	int texth = ctx->text_height(font);
	int textx = r.x;
	int texty = r.y;
	
	// cursor
	char *cursor = buf;
	float yOffset = 0.0f;
	float xOffset = 0.0f;
	float maxW = 0.0f;
	for (char *c = buf; *c != '\0'; c++ ){
		if (*c == '\n'){
			yOffset += texth;
			cursor = c+1;
		}

		int w = ctx->text_width(font, cursor, -1);
		if (w > maxW) maxW = w;
	}
  	
	xOffset = ctx->text_width(font, cursor, -1);

	mu_draw_control_frame(ctx, id, r, MU_COLOR_BASE, opt);  
	mu_set_clip(ctx, r);
	mu_draw_multiline_text(ctx, font, buf, -1, mu_vec2(textx, texty), mu_vec2(maxW, yOffset+texth), color);
	if (ctx->focus == id)
		mu_draw_rect(ctx, mu_rect(textx + xOffset, texty + yOffset, 1, texth), color);
	mu_set_clip(ctx, (mu_Rect){0, 0, 0x1000000, 0x1000000}); // clear clip
	
  	return res;
}

/* May be changed when Ctrl+V is implemented (if it is implemented)*/
/* Also a movable cursor could be implemented*/
void mu_infinite_input(mu_Context *ctx, char *id, char **str){

	mu_begin_panel(ctx, id);
	mu_Container *containter = mu_get_current_container(ctx);
	
	mu_Id input_id = mu_get_id(ctx, id, strlen(id));
	int colorid = MU_COLOR_BASE;
	colorid += (ctx->focus == input_id) ? 2 : (ctx->hover == input_id) ? 1 : 0;
  	mu_draw_rect(ctx, containter->rect, ctx->style->colors[colorid]);

	int textS = strlen(*str)+3;
	char textB[textS];
	strcpy(textB, *str);

	mu_Vec2 buffD = mu_get_bufD(ctx, textB);
	bool hScroll = buffD.x > (containter->rect.w - ctx->style->padding*2);
	mu_layout_row(
		ctx, 1, 
		(const int[]){hScroll ? buffD.x : -1}, 
		(buffD.y > (containter->rect.h - (hScroll ? ctx->style->scrollbar_size:0) - ctx->style->padding*2)) ? buffD.y : (containter->rect.h - (hScroll ? ctx->style->scrollbar_size:0) - ctx->style->padding*2)
	);

	if (mu_big_textbox(ctx, textB, textS, input_id, mu_layout_next(ctx), MU_OPT_NOFRAME) & MU_RES_CHANGE){
		char *tmp = realloc(*str, strlen(textB) + 1);
		if (tmp != NULL) {
			*str = tmp;
			strcpy(*str, textB);
		}
	}

	mu_end_panel(ctx);
}