#include <raylib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "origami.h"

static const char *commands[] = {
    NULL
};

/* <== Utils ===================================================> */

void DrawGrid2D(int w, int h, int gridSize, Color c){
    int maxX = (w/2 / gridSize + 1) * gridSize;
    int maxY = (h/2 / gridSize + 1) * gridSize;

    for (int x = -maxX; x <= maxX; x += gridSize)
        DrawLine(x, -maxY, x, maxY, c);
    
    for (int y = -maxY; y <= maxY; y += gridSize)
        DrawLine(-maxX, y, maxX, y, c);
}

/* <== Callbacks ===============================================> */


/* <== Register Function =======================================> */

void Window0(){
    OG_Layout *layout = OG_InitLayout(
        "window0", 
        (Rectangle){0,0,600,300}
    );

    // Lines
    OG_LayoutLine *vL = OG_LayoutAddLine(layout, OG_LAYOUT_LINE_V, 0.5f);
    OG_LayoutLine *hL = OG_LayoutAddLine(layout, OG_LAYOUT_LINE_H, 0.5f);

    // Containers
    OG_LayoutContainer *leftContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        &layout->lines[OG_LAYOUT_LINE_LEFT], 
        vL
    );

    OG_LayoutContainer *topRightContainer = OG_LayoutAddContainer(
        layout, 
        &layout->lines[OG_LAYOUT_LINE_TOP], 
        hL, 
        vL, 
        &layout->lines[OG_LAYOUT_LINE_RIGHT]
    );

    OG_LayoutContainer *bottomRightContainer = OG_LayoutAddContainer(
        layout, 
        hL, 
        &layout->lines[OG_LAYOUT_LINE_BOTTOM], 
        vL, 
        &layout->lines[OG_LAYOUT_LINE_RIGHT]
    );

    // Viewports
    OG_LayoutAddViewport(
        leftContainer, 
        OG_InitViewport(
            "window0_leftViewport", 
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
            NULL, 
            NULL, 
            NULL
        )
    );

    OG_LayoutAddViewport(
        topRightContainer, 
        OG_InitViewport(
            "window0_topRightViewport", 
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
            NULL, 
            NULL, 
            NULL
        )
    );

    OG_LayoutAddViewport(
        bottomRightContainer, 
        OG_InitViewport(
            "window0_bottomRightContainer", 
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
            NULL, 
            NULL, 
            NULL
        )
    );
       
}