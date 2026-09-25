/* meme-window.c
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
#include "meme-theme-switcher.h"
#include "meme-fileio.h"
#include "meme-canvas.h"
#include "meme-application.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include "config.h"

G_DEFINE_FINAL_TYPE (MemeWindow, meme_window, ADW_TYPE_APPLICATION_WINDOW)

void on_clear_clicked (MemeWindow *self) {
    meme_window_stop_gif_animation (self);
    gtk_stack_set_visible_child_name (self->content_stack, "empty");
    g_clear_object (&self->template_image);
    g_clear_object (&self->final_meme);
    g_clear_object (&self->crop_session_template_snapshot);
    if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; }
    free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);
    update_undo_redo_sensitivity (self);
    self->selected_layer = NULL;
    sync_ui_with_layer(self);
    gtk_picture_set_paintable (self->meme_preview, NULL);
    gtk_toggle_button_set_active (self->deep_fry_button, FALSE);
    gtk_toggle_button_set_active (self->cinematic_button, FALSE);
    gtk_toggle_button_set_active (self->crop_mode_button, FALSE);
    gtk_toggle_button_set_active (self->draw_mode_button, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->export_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->add_text_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->add_emoji_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->add_image_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->draw_mode_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), FALSE);
    gtk_toggle_button_set_active (self->bw_button, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->bw_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->clear_button), FALSE);
    self->zoom_level = 1.0;
    gtk_widget_set_size_request(GTK_WIDGET(self->meme_preview), -1, -1); 
}

void on_copy_clipboard_clicked (MemeWindow *self) {
    AdwToast *pill_toast;
    GdkClipboard *clipboard;
    GdkPixbuf *save;
    GdkTexture *texture;

    if (!self->final_meme) return;

    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
    save = self->final_meme;

    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        int iw = gdk_pixbuf_get_width(save); 
        int ih = gdk_pixbuf_get_height(save);
        save = gdk_pixbuf_new_subpixbuf(save, self->crop_x * iw, self->crop_y * ih, self->crop_w * iw, self->crop_h * ih);
    } else {
        g_object_ref(save);
    }

    texture = gdk_texture_new_for_pixbuf (save);
    gdk_clipboard_set_texture (clipboard, texture);
    g_object_unref (texture);
    g_object_unref (save);
    pill_toast = adw_toast_new("Copied to Clipboard");
    adw_toast_overlay_add_toast(self->copy_clip_feedback, pill_toast);
}

static void myapp_window_finalize (GObject *object) {
    MemeWindow *self = MEME_WINDOW (object);
    meme_window_stop_gif_animation (self);
    g_clear_object (&self->template_image);
    g_clear_object (&self->final_meme);
    g_clear_object (&self->crop_session_template_snapshot);
    g_clear_object (&self->template_window);
    g_clear_object (&self->template_settings);
    g_free (self->template_gif_path);
    if (self->draw_points) g_array_free (self->draw_points, TRUE);
    if (self->layers) meme_layer_list_free (self->layers);
    free_history_stack (&self->undo_stack);
    free_history_stack (&self->redo_stack);
    G_OBJECT_CLASS (meme_window_parent_class)->finalize (object);
}

static void
meme_window_toggle_sidebar (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter) {
    MemeWindow *self = MEME_WINDOW (widget);
    gboolean visible = adw_overlay_split_view_get_show_sidebar (self->split_view);
    adw_overlay_split_view_set_show_sidebar (self->split_view, !visible);
}

static void meme_window_class_init (MemeWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = myapp_window_finalize;
    g_type_ensure (MEME_TYPE_THEME_SWITCHER);
    gtk_widget_class_set_template_from_resource (widget_class, "/io/github/vani_tty1/memerist/meme-window.ui");
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, export_loading_screen);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_group);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, open_template_row);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, transform_group);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, draw_group);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, draw_mode_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, draw_color_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, draw_width_scale);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_draw_mode_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, meme_preview);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, crop_overlay_area);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, content_stack);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, split_view);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, add_text_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, add_emoji_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, font_choose_row);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, font_choose_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_text_container);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_text_view);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_font_size);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_font_size_row);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, undo_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, redo_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, export_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, load_image_button);
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, pill_btn_open_image);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, clear_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, add_image_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, global_filters_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, deep_fry_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, cinematic_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, bw_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_opacity_scale);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, layer_rotation_scale);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, blend_mode_row);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, delete_layer_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, crop_mode_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, rotate_left_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, rotate_right_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, flip_h_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, flip_v_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, crop_square_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, crop_43_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, crop_169_button);
    gtk_widget_class_bind_template_callback (widget_class, on_apply_crop_clicked);
    gtk_widget_class_bind_template_callback (widget_class, on_cancel_crop_clicked);
    gtk_widget_class_bind_template_callback (widget_class, on_open_template_window_clicked);
    gtk_widget_class_bind_template_callback (widget_class, meme_window_paste_from_clipboard);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, save_project_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, load_project_button);
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, main_menu_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, zoom_in);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, zoom_out);
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, copy_clipboard_button);
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, copy_clip_feedback);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, text_color_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, stroke_color_btn);
    gtk_widget_class_bind_template_child(widget_class, MemeWindow, file_popover);

    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_add_image_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_crop_mode_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_add_text_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_add_emoji_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_copy_clipboard_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_global_filters_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_cinematic_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_deep_fry_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_delete_layer_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_zoom_in);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_zoom_out);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_clear_button);

    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_rotate_left_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_rotate_right_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_flip_h_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_flip_v_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_crop_square_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_crop_43_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_crop_169_button);

    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_tools_page);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_transform_page);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_draw_page);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_draw_color_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_draw_width_scale);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_exit_draw_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_text_page);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_font_choose_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_text_color_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_stroke_color_btn);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_layer_text_container);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_layer_text_view);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_layer_font_size);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_text_delete_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_exit_text_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_bw_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_undo_button);
    gtk_widget_class_bind_template_child (widget_class, MemeWindow, footer_redo_button);
    gtk_widget_class_install_action (widget_class, "win.toggle-sidebar", NULL,
                                      meme_window_toggle_sidebar);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F9, 0,
                                          "win.toggle-sidebar", NULL);
}

static gboolean
on_window_drop (GtkDropTarget *target, const GValue *value,
                 double x, double y, MemeWindow *self)
{
    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) {
        GdkFileList *file_list = g_value_get_boxed (value);
        GSList *files = gdk_file_list_get_files (file_list);
        gboolean handled = FALSE;

        if (files) {
            meme_window_open_file (self, G_FILE (files->data));
            handled = TRUE;
        } else {
            adw_toast_overlay_add_toast (self->copy_clip_feedback,
                                          adw_toast_new ("Couldn't read the dropped item"));
        }
        g_slist_free (files);
        return handled;
    }

    if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE)) {
        meme_window_open_texture (self, GDK_TEXTURE (g_value_get_object (value)));
        return TRUE;
    }

    adw_toast_overlay_add_toast (self->copy_clip_feedback,
                                  adw_toast_new ("That doesn't look like an image"));
    return FALSE;
}

static void meme_window_init (MemeWindow *self) {
    GtkEventController *scroll;
    GtkEventController *key_controller;
    GtkEventController *motion;
    GtkTextBuffer *buffer;

    gtk_widget_init_template (GTK_WIDGET (self));
    #ifdef PROFILE
        if (g_strcmp0 (PROFILE, "development") == 0)
            gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
    #endif
    self->layers = NULL; self->undo_stack = NULL; self->redo_stack = NULL;
    self->draw_points = NULL;
    self->draw_line_width = 8.0;
    self->draw_color = (GdkRGBA) { 0.91, 0.1, 0.15, 1.0 };
    g_signal_connect (self->text_color_btn, "notify::rgba", G_CALLBACK (on_color_changed), self);
    g_signal_connect (self->stroke_color_btn, "notify::rgba", G_CALLBACK (on_color_changed), self);
    
    g_signal_connect (self->rotate_left_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->rotate_right_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->flip_h_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->flip_v_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->crop_square_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_43_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_169_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_mode_button, "toggled", G_CALLBACK (on_crop_mode_toggled), self);
    g_signal_connect (self->draw_mode_button, "toggled", G_CALLBACK (on_draw_mode_toggled), self);
    g_signal_connect (self->draw_color_btn, "notify::rgba", G_CALLBACK (on_draw_color_changed), self);
    g_signal_connect_swapped (self->draw_width_scale, "value-changed", G_CALLBACK (on_draw_width_changed), self);
    
    g_signal_connect_swapped (self->add_text_button, "clicked", G_CALLBACK (on_add_text_clicked), self);
    g_signal_connect (self->add_emoji_button, "clicked", G_CALLBACK (on_add_emoji_clicked), self);
    g_signal_connect (self->font_choose_btn, "notify::font-desc", G_CALLBACK (on_font_changed), self);
    buffer = gtk_text_view_get_buffer (self->layer_text_view);
    g_signal_connect_swapped (buffer, "changed", G_CALLBACK (on_layer_text_changed), self);
    g_signal_connect_swapped (self->layer_font_size, "value-changed", G_CALLBACK (on_layer_text_changed), self);
    
    g_signal_connect_swapped (self->load_image_button, "clicked", G_CALLBACK (on_load_image_clicked), self);
    g_signal_connect_swapped (self->pill_btn_open_image, "clicked", G_CALLBACK(on_load_image_clicked), self);
    g_signal_connect_swapped (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
    g_signal_connect_swapped (self->add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
    g_signal_connect_swapped (self->undo_button, "clicked", G_CALLBACK (myapp_window_perform_undo), self);
    g_signal_connect_swapped (self->redo_button, "clicked", G_CALLBACK (myapp_window_perform_redo), self);
    g_signal_connect_swapped (self->export_button, "clicked", G_CALLBACK (on_export_clicked), self);
    g_signal_connect_swapped (self->save_project_button, "clicked", G_CALLBACK (myapp_window_save_project), self);
    g_signal_connect_swapped (self->load_project_button, "clicked", G_CALLBACK (on_load_project_clicked), self);
    g_signal_connect_swapped(self->copy_clipboard_button, "clicked", G_CALLBACK(on_copy_clipboard_clicked), self);
    
    {
        GtkBuilder *template_builder = gtk_builder_new_from_resource ("/io/github/vani_tty1/memerist/template-window.ui");
        self->template_window = ADW_DIALOG (gtk_builder_get_object (template_builder, "template_window"));
        g_object_ref_sink (self->template_window);
        self->template_gallery = GTK_FLOW_BOX (gtk_builder_get_object (template_builder, "template_gallery"));
        self->import_template_button = GTK_BUTTON (gtk_builder_get_object (template_builder, "import_template_button"));
        self->delete_template_button = GTK_BUTTON (gtk_builder_get_object (template_builder, "delete_template_button"));
        self->select_mode_button = GTK_BUTTON (gtk_builder_get_object (template_builder, "select_mode_button"));
        self->select_all_button = GTK_BUTTON (gtk_builder_get_object (template_builder, "select_all_button"));
        self->restore_templates_button = GTK_BUTTON (gtk_builder_get_object (template_builder, "restore_templates_button"));
        self->template_content_stack = GTK_STACK (gtk_builder_get_object (template_builder, "template_content_stack"));
        g_object_unref (template_builder);
    }

    self->template_settings = g_settings_new ("io.github.vani_tty1.memerist");
    update_restore_templates_sensitivity (self);

    g_signal_connect_swapped (self->import_template_button, "clicked", G_CALLBACK (on_import_template_clicked), self);
    g_signal_connect_swapped (self->delete_template_button, "clicked", G_CALLBACK (on_delete_template_clicked), self);
    g_signal_connect_swapped (self->select_all_button, "clicked", G_CALLBACK (on_select_all_clicked), self);
    g_signal_connect_swapped (self->restore_templates_button, "clicked", G_CALLBACK (on_restore_templates_clicked), self);
    g_signal_connect_swapped (self->select_mode_button, "clicked", G_CALLBACK (on_select_mode_clicked), self);
    g_signal_connect (self->template_gallery, "child-activated", G_CALLBACK (on_template_selected), self);
    g_signal_connect (self->template_gallery, "selected-children-changed", G_CALLBACK (on_template_selection_changed), self);
    
    g_signal_connect (self->deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
    g_signal_connect_swapped (self->cinematic_button, "toggled", G_CALLBACK (on_text_changed), self);
    
    g_signal_connect_swapped (self->layer_opacity_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->layer_rotation_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->blend_mode_row, "notify::selected", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->delete_layer_button, "clicked", G_CALLBACK (on_delete_layer_clicked), self);
    
    gtk_drawing_area_set_draw_func (self->crop_overlay_area, draw_crop_overlay, self, NULL);
    gtk_widget_set_can_target (GTK_WIDGET (self->crop_overlay_area), FALSE);

    // Handlers moved to meme-canvas.c
    self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
    gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), GTK_EVENT_CONTROLLER (self->drag_gesture));
    g_signal_connect (self->drag_gesture, "drag-begin", G_CALLBACK (on_drag_begin), self);
    g_signal_connect (self->drag_gesture, "drag-update", G_CALLBACK (on_drag_update), self);
    g_signal_connect (self->drag_gesture, "drag-end", G_CALLBACK (on_drag_end), self);

    {
        GtkDropTarget *drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY);
        GType drop_types[] = { GDK_TYPE_FILE_LIST, GDK_TYPE_TEXTURE };
        gtk_drop_target_set_gtypes (drop_target, drop_types, G_N_ELEMENTS (drop_types));
        g_signal_connect (drop_target, "drop", G_CALLBACK (on_window_drop), self);
        gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
    }
    
    self->zoom_level = 1.0;
    g_signal_connect_swapped (self->zoom_in, "clicked", G_CALLBACK (on_zoom_in_clicked), self);
    g_signal_connect_swapped (self->zoom_out, "clicked", G_CALLBACK (on_zoom_out_clicked), self);


    g_signal_connect_swapped (self->footer_undo_button, "clicked", G_CALLBACK (myapp_window_perform_undo), self);
    g_signal_connect_swapped (self->footer_redo_button, "clicked", G_CALLBACK (myapp_window_perform_redo), self);
    g_signal_connect_swapped (self->footer_add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
    g_signal_connect (self->footer_crop_mode_button, "toggled", G_CALLBACK (on_crop_mode_toggled), self);
    g_signal_connect (self->footer_draw_mode_button, "toggled", G_CALLBACK (on_draw_mode_toggled), self);
    g_signal_connect_swapped (self->footer_add_text_button, "clicked", G_CALLBACK (on_add_text_clicked), self);
    g_signal_connect (self->footer_add_emoji_button, "clicked", G_CALLBACK (on_add_emoji_clicked), self);
    g_signal_connect_swapped (self->footer_copy_clipboard_button, "clicked", G_CALLBACK (on_copy_clipboard_clicked), self);
    g_signal_connect (self->footer_deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
    g_signal_connect_swapped (self->footer_cinematic_button, "toggled", G_CALLBACK (on_text_changed), self);
    g_signal_connect_swapped (self->footer_delete_layer_button, "clicked", G_CALLBACK (on_delete_layer_clicked), self);
    g_signal_connect_swapped (self->footer_zoom_in, "clicked", G_CALLBACK (on_zoom_in_clicked), self);
    g_signal_connect_swapped (self->footer_zoom_out, "clicked", G_CALLBACK (on_zoom_out_clicked), self);
    g_signal_connect_swapped (self->footer_clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);

    g_signal_connect (self->footer_rotate_left_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->footer_rotate_right_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->footer_flip_h_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->footer_flip_v_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->footer_crop_square_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->footer_crop_43_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->footer_crop_169_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);

    g_signal_connect_swapped (self->footer_text_delete_button, "clicked", G_CALLBACK (on_delete_layer_clicked), self);
    g_signal_connect_swapped (self->footer_exit_text_button, "clicked", G_CALLBACK (on_exit_text_editing_clicked), self);
    g_signal_connect_swapped (self->footer_exit_draw_button, "clicked", G_CALLBACK (on_exit_draw_editing_clicked), self);
    g_signal_connect (self->bw_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
    g_signal_connect (self->footer_bw_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
    populate_template_gallery (self);
    
    motion = gtk_event_controller_motion_new ();
    gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), motion);
    g_signal_connect (motion, "motion", G_CALLBACK (on_mouse_move), self);
    
    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_window_key_pressed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), key_controller);
    
    
    scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_canvas_scroll), self);
    gtk_widget_add_controller(GTK_WIDGET(self->meme_preview), scroll);

    gtk_flow_box_set_sort_func (self->template_gallery, sort_templates_by_mtime, NULL, NULL);
    update_footer_pages (self);
}
