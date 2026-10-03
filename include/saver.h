#ifndef SAVER_H
#define SAVER_H

#include <stdbool.h>

#define ASSEMBLIES_VERSION 0
#define TITLEBLOCKS_VERSION 0

bool SaveTitleBlock(char *path, void *param);
bool SavePanelAssembly(char *path, void *param);
bool SaveMainDoc(char *path, void *param);
bool SaveCutouts(char *path, void *param);
bool SaveSurfaceSVG(char *path, void *param);

#endif