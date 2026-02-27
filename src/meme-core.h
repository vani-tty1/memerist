#pragma once
#include <gtk/gtk.h>
#define CLAMP_U8(val) ((val) < 0 ? 0 : ((val) > 255 ? 255 : (val)))



typedef enum {
  DRAG_TYPE_NONE,
  DRAG_TYPE_IMAGE_MOVE,
  DRAG_TYPE_IMAGE_RESIZE,
  DRAG_TYPE_CROP_MOVE,
  DRAG_TYPE_CROP_RESIZE
} DragType;

typedef enum {
  HANDLE_NONE,
  HANDLE_TOP_LEFT,
  HANDLE_TOP,
  HANDLE_TOP_RIGHT,
  HANDLE_RIGHT,
  HANDLE_BOTTOM_RIGHT,
  HANDLE_BOTTOM,
  HANDLE_BOTTOM_LEFT,
  HANDLE_LEFT,
  HANDLE_CENTER
} ResizeHandle;

typedef enum {
  BLEND_NORMAL,
  BLEND_MULTIPLY,
  BLEND_SCREEN,
  BLEND_OVERLAY
} BlendMode;

typedef enum {
  LAYER_TYPE_IMAGE,
  LAYER_TYPE_TEXT
} LayerType;

typedef struct {
  LayerType type;
  GdkPixbuf *pixbuf;
  char *text;
  double font_size;
  char *font_family;
  double x;
  double y;
  double width;
  double height;
  double scale;
  double rotation;
  double opacity;
  BlendMode blend_mode;
} ImageLayer;


ImageLayer *meme_layer_copy (const ImageLayer *src);
void meme_layer_free (gpointer data);
GList *meme_layer_list_copy (GList *src);
void meme_layer_list_free (GList *list);