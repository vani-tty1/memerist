#include "meme-renderer.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "glib.h"
#include "meme-core.h"
#include "pango/pango-layout.h"
#include "pango/pango-types.h"
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdio.h>

void meme_get_image_coordinates(GtkWidget *widget, GdkPixbuf *img, double wx, double wy, double *ix, double *iy) {
    double ww, wh, iw, ih, scale, draw_w, draw_h, off_x, off_y;
    double w_ratio, h_ratio;

    if (!img) {
        *ix = 0;
        *iy = 0;
        return;
    }

    ww = gtk_widget_get_width(widget);
    wh = gtk_widget_get_height(widget);

    if (ww <= 0 || wh <= 0) {
        *ix = 0;
        *iy = 0;
        return;
    }

    iw = gdk_pixbuf_get_width(img);
    ih = gdk_pixbuf_get_height(img);

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
meme_get_crop_handle_at_position(double x, double y, double cx, double cy, double cw, double ch, double rx, double ry) {
    if (fabs(x - cx) < rx && fabs(y - cy) < ry) return HANDLE_TOP_LEFT;
    if (fabs(x - (cx + cw)) < rx && fabs(y - cy) < ry) return HANDLE_TOP_RIGHT;
    if (fabs(x - cx) < rx && fabs(y - (cy + ch)) < ry) return HANDLE_BOTTOM_LEFT;
    if (fabs(x - (cx + cw)) < rx && fabs(y - (cy + ch)) < ry) return HANDLE_BOTTOM_RIGHT;

    if (fabs(y - cy) < ry && x > cx && x < cx + cw) return HANDLE_TOP;
    if (fabs(y - (cy + ch)) < ry && x > cx && x < cx + cw) return HANDLE_BOTTOM;
    if (fabs(x - cx) < rx && y > cy && y < cy + ch) return HANDLE_LEFT;
    if (fabs(x - (cx + cw)) < rx && y > cy && y < cy + ch) return HANDLE_RIGHT;

    if (x > cx && x < cx + cw && y > cy && y < cy + ch) return HANDLE_CENTER;

    return HANDLE_NONE;
}

GdkPixbuf *meme_apply_saturation_contrast(GdkPixbuf *src, double sat, double contrast) {
    GdkPixbuf *copy;
    int w, h, stride, n_channels, x, y;
    guchar *pixels, *row_ptr, *p;
    int sat_fixed;
    int inv_sat_fixed;
    guchar contrast_lut[256];


    if (!src)
        return NULL;
    copy = gdk_pixbuf_copy(src);
    w = gdk_pixbuf_get_width(copy);
    h = gdk_pixbuf_get_height(copy);
    stride = gdk_pixbuf_get_rowstride(copy);
    n_channels = gdk_pixbuf_get_n_channels(copy);
    pixels = gdk_pixbuf_get_pixels(copy);

    for (int i = 0; i < 256; i++) {
        double val = ((double)i - 128.0) * contrast + 128.0;
        contrast_lut[i] = CLAMP_U8((int)val);
    }

    sat_fixed = (int)(sat * 1024.0);
    inv_sat_fixed = (int)((1.0 - sat) * 1024.0);

    for (y = 0; y < h; y++) {
        row_ptr = pixels + (y * stride);
        for (x = 0; x < w; x++) {
            int r, g, b;
            int gray;
            int rs, gs, bs;
            p = row_ptr + (x * n_channels);

            r = p[0], g = p[1], b = p[2];
            gray = (r * 306 + g * 601 + b * 117) >> 10;

            rs = (gray * inv_sat_fixed + r * sat_fixed) >> 10;
            gs = (gray * inv_sat_fixed + g * sat_fixed) >> 10;
            bs = (gray * inv_sat_fixed + b * sat_fixed) >> 10;

            p[0] = contrast_lut[CLAMP_U8(rs)];
            p[1] = contrast_lut[CLAMP_U8(gs)];
            p[2] = contrast_lut[CLAMP_U8(bs)];
        }
    }
    return copy;
}

GdkPixbuf *meme_apply_deep_fry(GdkPixbuf *src) {
    GdkPixbuf *fried = gdk_pixbuf_copy(src);
    int w = gdk_pixbuf_get_width(fried);
    int h = gdk_pixbuf_get_height(fried);
    int nc = gdk_pixbuf_get_n_channels(fried);
    int rs = gdk_pixbuf_get_rowstride(fried);
    GdkPixbuf *shrunk, *final;
    guchar *pixels, *row, *p;
    double contrast = 2.0;
    int noise_level = 30;
    int x, y, i, noise, val;
    double c_val;

    pixels = gdk_pixbuf_get_pixels(fried);

    for (y = 0; y < h; y++) {
        row = pixels + y * rs;
        for (x = 0; x < w; x++) {
            p = row + x * nc;
            for (i = 0; i < 3; i++) {
                noise = (rand() % (noise_level * 2 + 1)) - noise_level;
                val = p[i] + noise;
                c_val = ((double)val - 128.0) * contrast + 128.0;
                p[i] = CLAMP_U8((int)c_val);
            }
        }
    }

    shrunk = gdk_pixbuf_scale_simple(fried, MAX(w * 0.25, 1), MAX(h * 0.25, 1), GDK_INTERP_NEAREST);
    final = gdk_pixbuf_scale_simple(shrunk, w, h, GDK_INTERP_NEAREST);
    g_object_unref(fried);
    g_object_unref(shrunk);
    return final;
}

static void meme_layer_ensure_text_pixbuf(ImageLayer *layer, int bg_width) {
    PangoLayout *layout;
    PangoLayout *layout2;
    cairo_surface_t *surf;
    cairo_t *cr;
    cairo_surface_t *surf_m;
    cairo_t *cr_m;
    int tw;
    int th;
    PangoRectangle ink_rect;
    PangoFontDescription *desc;
    double max_width;


    gboolean is_emoji;

    if ((layer->type != LAYER_TYPE_TEXT && layer->type != LAYER_TYPE_EMOJI) || !layer->text || layer->pixbuf) return;
    is_emoji = (layer->type == LAYER_TYPE_EMOJI);

    surf_m = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cr_m = cairo_create(surf_m);
    layout = pango_cairo_create_layout(cr_m);
    pango_layout_set_text(layout, layer->text, -1);
    max_width = (bg_width * 0.9) / layer->scale;
    pango_layout_set_width(layout, max_width * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);


    desc = is_emoji
                                     ? pango_font_description_from_string("Noto Color Emoji")
                                     : (layer->font_family
                                     ? pango_font_description_from_string(layer->font_family)
                                     : pango_font_description_from_string("Sans Bold"));
    pango_font_description_set_absolute_size(desc, layer->font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);

    pango_layout_get_pixel_extents(layout, &ink_rect, NULL);
    tw = ink_rect.width;
    th = ink_rect.height;
    layer->width = tw + 10;
    layer->height = th + 10;
    
    g_object_unref(layout);
    cairo_destroy(cr_m);
    cairo_surface_destroy(surf_m);

    surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, tw + 10, th + 10);
    cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    layout2 = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout2, layer->text, -1);
    pango_layout_set_width(layout2, max_width * PANGO_SCALE);
    pango_layout_set_wrap(layout2, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout2, PANGO_ALIGN_CENTER);
    pango_layout_set_font_description(layout2, desc);

    cairo_move_to(cr, 5.0 - ink_rect.x, 5.0 - ink_rect.y);

    if (is_emoji) {
        pango_cairo_show_layout(cr, layout2);
    } else {
        pango_cairo_layout_path(cr, layout2);

        cairo_set_source_rgba(cr, layer->stroke_color.red, layer->stroke_color.green, layer->stroke_color.blue, 1.0);
        cairo_set_line_width(cr, layer->font_size * 0.08);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, layer->text_color.red, layer->text_color.green, layer->text_color.blue, 1.0);
        cairo_fill(cr);
    }
    cairo_surface_flush(surf);

    layer->pixbuf = gdk_pixbuf_get_from_surface(surf, 0, 0, tw + 10, th + 10);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    g_object_unref(layout2);
    pango_font_description_free(desc);
}

GdkPixbuf *meme_render_composite(GdkPixbuf *bg, GList *layers,
                                gboolean cinematic,
                                gboolean deep_fry, gboolean bw,
                                gboolean fast_mode) {
    GdkPixbuf *comp;
    int render_w;
    int render_h;
    cairo_surface_t *surf;
    cairo_t *cr;
    double scale = 1.0;
    int orig_w;
    int orig_h;
    if (!bg) return NULL;


    orig_w = gdk_pixbuf_get_width(bg);
    orig_h  = gdk_pixbuf_get_height(bg);

    for (GList *l = layers; l != NULL; l = l->next) {
        meme_layer_ensure_text_pixbuf((ImageLayer *)l->data, orig_w);
    }


    if (fast_mode && orig_w > 800) {
        scale = 800.0 / (double)orig_w;
    }

    render_w = orig_w * scale;
    render_h = orig_h * scale;
    surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, render_w, render_h);
    cr = cairo_create(surf);

    if (scale < 1.0) {
        GdkPixbuf *scaled_bg = gdk_pixbuf_scale_simple(bg, render_w, render_h, GDK_INTERP_NEAREST);
        gdk_cairo_set_source_pixbuf(cr, scaled_bg, 0.0, 0.0);
        cairo_paint(cr);
        g_object_unref(scaled_bg);
    } else {
        gdk_cairo_set_source_pixbuf(cr, bg, 0.0, 0.0);
        cairo_paint(cr);
    }

    cairo_scale(cr, scale, scale);

    for (GList *l = layers; l != NULL; l = l->next) {
        ImageLayer *layer = (ImageLayer *)l->data;
        cairo_save(cr);

        cairo_translate(cr, layer->x * orig_w, layer->y * orig_h);
        cairo_rotate(cr, layer->rotation);
        cairo_scale(cr, layer->scale, layer->scale);

        if (layer->blend_mode == BLEND_MULTIPLY) cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);
        else if (layer->blend_mode == BLEND_SCREEN) cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
        else if (layer->blend_mode == BLEND_OVERLAY) cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
        else cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        if (layer->pixbuf) {
            cairo_pattern_t *pat;

            gdk_cairo_set_source_pixbuf(cr, layer->pixbuf, -layer->width / 2.0, -layer->height / 2.0);

            pat = cairo_get_source(cr);
            cairo_pattern_set_filter(pat, fast_mode ? CAIRO_FILTER_FAST : CAIRO_FILTER_GOOD);

            if (layer->opacity < 1.0) cairo_paint_with_alpha(cr, layer->opacity);
            else cairo_paint(cr);
        }
        cairo_restore(cr);
    }

    cairo_surface_flush(surf);
    cairo_destroy(cr);

    comp = gdk_pixbuf_get_from_surface(surf, 0, 0, render_w, render_h);
    cairo_surface_destroy(surf);

    if (bw) {
        GdkPixbuf *tmp = meme_core_apply_black_and_white(comp);
        g_object_unref(comp);
        comp = tmp;
    }

    if (!fast_mode && (cinematic || deep_fry)) {
        GdkPixbuf *tmp = meme_core_apply_effects(comp, cinematic, deep_fry);
        if (tmp) {
            g_object_unref(comp);
            comp = tmp;
        }
    }
    return comp;
}

void meme_draw_crop_chrome (cairo_t *cr, double w, double h,
                             double abs_x, double abs_y, double abs_w, double abs_h) {
    double scale_ref = (w < h ? w : h) / 800.0;
    double corner_len = CLAMP(26.0 * scale_ref, 18.0, 36.0);
    double edge_len = CLAMP(20.0 * scale_ref, 14.0, 28.0);
    double handle_thick = 4.0;
    int i;

    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    if (abs_y > 0) {
        cairo_rectangle(cr, 0, 0, w, abs_y);
        cairo_fill(cr);
    }
    if (abs_y + abs_h < h) {
        cairo_rectangle(cr, 0, abs_y + abs_h, w, h - (abs_y + abs_h));
        cairo_fill(cr);
    }
    if (abs_x > 0) {
        cairo_rectangle(cr, 0, abs_y, abs_x, abs_h);
        cairo_fill(cr);
    }
    if (abs_x + abs_w < w) {
        cairo_rectangle(cr, abs_x + abs_w, abs_y, w - (abs_x + abs_w), abs_h);
        cairo_fill(cr);
    }

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.35);
    for (i = 1; i <= 2; i++) {
        double gx = abs_x + abs_w * i / 3.0;
        double gy = abs_y + abs_h * i / 3.0;
        cairo_move_to(cr, gx, abs_y);
        cairo_line_to(cr, gx, abs_y + abs_h);
        cairo_move_to(cr, abs_x, gy);
        cairo_line_to(cr, abs_x + abs_w, gy);
    }
    cairo_stroke(cr);


    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_rectangle(cr, abs_x, abs_y, abs_w, abs_h);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_rectangle(cr, abs_x, abs_y, abs_w, abs_h);
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    {
        struct { double x, y, dx, dy; } corners[4] = {
            { abs_x, abs_y, 1, 1 },
            { abs_x + abs_w, abs_y, -1, 1 },
            { abs_x, abs_y + abs_h, 1, -1 },
            { abs_x + abs_w, abs_y + abs_h, -1, -1 },
        };
        int c;
        for (c = 0; c < 4; c++) {
            double x = corners[c].x, y = corners[c].y;
            double dx = corners[c].dx, dy = corners[c].dy;

            cairo_set_line_width(cr, handle_thick + 2.0);
            cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
            cairo_move_to(cr, x + dx * corner_len, y);
            cairo_line_to(cr, x, y);
            cairo_line_to(cr, x, y + dy * corner_len);
            cairo_stroke(cr);

            cairo_set_line_width(cr, handle_thick);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            cairo_move_to(cr, x + dx * corner_len, y);
            cairo_line_to(cr, x, y);
            cairo_line_to(cr, x, y + dy * corner_len);
            cairo_stroke(cr);
        }
    }

    {
        struct { double x, y, horiz; } edges[4] = {
            { abs_x + abs_w / 2.0, abs_y, 1 },
            { abs_x + abs_w / 2.0, abs_y + abs_h, 1 },
            { abs_x, abs_y + abs_h / 2.0, 0 },
            { abs_x + abs_w, abs_y + abs_h / 2.0, 0 },
        };
        int e;
        for (e = 0; e < 4; e++) {
            double x = edges[e].x, y = edges[e].y;
            double half = edge_len / 2.0;

            cairo_set_line_width(cr, handle_thick + 2.0);
            cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
            if (edges[e].horiz) {
                cairo_move_to(cr, x - half, y);
                cairo_line_to(cr, x + half, y);
            } else {
                cairo_move_to(cr, x, y - half);
                cairo_line_to(cr, x, y + half);
            }
            cairo_stroke(cr);

            cairo_set_line_width(cr, handle_thick);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            if (edges[e].horiz) {
                cairo_move_to(cr, x - half, y);
                cairo_line_to(cr, x + half, y);
            } else {
                cairo_move_to(cr, x, y - half);
                cairo_line_to(cr, x, y + half);
            }
            cairo_stroke(cr);
        }
    }

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
}

void meme_draw_alignment_guides (cairo_t *cr, double off_x, double off_y,
                                  double draw_w, double draw_h,
                                  gboolean v_active, double v_x,
                                  gboolean h_active, double h_y) {
    if (!v_active && !h_active) return;

    cairo_save (cr);
    cairo_set_line_width (cr, 1.0);
    cairo_set_dash (cr, (double[]){ 4.0, 3.0 }, 2, 0.0);
    cairo_set_source_rgba (cr, 0.976, 0.204, 0.573, 0.95);

    if (v_active) {
        double x = off_x + v_x * draw_w;
        cairo_move_to (cr, x, off_y);
        cairo_line_to (cr, x, off_y + draw_h);
    }
    if (h_active) {
        double y = off_y + h_y * draw_h;
        cairo_move_to (cr, off_x, y);
        cairo_line_to (cr, off_x + draw_w, y);
    }
    cairo_stroke (cr);
    cairo_restore (cr);
}

GdkPixbuf *meme_bake_stroke_pixbuf (GArray *points, int img_w, int img_h,
                                     double line_width, const GdkRGBA *color,
                                     double *out_cx, double *out_cy,
                                     double *out_w, double *out_h) {
    double min_x, min_y, max_x, max_y, pad, bw, bh;
    int surf_w, surf_h, i;
    cairo_surface_t *surf;
    cairo_t *cr;
    GdkPixbuf *pixbuf;

    if (!points || points->len == 0) return NULL;

    min_x = min_y = G_MAXDOUBLE;
    max_x = max_y = -G_MAXDOUBLE;
    for (i = 0; i < (int) points->len; i++) {
        StrokePoint p = g_array_index (points, StrokePoint, i);
        double px = p.x * img_w, py = p.y * img_h;
        min_x = MIN (min_x, px); max_x = MAX (max_x, px);
        min_y = MIN (min_y, py); max_y = MAX (max_y, py);
    }

    pad = line_width / 2.0 + 2.0;
    min_x -= pad; min_y -= pad; max_x += pad; max_y += pad;
    bw = MAX (max_x - min_x, 1.0);
    bh = MAX (max_y - min_y, 1.0);
    surf_w = (int) ceil (bw);
    surf_h = (int) ceil (bh);

    surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, surf_w, surf_h);
    cr = cairo_create (surf);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width (cr, line_width);
    cairo_set_source_rgba (cr, color->red, color->green, color->blue, color->alpha);

    for (i = 0; i < (int) points->len; i++) {
        StrokePoint p = g_array_index (points, StrokePoint, i);
        double px = p.x * img_w - min_x;
        double py = p.y * img_h - min_y;
        if (i == 0) cairo_move_to (cr, px, py);
        else cairo_line_to (cr, px, py);
    }
    if (points->len == 1) {
        StrokePoint p = g_array_index (points, StrokePoint, 0);
        cairo_line_to (cr, p.x * img_w - min_x + 0.01, p.y * img_h - min_y + 0.01);
    }
    cairo_stroke (cr);
    cairo_surface_flush (surf);

    pixbuf = gdk_pixbuf_get_from_surface (surf, 0, 0, surf_w, surf_h);
    cairo_destroy (cr);
    cairo_surface_destroy (surf);

    *out_cx = (min_x + bw / 2.0) / img_w;
    *out_cy = (min_y + bh / 2.0) / img_h;
    *out_w = surf_w;
    *out_h = surf_h;
    return pixbuf;
}

void meme_draw_stroke_preview (cairo_t *cr, GArray *points,
                                double img_w, double img_h, double scale,
                                double off_x, double off_y,
                                double line_width, const GdkRGBA *color) {
    int i;
    if (!points || points->len == 0) return;

    cairo_save (cr);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width (cr, MAX (line_width * scale, 1.0));
    cairo_set_source_rgba (cr, color->red, color->green, color->blue, color->alpha);

    for (i = 0; i < (int) points->len; i++) {
        StrokePoint p = g_array_index (points, StrokePoint, i);
        double x = off_x + p.x * img_w * scale;
        double y = off_y + p.y * img_h * scale;
        if (i == 0) cairo_move_to (cr, x, y);
        else cairo_line_to (cr, x, y);
    }
    cairo_stroke (cr);
    cairo_restore (cr);
}

GdkTexture *meme_render_editor_overlay(GdkPixbuf *composite, GList *layers, ImageLayer *selected, gboolean crop_active, double cx, double cy, double cw, double ch) {
    GdkPixbuf *res;
    GdkTexture *tex;
    int w;
    int h;
    cairo_surface_t *surf;
    cairo_t *cr;

    if (!composite)
        return NULL;

    w = gdk_pixbuf_get_width(composite);
    h = gdk_pixbuf_get_height(composite);
    surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cr = cairo_create(surf);

    gdk_cairo_set_source_pixbuf(cr, composite, 0, 0);
    cairo_paint(cr);

    if (crop_active) {
        meme_draw_crop_chrome(cr, w, h, cx * w, cy * h, cw * w, ch * h);
    } else if (selected) {
        double sx = selected->x * w;
        double sy = selected->y * h;
        double bw = selected->width * selected->scale;
        double bh = selected->height * selected->scale;
        // radius of circles on the visual handler
        double hr = 6.0;

        cairo_save(cr);
        cairo_translate(cr, sx, sy);
        cairo_rotate(cr, selected->rotation);

        cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);
        cairo_set_line_width(cr, 2.0);
        cairo_rectangle(cr, -bw / 2.0, -bh / 2.0, bw, bh);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

        cairo_arc(cr, -bw / 2.0, -bh / 2.0, hr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, bw / 2.0, -bh / 2.0, hr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, -bw / 2.0, bh / 2.0, hr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, bw / 2.0, bh / 2.0, hr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    cairo_destroy(cr);
    res = gdk_pixbuf_get_from_surface(surf, 0, 0, w, h);
    cairo_surface_destroy(surf);

    tex = gdk_texture_new_for_pixbuf(res);
    g_object_unref(res);
    return tex;
}
