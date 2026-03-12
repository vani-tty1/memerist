/* myapp-window.c
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


#include "meme-window-private.h"
#include "meme-fileio.h"
#include "meme-canvas.h"
#include "meme-history.h"
#include "keyboard-shortcuts.h"
#include <glib/gstdio.h>

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

// Function prototypes for internal use
static void populate_template_gallery (MyappWindow *self);

void render_meme (MyappWindow *self) {
    if (!self->template_image) return;
    gboolean is_dragging = (self->drag_type != DRAG_TYPE_NONE);
    if (self->final_meme) g_object_unref(self->final_meme);
    self->final_meme = meme_render_composite(
        self->template_image,
        self->layers,
        is_dragging ? FALSE : gtk_toggle_button_get_active(self->cinematic_button),
        is_dragging ? FALSE : gtk_toggle_button_get_active(self->deep_fry_button)
    );

    GdkTexture *tex = meme_render_editor_overlay(
        self->final_meme,
        self->layers,
        self->selected_layer,
        gtk_toggle_button_get_active(self->crop_mode_button),
        self->crop_x, self->crop_y, self->crop_w, self->crop_h
    );

    gtk_picture_set_paintable(self->meme_preview, GDK_PAINTABLE(tex));
    g_object_unref(tex);
}

static void on_text_changed (MyappWindow *self) { if (self->template_image) render_meme (self); }
static void on_deep_fry_toggled (GtkToggleButton *btn, MyappWindow *self) { render_meme (self); }

static void on_layer_text_changed (MyappWindow *self) {
    if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
        g_free (self->selected_layer->text);
      
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->layer_text_view);
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        
        self->selected_layer->text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
        self->selected_layer->font_size = gtk_spin_button_get_value (self->layer_font_size);
        render_meme (self);
    }
}

static void on_add_text_clicked (MyappWindow *self) {
    push_undo (self);
    ImageLayer *new_layer = g_new0 (ImageLayer, 1);
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

void update_template_image (MyappWindow *self, GdkPixbuf *new_pixbuf) {
    if (!new_pixbuf) return;
    push_undo (self);
    if (self->template_image) g_object_unref (self->template_image);
    self->template_image = new_pixbuf;
    render_meme (self);
}

static void on_rotate_clicked (GtkWidget *btn, MyappWindow *self) {
    gboolean clockwise;
    GdkPixbuf *new_pix;
    if (!self->template_image) return;
    clockwise = (btn == GTK_WIDGET (self->rotate_right_button));
    new_pix = gdk_pixbuf_rotate_simple (self->template_image,
    clockwise ? GDK_PIXBUF_ROTATE_CLOCKWISE : GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
    update_template_image (self, new_pix);
}

static void on_flip_clicked (GtkWidget *btn, MyappWindow *self) {
    gboolean horizontal;
    GdkPixbuf *new_pix;
    if (!self->template_image) return;
    horizontal = (btn == GTK_WIDGET (self->flip_h_button));
    new_pix = gdk_pixbuf_flip (self->template_image, horizontal);
    update_template_image (self, new_pix);
}

static void on_crop_preset_clicked (GtkWidget *btn, MyappWindow *self) {
    int w, h;
    double target_ratio = 1.0;
    double current_ratio;
    if (!self->template_image) return;
    w = gdk_pixbuf_get_width (self->template_image);
    h = gdk_pixbuf_get_height (self->template_image);
    current_ratio = (double)w / (double)h;

    if (btn == GTK_WIDGET (self->crop_square_button)) target_ratio = 1.0;
    else if (btn == GTK_WIDGET (self->crop_43_button)) target_ratio = 4.0/3.0;
    else if (btn == GTK_WIDGET (self->crop_169_button)) target_ratio = 16.0/9.0;

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

static void on_crop_mode_toggled (GtkToggleButton *btn, MyappWindow *self) {
    gboolean active = gtk_toggle_button_get_active (btn);
    gtk_widget_set_visible (GTK_WIDGET (self->transform_group), active);
    gtk_widget_set_visible (GTK_WIDGET (self->layer_group), !active);
    gtk_widget_set_visible (GTK_WIDGET (self->templates_group), !active);
    if (active) {
    self->crop_x = 0.0; self->crop_y = 0.0;
    self->crop_w = 1.0; self->crop_h = 1.0;
    adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
    } else {
        gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
    }
    render_meme(self);
}

static void on_apply_crop_clicked (MyappWindow *self) {
    if (!self->template_image) return;
    int iw = gdk_pixbuf_get_width(self->template_image);
    int ih = gdk_pixbuf_get_height(self->template_image);
    int x = self->crop_x * iw;
    int y = self->crop_y * ih;
    int w = self->crop_w * iw;
    int h = self->crop_h * ih;
    if (w <= 0 || h <= 0) return;

    push_undo(self);
    GList *l;
    for (l = self->layers; l != NULL; l = l->next) {
        ImageLayer *layer = (ImageLayer *)l->data;
        double abs_x = layer->x * iw;
        double abs_y = layer->y * ih;
        layer->x = (abs_x - x) / (double)w;
        layer->y = (abs_y - y) / (double)h;
    }
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(self->template_image, x, y, w, h);
    GdkPixbuf *new_pix = gdk_pixbuf_copy(sub);
    g_object_unref(sub);
    update_template_image(self, new_pix); 
    self->crop_x = 0; self->crop_y = 0; self->crop_w = 1; self->crop_h = 1;
    gtk_toggle_button_set_active(self->crop_mode_button, FALSE);
}

static void on_font_changed (GObject *object, GParamSpec *pspec, MyappWindow *self) {
    if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
        PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc (self->font_choose_btn);
        if (desc) {
            g_free (self->selected_layer->font_family);
            self->selected_layer->font_family = pango_font_description_to_string (desc);
            render_meme (self);
        }
    }
}

void sync_ui_with_layer(MyappWindow *self) {
    gboolean sensitive = (self->selected_layer != NULL);
    gboolean is_text = (sensitive && self->selected_layer->type == LAYER_TYPE_TEXT);

    g_signal_handlers_block_by_func(self->layer_opacity_scale, on_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_rotation_scale, on_text_changed, self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->layer_text_view);
    g_signal_handlers_block_by_func(buffer, on_layer_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_font_size, on_layer_text_changed, self);

    if (sensitive) {
        gtk_range_set_value(GTK_RANGE(self->layer_opacity_scale), self->selected_layer->opacity);
        gtk_range_set_value(GTK_RANGE(self->layer_rotation_scale), self->selected_layer->rotation);
        adw_combo_row_set_selected(self->blend_mode_row, self->selected_layer->blend_mode);
        if (is_text) {
            gtk_text_buffer_set_text(buffer, self->selected_layer->text ? self->selected_layer->text : "", -1);
            gtk_spin_button_set_value(self->layer_font_size, self->selected_layer->font_size);
        }
    }
    if (self->transform_group) {
            gtk_widget_set_visible(GTK_WIDGET(self->transform_group), sensitive && !is_text);
    }
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
    
    if (is_text && self->selected_layer->font_family) {
        g_signal_handlers_block_by_func (self->font_choose_btn, on_font_changed, self);
        PangoFontDescription *desc = pango_font_description_from_string (self->selected_layer->font_family);
        gtk_font_dialog_button_set_font_desc (self->font_choose_btn, desc);
        pango_font_description_free (desc);
        g_signal_handlers_unblock_by_func (self->font_choose_btn, on_font_changed, self);
    }
}

static void on_layer_control_changed (MyappWindow *self) {
    if (self->selected_layer) {
        self->selected_layer->opacity = gtk_range_get_value(GTK_RANGE(self->layer_opacity_scale));
        self->selected_layer->rotation = gtk_range_get_value(GTK_RANGE(self->layer_rotation_scale));
        self->selected_layer->blend_mode = (BlendMode)adw_combo_row_get_selected(self->blend_mode_row);
        render_meme(self);
    }
}

static void on_delete_layer_clicked (MyappWindow *self) {
    if (self->selected_layer) {
        push_undo (self);
        self->layers = g_list_remove(self->layers, self->selected_layer);
        meme_layer_free(self->selected_layer);
        self->selected_layer = NULL;
        sync_ui_with_layer(self);
        render_meme(self);
    }
}

void on_clear_clicked (MyappWindow *self) {
    gtk_stack_set_visible_child_name (self->content_stack, "empty");
    g_clear_object (&self->template_image);
    g_clear_object (&self->final_meme);
    if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; }
    free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);
    self->selected_layer = NULL;
    sync_ui_with_layer(self);
    gtk_picture_set_paintable (self->meme_preview, NULL);
    gtk_toggle_button_set_active (self->deep_fry_button, FALSE);
    gtk_toggle_button_set_active (self->cinematic_button, FALSE);
    gtk_toggle_button_set_active (self->crop_mode_button, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->export_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->add_text_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->add_image_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), FALSE);
    self->zoom_level = 1.0;
    gtk_widget_set_size_request(GTK_WIDGET(self->meme_preview), -1, -1); 
}

void on_copy_clipboard_clicked (MyappWindow *self) {
    if (!self->final_meme) return;

    GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
    GdkPixbuf *save = self->final_meme;

    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        int iw = gdk_pixbuf_get_width(save); 
        int ih = gdk_pixbuf_get_height(save);
        save = gdk_pixbuf_new_subpixbuf(save, self->crop_x * iw, self->crop_y * ih, self->crop_w * iw, self->crop_h * ih);
    } else {
        g_object_ref(save);
    }

    GdkTexture *texture = gdk_texture_new_for_pixbuf (save);
    gdk_clipboard_set_texture (clipboard, texture);

    g_object_unref (texture);
    g_object_unref (save);
    AdwToast *pill_toast = adw_toast_new("Copied to Clipboard");
    adw_toast_overlay_add_toast(self->copy_clip_feedback, pill_toast);
}

static void myapp_window_finalize (GObject *object) {
    MyappWindow *self = MYAPP_WINDOW (object);
    g_clear_object (&self->template_image);
    g_clear_object (&self->final_meme);
    if (self->layers) meme_layer_list_free (self->layers);
    free_history_stack (&self->undo_stack);
    free_history_stack (&self->redo_stack);
    G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static char * get_user_template_dir (void) {
    return g_build_filename (g_get_user_data_dir (), "io.github.vani_tty1.memerist", "templates", NULL);
}

static gboolean is_user_template (const char *path) {
    g_autofree char *user_dir = get_user_template_dir ();
    return g_str_has_prefix (path, user_dir);
}

static void add_file_to_gallery (MyappWindow *self, const char *full_path) {
    GtkWidget *picture;
    if (g_str_has_prefix (full_path, "resource://")) {
        picture = gtk_picture_new_for_resource (full_path + 11);
    } else {
        picture = gtk_picture_new_for_filename (full_path);
    }
    gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
    gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request (picture, 120, 120);
    g_object_set_data_full (G_OBJECT (picture), "template-path", g_strdup (full_path), g_free);
    gtk_flow_box_append (self->template_gallery, picture);
}

static void scan_directory_for_templates (MyappWindow *self, const char *dir_path) {
    GDir *dir = g_dir_open (dir_path, 0, NULL);
    const char *filename;
    if (!dir) return;
    while ((filename = g_dir_read_name (dir)) != NULL) {
        if (g_str_has_suffix (filename, ".png") || g_str_has_suffix (filename, ".jpg") || g_str_has_suffix (filename, ".jpeg")) {
            char *full_path = g_build_filename (dir_path, filename, NULL);
            add_file_to_gallery (self, full_path);
            g_free (full_path);
        }
    }
    g_dir_close (dir);
}

static void scan_resources_for_templates (MyappWindow *self) {
    GError *error = NULL;
    const char *res_path = "/io/github/vani_tty1/memerist/templates";
    char **files = g_resources_enumerate_children (res_path, 0, &error);

    if (files) {
        for (int i = 0; files[i] != NULL; i++) {
            char *full_uri = g_strdup_printf ("resource://%s/%s", res_path, files[i]);
            add_file_to_gallery (self, full_uri);
            g_free (full_uri);
        }
        g_strfreev (files);
    }
}

static void populate_template_gallery (MyappWindow *self) {
    char *user_dir;
    gtk_flow_box_remove_all(self->template_gallery);
    scan_resources_for_templates (self);
    user_dir = get_user_template_dir ();
    g_mkdir_with_parents (user_dir, 0755);
    scan_directory_for_templates (self, user_dir);
    g_free (user_dir);
}

static void on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self) {
    GtkWidget *image;
    const char *template_path;
    GError *error = NULL;
    on_clear_clicked(self);

    if (!child) { 
        gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), FALSE); 
        return; 
    }
  
    image = gtk_flow_box_child_get_child (child);
    template_path = g_object_get_data (G_OBJECT (image), "template-path");
    if (!template_path) return;

    gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), is_user_template (template_path));
  
    g_clear_object (&self->template_image);
    if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; }
    free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);

    if (g_str_has_prefix (template_path, "resource://")) {
        self->template_image = gdk_pixbuf_new_from_resource (template_path + 11, &error);
    } else {
        self->template_image = gdk_pixbuf_new_from_file (template_path, &error);
    }

    if (self->template_image) {
        gtk_stack_set_visible_child_name (self->content_stack, "content");
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), TRUE);
        self->zoom_level = 1.0;
        apply_zoom(self);
        render_meme (self);
    }
}

static void on_copy_import_finished(GObject *source_object, GAsyncResult *res, gpointer user_data){
    GFile * source_file = G_FILE(source_object);
    MyappWindow *self = MYAPP_WINDOW(g_object_get_data(G_OBJECT(source_file), "window-ptr"));
    char *dest_path = g_object_get_data(G_OBJECT(source_file), "dest-path");
    GError *error = NULL;
    
    if(g_file_copy_finish(source_file, res, &error)){
        add_file_to_gallery(self, dest_path);
    }else{
        g_printerr("Error copying file: %s\n", error->message);
        g_clear_error(&error);
    }
    g_object_unref(source_file);
}

static void on_import_template_response(GObject *s, GAsyncResult *r, gpointer d){
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MyappWindow *self = MYAPP_WINDOW(d);
    GFile *source_file, *dest_file;
    GError *error = NULL;
    char *filename, *user_dir_path, *dest_path;
    
    source_file = gtk_file_dialog_open_finish(dialog, r, &error);
    if(error){g_printerr("%s\n", error->message); g_error_free(error); return; }
    
    filename = g_file_get_basename(source_file);
    user_dir_path = get_user_template_dir();
    g_mkdir_with_parents(user_dir_path, 0755);
    dest_path = g_build_filename(user_dir_path, filename, NULL);
    dest_file = g_file_new_for_path(dest_path);
    
    g_object_set_data_full(G_OBJECT(source_file), "dest-path", g_strdup(dest_path), g_free);
    g_object_set_data(G_OBJECT(source_file), "window-ptr", self);
    g_file_copy_async(source_file, dest_file, G_FILE_COPY_OVERWRITE,G_PRIORITY_DEFAULT, NULL, NULL, NULL,on_copy_import_finished, NULL);
    g_free(filename); 
    g_free(user_dir_path); 
    g_free(dest_path);
    g_object_unref(dest_file);
}

static void on_import_template_clicked (MyappWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Import Template");
    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_import_template_response, self);
    g_object_unref (dialog);
}

static void on_delete_confirm_response (GObject *s, GAsyncResult *r, gpointer d) {
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG (s);
    MyappWindow *self = MYAPP_WINDOW (d);
    if (gtk_alert_dialog_choose_finish (dialog, r, NULL) == 1) {
        GList *selected = gtk_flow_box_get_selected_children (self->template_gallery);
        if (selected) {
            GtkFlowBoxChild *child = selected->data;
            GtkWidget *image = gtk_flow_box_child_get_child (child);
            const char *path = g_object_get_data (G_OBJECT (image), "template-path");
            if (g_unlink (path) == 0) {
                gtk_flow_box_remove (self->template_gallery, GTK_WIDGET (child));
                on_clear_clicked (self);
            }
            g_list_free (selected);
        }
    }
}

static void on_delete_template_clicked (MyappWindow *self) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new ("Delete this template?");
    gtk_alert_dialog_set_buttons (dialog, (const char *[]) {"Cancel", "Delete", NULL});
    gtk_alert_dialog_set_default_button (dialog, 1);
    gtk_alert_dialog_choose (dialog, GTK_WINDOW (self), NULL, on_delete_confirm_response, self);
}

void apply_zoom(MyappWindow *self) {
    if (!self->template_image) return;
    int img_w = gdk_pixbuf_get_width(self->template_image);
    int img_h = gdk_pixbuf_get_height(self->template_image);

    int win_w = gtk_widget_get_width(GTK_WIDGET(self)) - 320;
    int win_h = gtk_widget_get_height(GTK_WIDGET(self)) - 60;
    
    double fit_scale = MIN((double)win_w / img_w, (double)win_h / img_h) * 0.6;
    double final_scale = fit_scale * self->zoom_level;

    gtk_widget_set_size_request(GTK_WIDGET(self->meme_preview), (int)(img_w * final_scale), (int)(img_h * final_scale));
}

static void on_zoom_in_clicked(MyappWindow *self) {
    self->zoom_level += 0.2; 
    apply_zoom(self);
}

static void on_zoom_out_clicked(MyappWindow *self) {
    self->zoom_level = MAX(0.2, self->zoom_level - 0.2); 
    apply_zoom(self);
}

static void myapp_window_class_init (MyappWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = myapp_window_finalize;
    
    gtk_widget_class_set_template_from_resource (widget_class, "/io/github/vani_tty1/memerist/meme-window.ui");
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_group);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, templates_group);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, transform_group);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, meme_preview);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, content_stack);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, split_view);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_text_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, font_choose_row);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, font_choose_btn);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_text_container);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_text_view);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_font_size);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_font_size_row);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, export_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_image_button);
    gtk_widget_class_bind_template_child(widget_class, MyappWindow, pill_btn_open_image);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, clear_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_image_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, import_template_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, delete_template_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, global_filters_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, deep_fry_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, template_gallery);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, cinematic_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_opacity_scale);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_rotation_scale);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, blend_mode_row);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, delete_layer_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, crop_mode_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, rotate_left_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, rotate_right_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, flip_h_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, flip_v_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, crop_square_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, crop_43_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, crop_169_button);
    gtk_widget_class_bind_template_callback (widget_class, on_apply_crop_clicked);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, save_project_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_project_button);
    gtk_widget_class_bind_template_child(widget_class, MyappWindow, main_menu_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, zoom_in);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, zoom_out);
    gtk_widget_class_bind_template_child(widget_class, MyappWindow, copy_clipboard_button);
    gtk_widget_class_bind_template_child(widget_class, MyappWindow, copy_clip_feedback);
}

static void myapp_window_init (MyappWindow *self) {
    gtk_widget_init_template (GTK_WIDGET (self));
    self->layers = NULL; self->undo_stack = NULL; self->redo_stack = NULL;
    
    g_signal_connect (self->rotate_left_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->rotate_right_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
    g_signal_connect (self->flip_h_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->flip_v_button, "clicked", G_CALLBACK (on_flip_clicked), self);
    g_signal_connect (self->crop_square_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_43_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_169_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
    g_signal_connect (self->crop_mode_button, "toggled", G_CALLBACK (on_crop_mode_toggled), self);
    
    g_signal_connect_swapped (self->add_text_button, "clicked", G_CALLBACK (on_add_text_clicked), self);
    g_signal_connect (self->font_choose_btn, "notify::font-desc", G_CALLBACK (on_font_changed), self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->layer_text_view);
    g_signal_connect_swapped (buffer, "changed", G_CALLBACK (on_layer_text_changed), self);
    g_signal_connect_swapped (self->layer_font_size, "value-changed", G_CALLBACK (on_layer_text_changed), self);
    
    g_signal_connect_swapped (self->load_image_button, "clicked", G_CALLBACK (on_load_image_clicked), self);
    g_signal_connect_swapped (self->pill_btn_open_image, "clicked", G_CALLBACK(on_load_image_clicked), self);
    g_signal_connect_swapped (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
    g_signal_connect_swapped (self->add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
    g_signal_connect_swapped (self->export_button, "clicked", G_CALLBACK (on_export_clicked), self);
    g_signal_connect_swapped (self->save_project_button, "clicked", G_CALLBACK (myapp_window_save_project), self);
    g_signal_connect_swapped (self->load_project_button, "clicked", G_CALLBACK (on_load_project_clicked), self);
    g_signal_connect_swapped(self->copy_clipboard_button, "clicked", G_CALLBACK(on_copy_clipboard_clicked), self);
    
    g_signal_connect_swapped (self->import_template_button, "clicked", G_CALLBACK (on_import_template_clicked), self);
    g_signal_connect_swapped (self->delete_template_button, "clicked", G_CALLBACK (on_delete_template_clicked), self);
    g_signal_connect (self->template_gallery, "child-activated", G_CALLBACK (on_template_selected), self);
    
    g_signal_connect (self->deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
    g_signal_connect_swapped (self->cinematic_button, "toggled", G_CALLBACK (on_text_changed), self);
    
    g_signal_connect_swapped (self->layer_opacity_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->layer_rotation_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->blend_mode_row, "notify::selected", G_CALLBACK (on_layer_control_changed), self);
    g_signal_connect_swapped (self->delete_layer_button, "clicked", G_CALLBACK (on_delete_layer_clicked), self);
    
    // Handlers moved to meme-canvas.c
    self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
    gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), GTK_EVENT_CONTROLLER (self->drag_gesture));
    g_signal_connect (self->drag_gesture, "drag-begin", G_CALLBACK (on_drag_begin), self);
    g_signal_connect (self->drag_gesture, "drag-update", G_CALLBACK (on_drag_update), self);
    g_signal_connect (self->drag_gesture, "drag-end", G_CALLBACK (on_drag_end), self);
    
    self->zoom_level = 1.0;
    g_signal_connect_swapped (self->zoom_in, "clicked", G_CALLBACK (on_zoom_in_clicked), self);
    g_signal_connect_swapped (self->zoom_out, "clicked", G_CALLBACK (on_zoom_out_clicked), self);
    
    populate_template_gallery (self);
    
    GtkEventController *motion = gtk_event_controller_motion_new ();
    gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), motion);
    g_signal_connect (motion, "motion", G_CALLBACK (on_mouse_move), self);
    
    GtkEventController *key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_window_key_pressed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), key_controller);
    
    GtkBuilder *builder = gtk_builder_new_from_resource("/io/github/vani_tty1/memerist/primary-menu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "primary_menu"));
    
    gtk_menu_button_set_menu_model(self->main_menu_button, menu);
    
    g_object_unref(builder);
}