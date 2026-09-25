#pragma once
#include "meme-core.h"

void meme_get_image_coordinates (GtkWidget *widget, GdkPixbuf *img, double wx, double wy, double *ix, double *iy);
ResizeHandle meme_get_crop_handle_at_position(double x, double y, double cx, double cy, double cw, double ch, double rx, double ry);

GdkPixbuf *meme_apply_saturation_contrast (GdkPixbuf *src, double sat, double contrast);
GdkPixbuf *meme_apply_deep_fry (GdkPixbuf *src);


GdkPixbuf *meme_render_composite(GdkPixbuf *bg, GList *layers, gboolean cinematic, gboolean deep_fry, gboolean bw, gboolean fast_mode);

GdkTexture *meme_render_editor_overlay (GdkPixbuf *composite, 
                                        GList *layers, 
                                        ImageLayer *selected_layer,
                                        gboolean crop_active,
                                        double cx, double cy, double cw, double ch);


void meme_draw_crop_chrome (cairo_t *cr, double w, double h,
                             double abs_x, double abs_y, double abs_w, double abs_h);


void meme_draw_alignment_guides (cairo_t *cr, double off_x, double off_y,
                                  double draw_w, double draw_h,
                                  gboolean v_active, double v_x,
                                  gboolean h_active, double h_y);

GdkPixbuf *meme_bake_stroke_pixbuf (GArray *points, int img_w, int img_h,
                                     double line_width, const GdkRGBA *color,
                                     double *out_cx, double *out_cy,
                                     double *out_w, double *out_h);

// Draws the in-progress stroke directly onto the editor overlay while
// the user is still dragging, before it's been baked into a layer.
void meme_draw_stroke_preview (cairo_t *cr, GArray *points,
                                double img_w, double img_h, double scale,
                                double off_x, double off_y,
                                double line_width, const GdkRGBA *color);
