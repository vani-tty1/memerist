#pragma once
#include "meme-core.h"

void meme_get_image_coordinates (GtkWidget *widget, GdkPixbuf *img, double wx, double wy, double *ix, double *iy);
ResizeHandle meme_get_crop_handle_at_position(double x, double y, double cx, double cy, double cw, double ch, double rx, double ry);

GdkPixbuf *meme_apply_saturation_contrast (GdkPixbuf *src, double sat, double contrast);
GdkPixbuf *meme_apply_deep_fry (GdkPixbuf *src);


GdkPixbuf *meme_render_composite (GdkPixbuf *bg, GList *layers, gboolean cinematic, gboolean deep_fry);

GdkTexture *meme_render_editor_overlay (GdkPixbuf *composite, 
                                        GList *layers, 
                                        ImageLayer *selected_layer,
                                        gboolean crop_active,
                                        double cx, double cy, double cw, double ch);