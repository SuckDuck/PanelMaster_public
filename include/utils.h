#ifndef UTILS_H
#define UTILS_H

#include <raylib.h>
#include "origami.h"

typedef struct LinePoints{
    Vector2 A, B;
} LinePoints;

typedef struct RectCorners{
    Vector2 points[5];
} RectCorners;

Vector2 Vector2RotateDegrees(Vector2 v, float angle);
RectCorners GetRectCorners(Rectangle rect, Vector2 center, float scale, float rotation);
Rectangle RectFromCorners(RectCorners rc);
void DrawRecFromCenter(Rectangle rec, float lineThick, Color color);
void DrawViewportGrid(OG_Viewport *v, float spacing, Color color, bool only00);
void DrawRectCorners(RectCorners rectCorners, Color color, int thickness);
void DrawLineDashedEx(Vector2 A, Vector2 B, float thickness, float dashSize, float gapSize, Color color);
void DrawDashedRect(Rectangle r, float thickness, float dashSize, float gapSize, Color color);
void DrawHatchPoly(Vector2 *p, int pQ, float space, Color c);
Vector2 BlenderToRaylib(Vector2 v);
void DrawTextureFromScreenSpace(Texture2D *texture, Vector2 *points, int pointCount, Color tint);
float NormalizeAngle(float angle);
bool IsBlank(const char *str);
Color ColorWithAlpha(Color c, int alpha);
bool IsHorizontal(float rotation);
bool IsVertical(float rotation);
int mu_number_custom(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt);
void mu_space(mu_Context *ctx);
void mu_vertical_space(mu_Context *ctx, int height);
void mu_tabs(mu_Context *ctx, int tab_count, const char **labels, int *state, int opt);
void mu_equal_layout_l(mu_Context *ctx, int q, const int *labels_widths);
void mu_equal_layout_l_auto(mu_Context *ctx, const char **labels, int height);
void mu_infinite_input(mu_Context *ctx, char *id, char **str);

#endif