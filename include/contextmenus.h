// Copyright 2026 Esteban Huertas
// SPDX-License-Identifier: Apache-2.0

#ifndef CONTEXTMENUS_H
#define CONTEXTMENUS_H

#include "titleblock.h"

void OpenAchorContextMenu();
void OpenAnnotationContextMenu();
void OpenSurfaceContextMenu(void *param);
void OpenTreeListContextMenu();
void OpenSurfaceTreeContextMenu();
void OpenPageListContextMenu();
void OpenEntityContextMenu();
void OpenTitleBlockRectContextMenu(TitleBlockRect *r);
void OpenTitleBlockContextMenu();
void OpenTitleBlockEditorContextMenu();

#endif