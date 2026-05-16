#pragma once
#include "meme-window-private.h"

void on_load_image_clicked (MemeWindow *self);
void on_add_image_clicked (MemeWindow *self);
void on_export_clicked (MemeWindow *self);
void on_load_project_clicked (MemeWindow *self);

typedef struct {
    GFile    *file;
    GKeyFile *keyfile;
    int       pending;
} SaveCtx;

typedef struct {
    SaveCtx *save_ctx;
    gchar   *group;
    gchar   *key;
} EncodeCtx;
