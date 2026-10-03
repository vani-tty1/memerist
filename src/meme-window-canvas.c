/* meme-window-canvas.c
 *
 * Copyright 2025 Giovanni
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "adwaita.h"
#include "gtk/gtk.h"
#include "meme-window-private.h"
#include "meme-canvas.h"

gboolean meme_window_begin_layer_drag (MemeWindow *self) {
    ImageLayer *layer = self->selected_layer;
    GdkTexture *below = NULL, *above = NULL, *layer_tex = NULL;
    MemeDragLayerState state;
    gboolean cinematic, deepfry, bw, reduce_quality_on_drag;

    g_clear_object (&self->drag_preview);

    if (!layer || !self->template_image) return FALSE;
    if (self->drag_type != DRAG_TYPE_IMAGE_MOVE &&
        self->drag_type != DRAG_TYPE_IMAGE_RESIZE) return FALSE;

    cinematic = gtk_toggle_button_get_active (self->cinematic_button);
    deepfry = gtk_toggle_button_get_active (self->deep_fry_button);
    bw = gtk_toggle_button_get_active (self->bw_button);
    reduce_quality_on_drag = g_settings_get_boolean (self->template_settings, "reduce-quality-on-drag");

    if (bw) return FALSE;

    if ((cinematic || deepfry) && !reduce_quality_on_drag) return FALSE;

    if (!meme_render_drag_split (self->template_image, self->layers, layer,
                                 &below, &above, &layer_tex))
        return FALSE;

    self->drag_preview = meme_drag_preview_new (below, above, layer_tex);
    g_clear_object (&below);
    g_clear_object (&above);
    g_clear_object (&layer_tex);

    meme_drag_preview_state_from_layer (&state, layer);
    meme_drag_preview_set_state (self->drag_preview, &state);
    gtk_picture_set_paintable (self->meme_preview, GDK_PAINTABLE (self->drag_preview));
    return TRUE;
}

void meme_window_update_layer_drag (MemeWindow *self) {
    MemeDragLayerState state;

    if (!self->drag_preview || !self->selected_layer) return;
    meme_drag_preview_state_from_layer (&state, self->selected_layer);
    meme_drag_preview_set_state (self->drag_preview, &state);
}

void meme_window_end_layer_drag (MemeWindow *self) {
    g_clear_object (&self->drag_preview);
}

void render_meme (MemeWindow *self) {
    gboolean is_dragging, is_crop_drag, crop_active, cinematic, deepfry, bw_button;
    gboolean reduce_quality_on_drag, use_fast_preview;
    GdkTexture *tex;

    if (self->drag_preview) {
        if (self->drag_type == DRAG_TYPE_IMAGE_MOVE ||
            self->drag_type == DRAG_TYPE_IMAGE_RESIZE) {
            meme_window_update_layer_drag (self);
            return;
        }
        g_clear_object (&self->drag_preview);
    }

    if (!self->template_image) return;
    
    is_dragging = (self->drag_type != DRAG_TYPE_NONE);
    is_crop_drag = (self->drag_type == DRAG_TYPE_CROP_MOVE ||
                             self->drag_type == DRAG_TYPE_CROP_RESIZE);
    crop_active = gtk_toggle_button_get_active(self->crop_mode_button);
    cinematic = gtk_toggle_button_get_active(self->cinematic_button);
    deepfry = gtk_toggle_button_get_active(self->deep_fry_button);
    bw_button = gtk_toggle_button_get_active(self->bw_button);

    reduce_quality_on_drag = g_settings_get_boolean (self->template_settings, "reduce-quality-on-drag");
    use_fast_preview = is_dragging && reduce_quality_on_drag;

    if (!self->final_meme || !is_crop_drag) {
        if (self->final_meme) g_object_unref(self->final_meme);
        self->final_meme = meme_render_composite(self->template_image,
                                        self->layers,
                                        cinematic,
                                        deepfry,
                                        bw_button,
                                        use_fast_preview);
    }
    gtk_widget_queue_draw(GTK_WIDGET(self->meme_preview));

    if (crop_active || (is_dragging && !is_crop_drag) || !self->selected_layer) {
        tex = gdk_texture_new_for_pixbuf(self->final_meme);
    } else {
        tex = meme_render_editor_overlay(
            self->final_meme, self->layers, self->selected_layer,
            FALSE, 0, 0, 0, 0
        );
    }

    gtk_picture_set_paintable(self->meme_preview, GDK_PAINTABLE(tex));
    g_object_unref(tex);

    gtk_widget_queue_draw(GTK_WIDGET(self->crop_overlay_area));
}

void on_deep_fry_toggled (GtkToggleButton *btn, MemeWindow *self) { render_meme (self); }

void update_template_image (MemeWindow *self, GdkPixbuf *new_pixbuf) {
    if (!new_pixbuf) return;
    push_undo (self);
    if (self->template_image) g_object_unref (self->template_image);
    self->template_image = new_pixbuf;
    render_meme (self);
}

void apply_zoom(MemeWindow *self) {
    int img_w, img_h, win_w, win_h;
    double fit_scale, final_scale;

    if (!self->template_image) return;
    img_w = gdk_pixbuf_get_width(self->template_image);
    img_h = gdk_pixbuf_get_height(self->template_image);
    win_w = gtk_widget_get_width(GTK_WIDGET(self)) - 320;
    win_h = gtk_widget_get_height(GTK_WIDGET(self)) - 60;

    fit_scale = MIN((double)win_w / img_w, (double)win_h / img_h) * 0.6;
    final_scale = fit_scale * self->zoom_level;

    gtk_widget_set_size_request(GTK_WIDGET(self->meme_preview), (int)(img_w * final_scale), (int)(img_h * final_scale));
}

void on_zoom_in_clicked(MemeWindow *self) {
    self->zoom_level += 0.2; 
    apply_zoom(self);
}

void on_zoom_out_clicked(MemeWindow *self) {
    self->zoom_level = MAX(0.2, self->zoom_level - 0.2); 
    apply_zoom(self);
}

gboolean on_canvas_scroll(GtkEventControllerScroll *ctrl, double dx, double dy, MemeWindow *self) {
    if (dy > 0) {
        self->zoom_level = MAX(0.2, self->zoom_level - 0.1);
    } else if (dy < 0) {
        self->zoom_level += 0.1;
    }
    apply_zoom(self);
    return TRUE;
}
