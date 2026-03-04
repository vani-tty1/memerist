#include "meme-renderer.h"
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>

void meme_get_image_coordinates (GtkWidget *widget, GdkPixbuf *img, double wx, double wy, double *ix, double *iy) {
  double ww, wh, iw, ih, scale, draw_w, draw_h, off_x, off_y;
  double w_ratio, h_ratio;

  if (!img) { *ix = 0; *iy = 0; return; }

  ww = gtk_widget_get_width (widget);
  wh = gtk_widget_get_height (widget);

  if (ww <= 0 || wh <= 0) { *ix = 0; *iy = 0; return; }

  iw = gdk_pixbuf_get_width (img);
  ih = gdk_pixbuf_get_height (img);

  w_ratio = ww / iw;
  h_ratio = wh / ih;
  scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  draw_w = iw * scale;
  draw_h = ih * scale;
  off_x = (ww - draw_w) / 2.0;
  off_y = (wh - draw_h) / 2.0;

  *ix = (wx - off_x) / draw_w;
  *iy = (wy - off_y) / draw_h;
}

ResizeHandle
meme_get_crop_handle_at_position (double x, double y, double cx, double cy, double cw, double ch) {
    double handle_radius = 0.05; 
    
    if (fabs(x - cx) < handle_radius && fabs(y - cy) < handle_radius) return HANDLE_TOP_LEFT;
    if (fabs(x - (cx + cw)) < handle_radius && fabs(y - cy) < handle_radius) return HANDLE_TOP_RIGHT;
    if (fabs(x - cx) < handle_radius && fabs(y - (cy + ch)) < handle_radius) return HANDLE_BOTTOM_LEFT;
    if (fabs(x - (cx + cw)) < handle_radius && fabs(y - (cy + ch)) < handle_radius) return HANDLE_BOTTOM_RIGHT;

    if (fabs(y - cy) < handle_radius && x > cx && x < cx + cw) return HANDLE_TOP;
    if (fabs(y - (cy + ch)) < handle_radius && x > cx && x < cx + cw) return HANDLE_BOTTOM;
    if (fabs(x - cx) < handle_radius && y > cy && y < cy + ch) return HANDLE_LEFT;
    if (fabs(x - (cx + cw)) < handle_radius && y > cy && y < cy + ch) return HANDLE_RIGHT;

    if (x > cx && x < cx + cw && y > cy && y < cy + ch) return HANDLE_CENTER;

    return HANDLE_NONE;
}

GdkPixbuf * meme_apply_saturation_contrast (GdkPixbuf *src, double sat, double contrast) {
  GdkPixbuf *copy;
  int w, h, stride, n_channels, x, y;
  guchar *pixels, *row_ptr, *p;
  double r, g, b, gray, one_minus_sat = 1.0 - sat;

  if (!src) return NULL;
  copy = gdk_pixbuf_copy (src);
  w = gdk_pixbuf_get_width (copy);
  h = gdk_pixbuf_get_height (copy);
  stride = gdk_pixbuf_get_rowstride (copy);
  n_channels = gdk_pixbuf_get_n_channels (copy);
  pixels = gdk_pixbuf_get_pixels (copy);

  for (y = 0; y < h; y++) {
    row_ptr = pixels + (y * stride);
    for (x = 0; x < w; x++) {
      p = row_ptr + (x * n_channels);
      r = (double)p[0]; g = (double)p[1]; b = (double)p[2];

      gray = 0.299 * r + 0.587 * g + 0.114 * b;
      r = gray * one_minus_sat + r * sat;
      g = gray * one_minus_sat + g * sat;
      b = gray * one_minus_sat + b * sat;

      r = (r - 128.0) * contrast + 128.0;
      g = (g - 128.0) * contrast + 128.0;
      b = (b - 128.0) * contrast + 128.0;

      p[0] = CLAMP_U8((int)r); p[1] = CLAMP_U8((int)g); p[2] = CLAMP_U8((int)b);
    }
  }
  return copy;
}

GdkPixbuf * meme_apply_deep_fry (GdkPixbuf *src) {
  GdkPixbuf *fried = gdk_pixbuf_copy (src);
  int w = gdk_pixbuf_get_width (fried);
  int h = gdk_pixbuf_get_height (fried);
  int nc = gdk_pixbuf_get_n_channels (fried);
  int rs = gdk_pixbuf_get_rowstride (fried);
  GdkPixbuf *shrunk, *final;
  guchar *pixels, *row, *p;
  double contrast = 2.0;
  int noise_level = 30;
  int x, y, i, noise, val;
  double c_val;

  pixels = gdk_pixbuf_get_pixels (fried);

  for (y = 0; y < h; y++) {
    row = pixels + y * rs;
    for (x = 0; x < w; x++) {
      p = row + x * nc;
      for (i = 0; i < 3; i++) {
        noise = (rand() % (noise_level * 2 + 1)) - noise_level;
        val = p[i] + noise;
        c_val = ((double)val - 128.0) * contrast + 128.0;
        p[i] = CLAMP_U8 ((int)c_val);
      }
    }
  }

  shrunk = gdk_pixbuf_scale_simple (fried, MAX (w * 0.25, 1), MAX (h * 0.25, 1), GDK_INTERP_NEAREST);
  final = gdk_pixbuf_scale_simple (shrunk, w, h, GDK_INTERP_NEAREST);
  g_object_unref (fried);
  g_object_unref (shrunk);
  return final;
}

GdkPixbuf * meme_render_composite (GdkPixbuf *bg, GList *layers, gboolean cinematic, gboolean deep_fry) {
  if (!bg) return NULL;
  int w = gdk_pixbuf_get_width (bg);
  int h = gdk_pixbuf_get_height (bg);

  cairo_surface_t *surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create (surf);

  gdk_cairo_set_source_pixbuf (cr, bg, 0.0, 0.0);
  cairo_paint (cr);

  GList *l;
  for (l = layers; l != NULL; l = l->next) {
    ImageLayer *layer = (ImageLayer *)l->data;
    double draw_x = layer->x * w;
    double draw_y = layer->y * h;

    cairo_save (cr);
    cairo_translate (cr, draw_x, draw_y);
    cairo_rotate (cr, layer->rotation);
    cairo_scale (cr, layer->scale, layer->scale);

    if (layer->blend_mode == BLEND_MULTIPLY) cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);
    else if (layer->blend_mode == BLEND_SCREEN) cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
    else if (layer->blend_mode == BLEND_OVERLAY) cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
    else cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf) {
       gdk_cairo_set_source_pixbuf (cr, layer->pixbuf, -layer->width/2.0, -layer->height/2.0);
       if (layer->opacity < 1.0) cairo_paint_with_alpha (cr, layer->opacity);
       else cairo_paint (cr);
    }
    else if (layer->type == LAYER_TYPE_TEXT && layer->text) {
        // everything under this automatically converts any font 
        // into a path so we can still use our stroke and fill
        PangoLayout *layout = pango_cairo_create_layout (cr);
        pango_layout_set_text (layout, layer->text, -1);
    
        PangoFontDescription *desc;
        if (layer->font_family) {
            desc = pango_font_description_from_string (layer->font_family);
        } else {
            desc = pango_font_description_from_string ("Sans Bold");
        }
        
        pango_font_description_set_absolute_size (desc, layer->font_size * PANGO_SCALE);
        pango_layout_set_font_description (layout, desc);
        pango_font_description_free (desc);
    
        int tw, th;
        pango_layout_get_pixel_size (layout, &tw, &th);
    
        layer->width = tw + 10;
        layer->height = th + 10;
    
        cairo_move_to (cr, -tw / 2.0, -th / 2.0);
        pango_cairo_layout_path (cr, layout);
    
        cairo_set_source_rgba (cr, 0, 0, 0, layer->opacity);
        cairo_set_line_width (cr, layer->font_size * 0.08);
        cairo_stroke_preserve (cr);
    
        cairo_set_source_rgba (cr, 1, 1, 1, layer->opacity);
        cairo_fill (cr);
    
        g_object_unref (layout);
    }
    cairo_restore (cr);
  }

  cairo_surface_flush (surf);
  cairo_destroy (cr);

  GdkPixbuf *comp = gdk_pixbuf_get_from_surface (surf, 0, 0, w, h);
  cairo_surface_destroy (surf);

  if (cinematic) {
      GdkPixbuf *tmp = meme_apply_saturation_contrast(comp, 1.15, 1.05);
      if (tmp) { g_object_unref(comp); comp = tmp; }
  }
  if (deep_fry) {
      GdkPixbuf *tmp = meme_apply_deep_fry(comp);
      if (tmp) { g_object_unref(comp); comp = tmp; }
  }
  return comp;
}

GdkTexture * meme_render_editor_overlay (GdkPixbuf *composite, GList *layers, ImageLayer *selected, gboolean crop_active, double cx, double cy, double cw, double ch) {
  if (!composite) return NULL;
  int w = gdk_pixbuf_get_width (composite);
  int h = gdk_pixbuf_get_height (composite);

  cairo_surface_t *surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create (surf);

  gdk_cairo_set_source_pixbuf(cr, composite, 0, 0);
  cairo_paint(cr);

  if (crop_active) {
     double abs_x = cx * w;
     double abs_y = cy * h;
     double abs_w = cw * w;
     double abs_h = ch * h;

     cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
     if (abs_y > 0) { cairo_rectangle(cr, 0, 0, w, abs_y); cairo_fill(cr); }
     if (abs_y + abs_h < h) { cairo_rectangle(cr, 0, abs_y + abs_h, w, h - (abs_y + abs_h)); cairo_fill(cr); }
     if (abs_x > 0) { cairo_rectangle(cr, 0, abs_y, abs_x, abs_h); cairo_fill(cr); }
     if (abs_x + abs_w < w) { cairo_rectangle(cr, abs_x + abs_w, abs_y, w - (abs_x + abs_w), abs_h); cairo_fill(cr); }

     
     cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
     cairo_set_line_width(cr, 2.0);
     cairo_rectangle(cr, abs_x, abs_y, abs_w, abs_h);
     cairo_stroke(cr);

     
     cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
     cairo_set_line_width(cr, 1.0);
     cairo_move_to(cr, abs_x + abs_w/3.0, abs_y); cairo_line_to(cr, abs_x + abs_w/3.0, abs_y + abs_h);
     cairo_move_to(cr, abs_x + 2*abs_w/3.0, abs_y); cairo_line_to(cr, abs_x + 2*abs_w/3.0, abs_y + abs_h);
     cairo_move_to(cr, abs_x, abs_y + abs_h/3.0); cairo_line_to(cr, abs_x + abs_w, abs_y + abs_h/3.0);
     cairo_move_to(cr, abs_x, abs_y + 2*abs_h/3.0); cairo_line_to(cr, abs_x + abs_w, abs_y + 2*abs_h/3.0);
     cairo_stroke(cr);

     
     double hr = 5.0;
     cairo_set_source_rgba(cr, 1, 1, 1, 1);
     cairo_arc(cr, abs_x, abs_y, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x + abs_w, abs_y, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x, abs_y + abs_h, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x + abs_w, abs_y + abs_h, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x + abs_w/2.0, abs_y, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x + abs_w/2.0, abs_y + abs_h, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x, abs_y + abs_h/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
     cairo_arc(cr, abs_x + abs_w, abs_y + abs_h/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
  }
  else if (selected) {
      double sx = selected->x * w;
      double sy = selected->y * h;
      double bw = selected->width * selected->scale;
      double bh = selected->height * selected->scale;
      //radius of circles on the visual handler
      double hr = 6.0; 
  
      cairo_save(cr);
      cairo_translate(cr, sx, sy);
      cairo_rotate(cr, selected->rotation);
      
      cairo_set_source_rgba(cr, 0.4, 0.2, 0.8, 0.8);
      cairo_set_line_width(cr, 2.0);
      cairo_rectangle(cr, -bw/2.0, -bh/2.0, bw, bh);
      cairo_stroke(cr);
  
      cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); 
      
      cairo_arc(cr, -bw/2.0, -bh/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
      cairo_arc(cr, bw/2.0, -bh/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
      cairo_arc(cr, -bw/2.0, bh/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
      cairo_arc(cr, bw/2.0, bh/2.0, hr, 0, 2*M_PI); cairo_fill(cr);
      cairo_restore(cr);
}

  cairo_destroy(cr);
  GdkPixbuf *res = gdk_pixbuf_get_from_surface(surf, 0, 0, w, h);
  cairo_surface_destroy(surf);

  GdkTexture *tex = gdk_texture_new_for_pixbuf(res);
  g_object_unref(res);
  return tex;
}