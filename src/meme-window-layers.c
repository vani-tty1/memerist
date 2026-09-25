/* meme-window-layers.c
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

void on_color_changed (GObject *object, GParamSpec *pspec, MemeWindow *self) {
    if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
        const GdkRGBA *tc = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(self->text_color_btn));
        const GdkRGBA *sc = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(self->stroke_color_btn));
        
        if (tc) self->selected_layer->text_color = *tc;
        if (sc) self->selected_layer->stroke_color = *sc;

        g_clear_object(&self->selected_layer->pixbuf);
        render_meme(self);
    }
}

void on_text_changed (MemeWindow *self) { if (self->template_image) render_meme (self); }

void on_layer_text_changed (MemeWindow *self) {
    if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
        GtkTextBuffer *buffer;
        GtkTextIter start, end;

        g_free (self->selected_layer->text);
        buffer = gtk_text_view_get_buffer (self->layer_text_view);
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        
        self->selected_layer->text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
        self->selected_layer->font_size = gtk_spin_button_get_value (self->layer_font_size);

        g_clear_object(&self->selected_layer->pixbuf);
        render_meme (self);
    }
}

void on_add_text_clicked (MemeWindow *self) {
    ImageLayer *new_layer;

    push_undo (self);
    new_layer = g_new0 (ImageLayer, 1);
    new_layer->type = LAYER_TYPE_TEXT;
    new_layer->text = g_strdup ("Text");
    new_layer->font_size = 60.0;
    new_layer->x = 0.5; new_layer->y = 0.5;
    new_layer->scale = 1.0; new_layer->opacity = 1.0;
    new_layer->blend_mode = BLEND_NORMAL;
    self->layers = g_list_append (self->layers, new_layer);
    self->selected_layer = new_layer;
    sync_ui_with_layer(self);
    render_meme (self);
}   

static void on_emoji_chooser_closed (GtkPopover *chooser, gpointer user_data) {
    gtk_widget_unparent (GTK_WIDGET (chooser));
}

static void on_emoji_picked (GtkEmojiChooser *chooser, const char *text, MemeWindow *self) {
    ImageLayer *new_layer;

    push_undo (self);
    new_layer = g_new0 (ImageLayer, 1);
    new_layer->type = LAYER_TYPE_EMOJI;
    new_layer->text = g_strdup (text);
    new_layer->font_size = 96.0;
    new_layer->x = 0.5; new_layer->y = 0.5;
    new_layer->scale = 1.0; new_layer->opacity = 1.0;
    new_layer->blend_mode = BLEND_NORMAL;
    self->layers = g_list_append (self->layers, new_layer);
    self->selected_layer = new_layer;
    sync_ui_with_layer (self);
    render_meme (self);
}


void on_add_emoji_clicked (GtkWidget *btn, MemeWindow *self) {
    GtkWidget *chooser;

    if (!self->template_image) return;

    chooser = gtk_emoji_chooser_new ();
    gtk_widget_set_parent (chooser, btn);
    g_signal_connect (chooser, "emoji-picked", G_CALLBACK (on_emoji_picked), self);
    g_signal_connect (chooser, "closed", G_CALLBACK (on_emoji_chooser_closed), NULL);
    gtk_popover_popup (GTK_POPOVER (chooser));
}

void on_font_changed (GObject *object, GParamSpec *pspec, MemeWindow *self) {
    if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
        PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc (self->font_choose_btn);
        if (desc) {
            g_free (self->selected_layer->font_family);
            self->selected_layer->font_family = pango_font_description_to_string (desc);
            render_meme (self);
            self->selected_layer->pixbuf = NULL;
        }
    }
}

void sync_ui_with_layer(MemeWindow *self) {
    GtkTextBuffer *buffer;

    gboolean sensitive = (self->selected_layer != NULL);
    gboolean is_text = (sensitive && self->selected_layer->type == LAYER_TYPE_TEXT);
    gboolean is_crop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->crop_mode_button));    
    g_signal_handlers_block_by_func(self->layer_opacity_scale, on_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_rotation_scale, on_text_changed, self);
    buffer = gtk_text_view_get_buffer (self->layer_text_view);
    g_signal_handlers_block_by_func(buffer, on_layer_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_font_size, on_layer_text_changed, self);

    if (sensitive) {
        gtk_spin_button_set_value(self->layer_opacity_scale, self->selected_layer->opacity);
        gtk_spin_button_set_value(self->layer_rotation_scale, self->selected_layer->rotation);
        adw_combo_row_set_selected(self->blend_mode_row, self->selected_layer->blend_mode);
        if (is_text) {
            gtk_text_buffer_set_text(buffer, self->selected_layer->text ? self->selected_layer->text : "", -1);
            gtk_spin_button_set_value(self->layer_font_size, self->selected_layer->font_size);
        }
    }
    gtk_widget_set_visible(GTK_WIDGET(self->layer_group), sensitive && !is_crop);
    gtk_widget_set_visible(GTK_WIDGET(self->layer_text_container), is_text);
    gtk_widget_set_visible(GTK_WIDGET(self->layer_font_size_row), is_text);
    gtk_widget_set_sensitive(GTK_WIDGET(self->layer_opacity_scale), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->layer_rotation_scale), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->blend_mode_row), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->delete_layer_button), sensitive);
    
    g_signal_handlers_unblock_by_func(self->layer_opacity_scale, on_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_rotation_scale, on_text_changed, self);
    g_signal_handlers_unblock_by_func(buffer, on_layer_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_font_size, on_layer_text_changed, self);

    gtk_widget_set_visible (GTK_WIDGET (self->font_choose_row), is_text);
    g_signal_handlers_block_by_func(self->text_color_btn, on_color_changed, self);
    g_signal_handlers_block_by_func(self->stroke_color_btn, on_color_changed, self);

    if (sensitive && is_text) {
        gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(self->text_color_btn), &self->selected_layer->text_color);
        gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(self->stroke_color_btn), &self->selected_layer->stroke_color);
    }

    g_signal_handlers_unblock_by_func(self->text_color_btn, on_color_changed, self);
    g_signal_handlers_unblock_by_func(self->stroke_color_btn, on_color_changed, self);
    
    if (is_text && self->selected_layer->font_family) {
        PangoFontDescription *desc;

        g_signal_handlers_block_by_func (self->font_choose_btn, on_font_changed, self);
        desc = pango_font_description_from_string (self->selected_layer->font_family);
        gtk_font_dialog_button_set_font_desc (self->font_choose_btn, desc);
        pango_font_description_free (desc);
        g_signal_handlers_unblock_by_func (self->font_choose_btn, on_font_changed, self);
    }

    update_footer_pages (self);
}

void on_layer_control_changed (MemeWindow *self) {
    if (self->selected_layer) {
        self->selected_layer->opacity = gtk_spin_button_get_value(self->layer_opacity_scale);
        self->selected_layer->rotation = gtk_spin_button_get_value(self->layer_rotation_scale);
        self->selected_layer->blend_mode = (BlendMode)adw_combo_row_get_selected(self->blend_mode_row);
        render_meme(self);
    }
}

void on_delete_layer_clicked (MemeWindow *self) {
    if (self->selected_layer) {
        push_undo (self);
        self->layers = g_list_remove(self->layers, self->selected_layer);
        meme_layer_free(self->selected_layer);
        self->selected_layer = NULL;
        sync_ui_with_layer(self);
        render_meme(self);
    }
}
