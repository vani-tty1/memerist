#include "meme-canvas.h"
#include "meme-history.h"
#include <math.h>

void on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self) {
    double ix, iy, img_w, img_h;
    if (!self->template_image) { gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL); return; }
    
    meme_get_image_coordinates(GTK_WIDGET(self->meme_preview), self->template_image, x, y, &ix, &iy);
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);
    
    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        ResizeHandle h = meme_get_crop_handle_at_position(ix, iy, self->crop_x, self->crop_y, self->crop_w, self->crop_h);
        const char *cursor = NULL;
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
            default: cursor = NULL; break;
        }
        gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), cursor);
        return;
  }

  GList *l;
  gboolean found = FALSE;
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

void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self) {
    double ix, iy, img_w, img_h;
    if (!self->template_image) return;
    meme_get_image_coordinates(GTK_WIDGET(self->meme_preview), self->template_image, x, y, &ix, &iy);
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);

    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        self->active_crop_handle = meme_get_crop_handle_at_position(ix, iy, self->crop_x, self->crop_y, self->crop_w, self->crop_h);
        if (self->active_crop_handle == HANDLE_CENTER) self->drag_type = DRAG_TYPE_CROP_MOVE;
        else if (self->active_crop_handle != HANDLE_NONE) self->drag_type = DRAG_TYPE_CROP_RESIZE;
        else self->drag_type = DRAG_TYPE_NONE;
        
        self->drag_start_x = ix; self->drag_start_y = iy;
        self->drag_obj_start_x = self->crop_x; self->drag_obj_start_y = self->crop_y;
        self->drag_obj_start_scale = self->crop_w; self->drag_obj_start_h = self->crop_h;
        return;
    }

    GList *l;
    for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
        ImageLayer *layer = (ImageLayer *)l->data;
        double hw = (layer->width * layer->scale) / (2.0 * img_w);
        double hh = (layer->height * layer->scale) / (2.0 * img_h);
        double l_left = layer->x - hw, l_right = layer->x + hw;
        double l_top = layer->y - hh, l_bot = layer->y + hh;
        double cx = 20.0 / img_w;
        gboolean corner = (fabs(ix - l_left) < cx || fabs(ix - l_right) < cx) && (fabs(iy - l_top) < cx || fabs(iy - l_bot) < cx);

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
            sync_ui_with_layer(self); render_meme(self); return;
        }
    }
    if (self->selected_layer) { self->selected_layer = NULL; sync_ui_with_layer(self); render_meme(self); }
}

void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self) {
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
        self->selected_layer->x = CLAMP(self->drag_obj_start_x + dx, 0.0, 1.0);
        self->selected_layer->y = CLAMP(self->drag_obj_start_y + dy, 0.0, 1.0);
    } else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE && self->selected_layer) {
        double cx = self->selected_layer->x * img_w, cy = self->selected_layer->y * img_h;
        double sdx = self->drag_start_x - cx, sdy = self->drag_start_y - cy;
        double cdx = (self->drag_start_x + offset_x/s) - cx, cdy = (self->drag_start_y + offset_y/s) - cy;
        double dist_s = sqrt(sdx*sdx + sdy*sdy), dist_c = sqrt(cdx*cdx + cdy*cdy);
        if (dist_s > 5.0) self->selected_layer->scale = CLAMP(self->drag_obj_start_scale * (dist_c/dist_s), 0.1, 5.0);
    }
    render_meme(self);
}

void on_drag_end (GtkGestureDrag *g, double x, double y, MyappWindow *self) { 
    self->drag_type = DRAG_TYPE_NONE; 
    render_meme(self);
}