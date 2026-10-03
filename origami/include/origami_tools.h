#ifndef ORIGAMI_FD_H
#define ORIGAMI_FD_H

#include <stdbool.h>

typedef enum {
    OG_FD_MODE_SELECT_FILE,
    OG_FD_MODE_SELECT_DIR
} OG_FileDialogMode;

void OG_OpenFileDialog(bool (*ok_callback)(char*, void*), void *okparam, void (*close_callback)(bool ok), OG_FileDialogMode mode, char *ext, char *msg);
void OG_OpenInputDialog(void (*ok_callback)(char*), char *hint);
void OG_OpenContextMenu(void (*callback)(int, void*), void *param, char *hint, int optQ, ...);
void OG_OpenContextMenuV2(void (*_callback)(int, void*), void *param, char *hint, int optQ, char **opts);
void OG_OpenContextMenuV3(void (*_callback)(char*, void*), void *param, char *hint, int optQ, char **opts);

#endif