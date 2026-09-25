#include "meme-canvas.h"
#include "meme-core.h"
#include <math.h>

#define SNAP_PIXELS 8.0
#define MAX_SNAP_CANDIDATES 256

typedef struct {
    double center;
    double line;
} SnapCandidate;

static int collect_snap_candidates (MemeWindow *self, gboolean is_x,
                                     double img_w, double img_h,
                                     double half_extent, SnapCandidate *out) {
    GList *l;
    int n = 0;

    out[n].center = 0.5; out[n].line = 0.5; n++;
    out[n].center = half_extent; out[n].line = 0.0; n++;
    out[n].center = 1.0 - half_extent; out[n].line = 1.0; n++;

    for (l = self->layers; l != NULL && n < MAX_SNAP_CANDIDATES - 3; l = l->next) {
        ImageLayer *o = (ImageLayer *)l->data;
        double o_center, o_half;

        if (o == self->selected_layer) continue;

        if (is_x) {
            o_center = o->x;
            o_half = (o->width * o->scale) / (2.0 * img_w);
        } else {
            o_center = o->y;
            o_half = (o->height * o->scale) / (2.0 * img_h);
        }

        out[n].center = o_center; out[n].line = o_center; n++;
        out[n].center = o_center - o_half + half_extent; out[n].line = o_center - o_half; n++;
        out[n].center = o_center + o_half - half_extent; out[n].line = o_center + o_half; n++;
    }
    return n;
}

static double apply_axis_snap (MemeWindow *self, gboolean is_x, double img_w, double img_h,
                                double proposed, double threshold,
                                gboolean *out_active, double *out_line) {
    SnapCandidate candidates[MAX_SNAP_CANDIDATES];
    double half_extent, best_diff;
    int n, i, best;

    if (!self->selected_layer) { *out_active = FALSE; return proposed; }

    half_extent = is_x
        ? (self->selected_layer->width * self->selected_layer->scale) / (2.0 * img_w)
        : (self->selected_layer->height * self->selected_layer->scale) / (2.0 * img_h);

    n = collect_snap_candidates (self, is_x, img_w, img_h, half_extent, candidates);

    best = -1;
    best_diff = G_MAXDOUBLE;
    for (i = 0; i < n; i++) {
        double diff = fabs (proposed - candidates[i].center);
        if (diff < best_diff) { best_diff = diff; best = i; }
    }

    if (best >= 0 && best_diff < threshold) {
        *out_active = TRUE;
        *out_line = candidates[best].line;
        return candidates[best].center;
    }

    *out_active = FALSE;
    return proposed;
}

void on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MemeWindow *self) {
    GList *l;
    gboolean found;
    double ix, iy, img_w, img_h;
    if (!self->template_image) { gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL); return; }

    if (gtk_toggle_button_get_active (self->draw_mode_button)) {
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "crosshair");
        return;
    }

    meme_get_image_coordinates(GTK_WIDGET(self->meme_preview), self->template_image, x, y, &ix, &iy);
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);
    
    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        const char *cursor;
        double ww = gtk_widget_get_width(GTK_WIDGET(self->meme_preview));
        double wh = gtk_widget_get_height(GTK_WIDGET(self->meme_preview));
        double scale = (ww / img_w < wh / img_h) ? (ww / img_w) : (wh / img_h);
        double rx = 24.0 / (img_w * scale);
        double ry = 24.0 / (img_h * scale);
        ResizeHandle h = meme_get_crop_handle_at_position(ix, iy, self->crop_x, self->crop_y, self->crop_w, self->crop_h, rx, ry);
        cursor = NULL;
        switch (h) {
            case HANDLE_TOP_LEFT: cursor = "nw-resize"; break;
            case HANDLE_TOP_RIGHT: cursor = "ne-resize"; break;
            case HANDLE_BOTTOM_LEFT: cursor = "sw-resize"; break;
            case HANDLE_BOTTOM_RIGHT: cursor = "se-resize"; break;
            case HANDLE_TOP: cursor = "n-resize"; break;
            case HANDLE_BOTTOM: cursor = "s-resize"; break;
            case HANDLE_LEFT: cursor = "w-resize"; break;
            case HANDLE_RIGHT: cursor = "e-resize"; break;
            case HANDLE_CENTER: cursor = "move"; break;
            case HANDLE_NONE: cursor = NULL ; break;
            default: cursor = NULL; break;
        }
        gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), cursor);
        return;
  }


  found = FALSE;
  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
      ImageLayer *layer = (ImageLayer *)l->data;
      double hw = (layer->width * layer->scale) / (2.0 * img_w);
      double hh = (layer->height * layer->scale) / (2.0 * img_h);
      double l_left = layer->x - hw, l_right = layer->x + hw;
      double l_top = layer->y - hh, l_bot = layer->y + hh;  
      double cx = 20.0 / img_w;
    
    if (layer == self->selected_layer) {
        if (fabs(ix - l_left) < cx && fabs(iy - l_top) < cx) { gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), "nw-resize"); found = TRUE; break; }
        if (fabs(ix - l_right) < cx && fabs(iy - l_top) < cx) { gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), "ne-resize"); found = TRUE; break; }
        if (fabs(ix - l_left) < cx && fabs(iy - l_bot) < cx) { gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), "sw-resize"); found = TRUE; break; }
        if (fabs(ix - l_right) < cx && fabs(iy - l_bot) < cx) { gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), "se-resize"); found = TRUE; break; }
    }
    if (ix >= l_left && ix <= l_right && iy >= l_top && iy <= l_bot) {
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "move");
        found = TRUE; break;
    }
}
    if (!found) gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
}

void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MemeWindow *self) {
    GList *l;
    double ix, iy, img_w, img_h;
    if (!self->template_image) return;
    meme_get_image_coordinates(GTK_WIDGET(self->meme_preview), self->template_image, x, y, &ix, &iy);
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);

    if (gtk_toggle_button_get_active (self->draw_mode_button)) {
        StrokePoint pt;
        push_undo (self);
        self->drag_type = DRAG_TYPE_DRAW_STROKE;
        if (self->draw_points) g_array_free (self->draw_points, TRUE);
        self->draw_points = g_array_new (FALSE, FALSE, sizeof (StrokePoint));
        pt.x = CLAMP (ix, 0.0, 1.0);
        pt.y = CLAMP (iy, 0.0, 1.0);
        g_array_append_val (self->draw_points, pt);
        return;
    }

    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        double ww = gtk_widget_get_width(GTK_WIDGET(self->meme_preview));
        double wh = gtk_widget_get_height(GTK_WIDGET(self->meme_preview));
        double scale = (ww / img_w < wh / img_h) ? (ww / img_w) : (wh / img_h);
        double rx = 24.0 / (img_w * scale);
        double ry = 24.0 / (img_h * scale);
        self->active_crop_handle = meme_get_crop_handle_at_position(ix, iy, self->crop_x, self->crop_y, self->crop_w, self->crop_h, rx, ry);
        if (self->active_crop_handle == HANDLE_CENTER) self->drag_type = DRAG_TYPE_CROP_MOVE;
        else if (self->active_crop_handle != HANDLE_NONE) self->drag_type = DRAG_TYPE_CROP_RESIZE;
        else self->drag_type = DRAG_TYPE_NONE;
        
        self->drag_start_x = ix; self->drag_start_y = iy;
        self->drag_obj_start_x = self->crop_x; self->drag_obj_start_y = self->crop_y;
        self->drag_obj_start_scale = self->crop_w; self->drag_obj_start_h = self->crop_h;
        return;
    }


    for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
        ImageLayer *layer = (ImageLayer *)l->data;
        double hw, hh, l_left, l_right, l_top, l_bot, cx;
        gboolean corner;

        if (layer->is_annotation)
            continue;

        hw = (layer->width * layer->scale) / (2.0 * img_w);
        hh = (layer->height * layer->scale) / (2.0 * img_h);
        l_left = layer->x - hw; l_right = layer->x + hw;
        l_top = layer->y - hh; l_bot = layer->y + hh;
        cx = 20.0 / img_w;
        corner = (fabs(ix - l_left) < cx || fabs(ix - l_right) < cx) && (fabs(iy - l_top) < cx || fabs(iy - l_bot) < cx);

        if (layer == self->selected_layer && corner) {
            push_undo(self);
            self->drag_type = DRAG_TYPE_IMAGE_RESIZE;
            self->selected_layer = layer;
            self->drag_obj_start_scale = layer->scale;
            self->drag_start_x = ix * img_w; self->drag_start_y = iy * img_h; 
            sync_ui_with_layer(self); render_meme(self); return;
        }

        if (ix >= l_left && ix <= l_right && iy >= l_top && iy <= l_bot) {
            push_undo(self);
            self->drag_type = DRAG_TYPE_IMAGE_MOVE;
            self->selected_layer = layer;
            self->drag_obj_start_x = layer->x; self->drag_obj_start_y = layer->y;
            self->drag_start_x = ix; self->drag_start_y = iy;
            self->snap_guide_v_active = FALSE; self->snap_guide_h_active = FALSE;
            sync_ui_with_layer(self); render_meme(self); return;
        }
    }
    if (self->selected_layer) { self->selected_layer = NULL; sync_ui_with_layer(self); render_meme(self); }
}

void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MemeWindow *self) {
    double dx, dy, img_w, img_h, ww, wh, wr, hr, s;
    if (self->drag_type == DRAG_TYPE_NONE || !self->template_image) return; 
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);
    ww = gtk_widget_get_width(GTK_WIDGET(self->meme_preview));
    wh = gtk_widget_get_height(GTK_WIDGET(self->meme_preview));
    wr = ww/img_w; hr = wh/img_h;
    s = (wr < hr) ? wr : hr;    
    dx = (offset_x / s) / img_w; dy = (offset_y / s) / img_h;
    
    if (self->drag_type == DRAG_TYPE_CROP_MOVE) {
        self->crop_x = CLAMP(self->drag_obj_start_x + dx, 0.0, 1.0 - self->crop_w);
        self->crop_y = CLAMP(self->drag_obj_start_y + dy, 0.0, 1.0 - self->crop_h);
    } else if (self->drag_type == DRAG_TYPE_CROP_RESIZE) {
        double nx = self->drag_obj_start_x, ny = self->drag_obj_start_y;
        double nw = self->drag_obj_start_scale, nh = self->drag_obj_start_h;
        ResizeHandle h = self->active_crop_handle;
      
        if (h==HANDLE_LEFT || h==HANDLE_TOP_LEFT || h==HANDLE_BOTTOM_LEFT) {
            double mr = self->drag_obj_start_x + self->drag_obj_start_scale;
            nx = CLAMP(self->drag_obj_start_x + dx, 0, mr - 0.05); nw = mr - nx;
        } else if (h==HANDLE_RIGHT || h==HANDLE_TOP_RIGHT || h==HANDLE_BOTTOM_RIGHT) {
            nw = CLAMP(self->drag_obj_start_scale + dx, 0.05, 1.0 - nx);
        }
        if (h==HANDLE_TOP || h==HANDLE_TOP_LEFT || h==HANDLE_TOP_RIGHT) {
            double mb = self->drag_obj_start_y + self->drag_obj_start_h;
            ny = CLAMP(self->drag_obj_start_y + dy, 0, mb - 0.05); nh = mb - ny;
        } else if (h==HANDLE_BOTTOM || h==HANDLE_BOTTOM_LEFT || h==HANDLE_BOTTOM_RIGHT) {
            nh = CLAMP(self->drag_obj_start_h + dy, 0.05, 1.0 - ny);
        }
        self->crop_x = nx; self->crop_y = ny; self->crop_w = nw; self->crop_h = nh;
    } else if (self->drag_type == DRAG_TYPE_IMAGE_MOVE && self->selected_layer) {
        double px = CLAMP(self->drag_obj_start_x + dx, 0.0, 1.0);
        double py = CLAMP(self->drag_obj_start_y + dy, 0.0, 1.0);
        double thresh_x = SNAP_PIXELS / (img_w * s);
        double thresh_y = SNAP_PIXELS / (img_h * s);

        self->selected_layer->x = apply_axis_snap (self, TRUE, img_w, img_h, px, thresh_x,
                                                     &self->snap_guide_v_active, &self->snap_guide_v_x);
        self->selected_layer->y = apply_axis_snap (self, FALSE, img_w, img_h, py, thresh_y,
                                                     &self->snap_guide_h_active, &self->snap_guide_h_y);
    } else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE && self->selected_layer) {
        double cx = self->selected_layer->x * img_w, cy = self->selected_layer->y * img_h;
        double sdx = self->drag_start_x - cx, sdy = self->drag_start_y - cy;
        double cdx = (self->drag_start_x + offset_x/s) - cx, cdy = (self->drag_start_y + offset_y/s) - cy;
        double dist_s = sqrt(sdx*sdx + sdy*sdy), dist_c = sqrt(cdx*cdx + cdy*cdy);
        if (dist_s > 5.0) self->selected_layer->scale = CLAMP(self->drag_obj_start_scale * (dist_c/dist_s), 0.1, 5.0);
        if (self->selected_layer->type == LAYER_TYPE_TEXT) {
            self->selected_layer->pixbuf = NULL;
        }
    } else if (self->drag_type == DRAG_TYPE_DRAW_STROKE && self->draw_points) {
        double start_x, start_y, wx, wy, pix, piy;
        StrokePoint pt;
        gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
        wx = start_x + offset_x;
        wy = start_y + offset_y;
        meme_get_image_coordinates (GTK_WIDGET (self->meme_preview), self->template_image, wx, wy, &pix, &piy);
        pt.x = CLAMP (pix, 0.0, 1.0);
        pt.y = CLAMP (piy, 0.0, 1.0);
        g_array_append_val (self->draw_points, pt);
    }

    if (self->drag_type == DRAG_TYPE_CROP_MOVE || self->drag_type == DRAG_TYPE_CROP_RESIZE ||
        self->drag_type == DRAG_TYPE_DRAW_STROKE) {
        gtk_widget_queue_draw(GTK_WIDGET(self->crop_overlay_area));
    } else {
        render_meme(self);
        if (self->drag_type == DRAG_TYPE_IMAGE_MOVE)
            gtk_widget_queue_draw(GTK_WIDGET(self->crop_overlay_area));
    }
}

void on_drag_end (GtkGestureDrag *g, double x, double y, MemeWindow *self) {
    if (self->snap_guide_v_active || self->snap_guide_h_active) {
        self->snap_guide_v_active = FALSE;
        self->snap_guide_h_active = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(self->crop_overlay_area));
    }
    if (self->drag_type == DRAG_TYPE_DRAW_STROKE && self->draw_points) {
        if (self->draw_points->len > 0 && self->template_image) {
            int img_w = gdk_pixbuf_get_width (self->template_image);
            int img_h = gdk_pixbuf_get_height (self->template_image);
            double cx = 0.5, cy = 0.5, bw = 0, bh = 0;
            GdkPixbuf *pb = meme_bake_stroke_pixbuf (self->draw_points, img_w, img_h,
                                                      self->draw_line_width, &self->draw_color,
                                                      &cx, &cy, &bw, &bh);
            if (pb) {
                ImageLayer *new_layer = g_new0 (ImageLayer, 1);
                new_layer->type = LAYER_TYPE_IMAGE;
                new_layer->pixbuf = pb;
                new_layer->width = bw;
                new_layer->height = bh;
                new_layer->x = cx;
                new_layer->y = cy;
                new_layer->scale = 1.0;
                new_layer->opacity = 1.0;
                new_layer->blend_mode = BLEND_NORMAL;
                new_layer->is_annotation = TRUE;
                self->layers = g_list_append (self->layers, new_layer);
                sync_ui_with_layer (self);
            }
        }
        g_array_free (self->draw_points, TRUE);
        self->draw_points = NULL;
    }
    self->drag_type = DRAG_TYPE_NONE;
    render_meme(self);
}

void free_history_stack (GList **stack) {
    GList *l;
    for (l = *stack; l != NULL; l = l->next) meme_layer_list_free ((GList *)l->data);
    g_list_free (*stack);
    *stack = NULL;
}

void update_undo_redo_sensitivity (MemeWindow *self) {
    gboolean can_undo = self->undo_stack != NULL;
    gboolean can_redo = self->redo_stack != NULL;

    if (self->undo_button)
        gtk_widget_set_sensitive (GTK_WIDGET (self->undo_button), can_undo);
    if (self->redo_button)
        gtk_widget_set_sensitive (GTK_WIDGET (self->redo_button), can_redo);
    if (self->footer_undo_button)
        gtk_widget_set_sensitive (GTK_WIDGET (self->footer_undo_button), can_undo);
    if (self->footer_redo_button)
        gtk_widget_set_sensitive (GTK_WIDGET (self->footer_redo_button), can_redo);
}

void push_undo (MemeWindow *self) {
    free_history_stack (&self->redo_stack);
    if (g_list_length (self->undo_stack) >= 20) {
        GList *last = g_list_last (self->undo_stack);
        meme_layer_list_free ((GList *)last->data);
        self->undo_stack = g_list_delete_link (self->undo_stack, last);
    }
    self->undo_stack = g_list_prepend (self->undo_stack, meme_layer_list_copy (self->layers));
    update_undo_redo_sensitivity (self);
}

void myapp_window_perform_undo(MemeWindow *self) {
    if (!self->undo_stack) return;
    self->redo_stack = g_list_prepend (self->redo_stack, self->layers);
    self->layers = (GList *)self->undo_stack->data;
    self->undo_stack = g_list_delete_link (self->undo_stack, self->undo_stack);
    self->selected_layer = NULL;
    sync_ui_with_layer (self);
    render_meme (self);
    update_undo_redo_sensitivity (self);
}

void myapp_window_perform_redo (MemeWindow *self) {
    if (!self->redo_stack) return;
    self->undo_stack = g_list_prepend (self->undo_stack, self->layers);
    self->layers = (GList *)self->redo_stack->data;
    self->redo_stack = g_list_delete_link (self->redo_stack, self->redo_stack);
    self->selected_layer = NULL;
    sync_ui_with_layer (self);
    render_meme (self);
    update_undo_redo_sensitivity (self);
}
