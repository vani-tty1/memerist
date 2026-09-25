/* meme-window-crop.c
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

void draw_crop_overlay (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    MemeWindow *self = MEME_WINDOW (user_data);
    double img_w, img_h, scale, off_x, off_y;

    if (!self->template_image) return;

    img_w = gdk_pixbuf_get_width (self->template_image);
    img_h = gdk_pixbuf_get_height (self->template_image);
    if (width <= 0 || height <= 0 || img_w <= 0 || img_h <= 0) return;

    scale = MIN (width / img_w, height / img_h);
    off_x = (width - img_w * scale) / 2.0;
    off_y = (height - img_h * scale) / 2.0;

    if (gtk_toggle_button_get_active (self->crop_mode_button)) {
        meme_draw_crop_chrome (cr, width, height,
            off_x + self->crop_x * img_w * scale,
            off_y + self->crop_y * img_h * scale,
            self->crop_w * img_w * scale,
            self->crop_h * img_h * scale);
    } else if (self->drag_type == DRAG_TYPE_DRAW_STROKE && self->draw_points) {
        meme_draw_stroke_preview (cr, self->draw_points, img_w, img_h, scale,
                                   off_x, off_y, self->draw_line_width, &self->draw_color);
    } else if (self->drag_type == DRAG_TYPE_IMAGE_MOVE &&
               (self->snap_guide_v_active || self->snap_guide_h_active)) {
        meme_draw_alignment_guides (cr, off_x, off_y, img_w * scale, img_h * scale,
                                     self->snap_guide_v_active, self->snap_guide_v_x,
                                     self->snap_guide_h_active, self->snap_guide_h_y);
    }
}

void on_rotate_clicked (GtkWidget *btn, MemeWindow *self) {
    gboolean clockwise;
    GdkPixbuf *new_pix;
    if (!self->template_image) return;
    clockwise = (btn == GTK_WIDGET (self->rotate_right_button)) ||
                (btn == GTK_WIDGET (self->footer_rotate_right_button));
    new_pix = gdk_pixbuf_rotate_simple (self->template_image,
    clockwise ? GDK_PIXBUF_ROTATE_CLOCKWISE : GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
    update_template_image (self, new_pix);
    meme_window_transform_gif_frames_rotate (self, clockwise);
    if (gtk_toggle_button_get_active (self->crop_mode_button)) {
        self->crop_x = 0.0; self->crop_y = 0.0;
        self->crop_w = 1.0; self->crop_h = 1.0;
        render_meme (self);
    }
}

void on_flip_clicked (GtkWidget *btn, MemeWindow *self) {
    gboolean horizontal;
    GdkPixbuf *new_pix;
    if (!self->template_image) return;
    horizontal = (btn == GTK_WIDGET (self->flip_h_button)) ||
                 (btn == GTK_WIDGET (self->footer_flip_h_button));
    new_pix = gdk_pixbuf_flip (self->template_image, horizontal);
    update_template_image (self, new_pix);
    meme_window_transform_gif_frames_flip (self, horizontal);
    // Same reasoning as rotate: don't let a stale crop selection carry
    // over onto the flipped image.
    if (gtk_toggle_button_get_active (self->crop_mode_button)) {
        self->crop_x = 0.0; self->crop_y = 0.0;
        self->crop_w = 1.0; self->crop_h = 1.0;
        render_meme (self);
    }
}

void on_crop_preset_clicked (GtkWidget *btn, MemeWindow *self) {
    int w, h;
    double target_ratio = 1.0;
    double current_ratio;
    if (!self->template_image) return;
    w = gdk_pixbuf_get_width (self->template_image);
    h = gdk_pixbuf_get_height (self->template_image);
    current_ratio = (double)w / (double)h;

    if (btn == GTK_WIDGET (self->crop_square_button) || btn == GTK_WIDGET (self->footer_crop_square_button))
        target_ratio = 1.0;
    else if (btn == GTK_WIDGET (self->crop_43_button) || btn == GTK_WIDGET (self->footer_crop_43_button))
        target_ratio = 4.0/3.0;
    else if (btn == GTK_WIDGET (self->crop_169_button) || btn == GTK_WIDGET (self->footer_crop_169_button))
        target_ratio = 16.0/9.0;

    if (current_ratio > target_ratio) {
        self->crop_h = 1.0;
        self->crop_w = target_ratio / current_ratio;
        self->crop_x = (1.0 - self->crop_w) / 2.0;
        self->crop_y = 0.0;
    } else {
        self->crop_w = 1.0;
        self->crop_h = current_ratio / target_ratio;
        self->crop_y = (1.0 - self->crop_h) / 2.0;
        self->crop_x = 0.0;
    }
    render_meme(self);
}

void update_footer_pages (MemeWindow *self) {
    gboolean crop_active = gtk_toggle_button_get_active (self->crop_mode_button);
    gboolean draw_active = gtk_toggle_button_get_active (self->draw_mode_button);
    gboolean is_text = (self->selected_layer != NULL && self->selected_layer->type == LAYER_TYPE_TEXT);

    gtk_widget_set_visible (GTK_WIDGET (self->footer_tools_page), !crop_active && !draw_active && !is_text);
    gtk_widget_set_visible (GTK_WIDGET (self->footer_text_page), !crop_active && !draw_active && is_text);
}

void on_exit_text_editing_clicked (MemeWindow *self) {
    self->selected_layer = NULL;
    sync_ui_with_layer (self);
    render_meme (self);
}

void on_crop_mode_toggled (GtkToggleButton *btn, MemeWindow *self) {
    gboolean active = gtk_toggle_button_get_active (btn);
    if (active && gtk_toggle_button_get_active (self->draw_mode_button))
        gtk_toggle_button_set_active (self->draw_mode_button, FALSE);
    gtk_widget_set_visible (GTK_WIDGET (self->transform_group), active);
    gtk_widget_set_visible (GTK_WIDGET (self->layer_group), !active);
    gtk_widget_set_visible (GTK_WIDGET (self->layer_group), !active && self->selected_layer != NULL);
    if (active) {
    meme_window_pause_gif_animation (self);
    self->crop_x = 0.0; self->crop_y = 0.0;
    self->crop_w = 1.0; self->crop_h = 1.0;
    g_clear_object (&self->crop_session_template_snapshot);
    if (self->template_image)
        self->crop_session_template_snapshot = g_object_ref (self->template_image);
    } else {
        gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
        meme_window_resume_gif_animation (self);
    }
    update_footer_pages (self);
    render_meme(self);
}

void on_draw_mode_toggled (GtkToggleButton *btn, MemeWindow *self) {
    gboolean active = gtk_toggle_button_get_active (btn);
    if (active && gtk_toggle_button_get_active (self->crop_mode_button))
        gtk_toggle_button_set_active (self->crop_mode_button, FALSE);
    gtk_widget_set_visible (GTK_WIDGET (self->draw_group), active);
    if (active) {
        self->selected_layer = NULL;
        sync_ui_with_layer (self);
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "crosshair");
    } else {
        gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
    }
    update_footer_pages (self);
    render_meme (self);
}

void on_exit_draw_editing_clicked (MemeWindow *self) {
    gtk_toggle_button_set_active (self->draw_mode_button, FALSE);
}

void on_draw_color_changed (GObject *object, GParamSpec *pspec, MemeWindow *self) {
    const GdkRGBA *c = gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (self->draw_color_btn));
    if (c) self->draw_color = *c;
}

void on_draw_width_changed (MemeWindow *self) {
    self->draw_line_width = gtk_spin_button_get_value (self->draw_width_scale);
}

void on_cancel_crop_clicked (MemeWindow *self) {
    // Restore the image exactly as it was when crop mode was entered,
    // undoing any rotate/flip done mid-session. The undo_stack only
    // tracks layers, not template_image, so we can't use it here.
    if (self->crop_session_template_snapshot) {
        if (self->template_image) g_object_unref (self->template_image);
        self->template_image = self->crop_session_template_snapshot;
        self->crop_session_template_snapshot = NULL;
    }
    self->crop_x = 0.0; self->crop_y = 0.0;
    self->crop_w = 1.0; self->crop_h = 1.0;
    gtk_toggle_button_set_active (self->crop_mode_button, FALSE);
    render_meme (self);
}

void on_apply_crop_clicked (MemeWindow *self) {
    GdkPixbuf *sub;
    GdkPixbuf *new_pix;
    int iw, ih, x, y, w, h;
    GList *l;

    if (!self->template_image) return;
    iw = gdk_pixbuf_get_width(self->template_image);
    ih = gdk_pixbuf_get_height(self->template_image);
    x = self->crop_x * iw;
    y = self->crop_y * ih;
    w = self->crop_w * iw;
    h = self->crop_h * ih;
    if (w <= 0 || h <= 0) return;

    push_undo(self);

    for (l = self->layers; l != NULL; l = l->next) {
        ImageLayer *layer = (ImageLayer *)l->data;
        double abs_x = layer->x * iw;
        double abs_y = layer->y * ih;
        layer->x = (abs_x - x) / (double)w;
        layer->y = (abs_y - y) / (double)h;
    }
    sub = gdk_pixbuf_new_subpixbuf(self->template_image, x, y, w, h);
    new_pix = gdk_pixbuf_copy(sub);
    g_object_unref(sub);
    update_template_image(self, new_pix); 
    meme_window_transform_gif_frames_crop (self, x, y, w, h);
    self->crop_x = 0; self->crop_y = 0; self->crop_w = 1; self->crop_h = 1;
    gtk_toggle_button_set_active(self->crop_mode_button, FALSE);
    g_clear_object (&self->crop_session_template_snapshot);
}
