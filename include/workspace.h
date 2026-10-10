// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <stdbool.h>

typedef struct InspectorSettings {
    int enableGrid;
    int enableAnchors;
    int enableAnnotations;
    int DrawSkins;
    int DrawCutOuts;
    float gridSpacing;
} InspectorSettings;

extern InspectorSettings inspectorSettings;

void WorkspaceStartDrawingExtensible();
void WorkspaceCancelDrawingExtensible();
bool WorkspaceIsDrawingExtensible();

#endif