/* meme-window-templates.c
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
#include <glib/gstdio.h>
#include <stdio.h>

static void set_template_select_mode (MemeWindow *self, gboolean active);
static void update_template_gallery_empty_state (MemeWindow *self);
static guint count_flowbox_children (GtkFlowBox *flowbox);

void on_open_template_window_clicked (MemeWindow *self) {
    //its just like seeing her, for the first time again.
    if (self->template_select_mode)
        set_template_select_mode (self, FALSE);
    adw_dialog_present (self->template_window, GTK_WIDGET (self));
}

static char * get_user_template_dir (void) {
    return g_build_filename (g_get_user_data_dir (), "io.github.vani_tty1.memerist", "templates", NULL);
}

static gboolean is_user_template (const char *path) {
    g_autofree char *user_dir = get_user_template_dir ();
    return g_str_has_prefix (path, user_dir);
}

static void add_file_to_gallery (MemeWindow *self, const char *full_path) {
    GtkWidget *picture;
    gint64 *mtime = g_new0(gint64, 1);

    if (g_str_has_prefix (full_path, "resource://")) {
        picture = gtk_picture_new_for_resource (full_path + 11);
        *mtime = 0; // Built-in resources are technically the oldest
    } else if (g_str_has_suffix (full_path, ".gif")) {
        GStatBuf stat_buf;
        GdkPixbuf *frame = gdk_pixbuf_new_from_file (full_path, NULL);

        if (frame) {
            GdkTexture *texture = gdk_texture_new_for_pixbuf (frame);
            picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
            g_object_unref (texture);
            g_object_unref (frame);
        } else {
            picture = gtk_picture_new ();
        }

        if (g_stat (full_path, &stat_buf) == 0) {
            *mtime = stat_buf.st_mtime;
        }
    } else {
        GStatBuf stat_buf;
        picture = gtk_picture_new_for_filename (full_path);

        if (g_stat(full_path, &stat_buf) == 0) {
            *mtime = stat_buf.st_mtime;
        }
    }

    gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
    gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request (picture, 120, 120);

    g_object_set_data_full (G_OBJECT (picture), "template-path", g_strdup (full_path), g_free);

    g_object_set_data_full (G_OBJECT (picture), "mtime", mtime, g_free);

    gtk_flow_box_append (self->template_gallery, picture);
}

// files are processed in small background batches
// to (hopefully) increases performance
static void on_templates_enumerated (GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source_object);
    MemeWindow *self = MEME_WINDOW (user_data);
    GError *error = NULL;
    
    GList *files = g_file_enumerator_next_files_finish (enumerator, res, &error);

    if (error) {
        g_printerr ("Error reading templates: %s\n", error->message);
        g_clear_error (&error);
        g_object_unref (enumerator);
        return;
    }

    // if no more files, we are done
    if (!files) {
        g_object_unref (enumerator);
        update_template_gallery_empty_state (self);
        return;
    }

    for (GList *l = files; l != NULL; l = l->next) {
        GFileInfo *info = G_FILE_INFO (l->data);
        const char *name = g_file_info_get_name (info);
        
        if (g_str_has_suffix (name, ".png") || g_str_has_suffix (name, ".jpg") || g_str_has_suffix (name, ".jpeg") || g_str_has_suffix (name, ".gif")) {
            GFile *child = g_file_enumerator_get_child (enumerator, info);
            char *full_path = g_file_get_path (child);
            add_file_to_gallery (self, full_path);
            g_free (full_path);
            g_object_unref (child);
        }
    }
    g_list_free_full (files, g_object_unref);
    g_file_enumerator_next_files_async (enumerator, 10, G_PRIORITY_DEFAULT, NULL, on_templates_enumerated, self);
}

static void on_templates_dir_opened (GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GFile *dir = G_FILE (source_object);
    MemeWindow *self = MEME_WINDOW (user_data);
    GError *error = NULL;
    GFileEnumerator *enumerator = g_file_enumerate_children_finish (dir, res, &error);

    if (error) {
        g_clear_error (&error);
        return;
    }
    g_file_enumerator_next_files_async (enumerator, 10, G_PRIORITY_DEFAULT, NULL, on_templates_enumerated, self);
}

static void scan_directory_for_templates_async (MemeWindow *self, const char *dir_path) {
    GFile *dir = g_file_new_for_path (dir_path);
    g_file_enumerate_children_async (dir,
                                     "standard::name",
                                     G_FILE_QUERY_INFO_NONE,
                                     G_PRIORITY_DEFAULT,
                                     NULL,
                                     on_templates_dir_opened,
                                     self);
    g_object_unref (dir);
}

static gboolean is_template_hidden (MemeWindow *self, const char *path) {
    gchar **hidden = g_settings_get_strv (self->template_settings, "hidden-templates");
    gboolean found = FALSE;

    for (int i = 0; hidden[i] != NULL; i++) {
        if (g_strcmp0 (hidden[i], path) == 0) { found = TRUE; break; }
    }
    g_strfreev (hidden);
    return found;
}

void update_restore_templates_sensitivity (MemeWindow *self) {
    gchar **hidden = g_settings_get_strv (self->template_settings, "hidden-templates");
    gtk_widget_set_sensitive (GTK_WIDGET (self->restore_templates_button), hidden[0] != NULL);
    g_strfreev (hidden);
}

static void hide_template (MemeWindow *self, const char *path) {
    gchar **hidden = g_settings_get_strv (self->template_settings, "hidden-templates");
    GPtrArray *updated = g_ptr_array_new ();
    gboolean already_hidden = FALSE;

    for (int i = 0; hidden[i] != NULL; i++) {
        g_ptr_array_add (updated, hidden[i]);
        if (g_strcmp0 (hidden[i], path) == 0) already_hidden = TRUE;
    }
    if (!already_hidden) g_ptr_array_add (updated, (gpointer) path);
    g_ptr_array_add (updated, NULL);

    g_settings_set_strv (self->template_settings, "hidden-templates",
                          (const gchar * const *) updated->pdata);

    g_ptr_array_free (updated, TRUE);
    g_strfreev (hidden);

    update_restore_templates_sensitivity (self);
}

void on_restore_templates_clicked (MemeWindow *self) {
    g_settings_reset (self->template_settings, "hidden-templates");
    populate_template_gallery (self);
    update_restore_templates_sensitivity (self);
}

static void scan_resources_for_templates (MemeWindow *self) {
    GError *error = NULL;
    const char *res_path = "/io/github/vani_tty1/memerist/templates";
    char **files = g_resources_enumerate_children (res_path, 0, &error);

    if (files) {
        for (int i = 0; files[i] != NULL; i++) {
            char *full_uri = g_strdup_printf ("resource://%s/%s", res_path, files[i]);
            if (!is_template_hidden (self, full_uri))
                add_file_to_gallery (self, full_uri);
            g_free (full_uri);
        }
        g_strfreev (files);
    }
}

void populate_template_gallery (MemeWindow *self) {
    char *user_dir;
    gtk_flow_box_remove_all(self->template_gallery);
    scan_resources_for_templates (self);
    user_dir = get_user_template_dir ();
    g_mkdir_with_parents (user_dir, 0755);
    scan_directory_for_templates_async (self, user_dir); 
    g_free (user_dir);
    update_template_gallery_empty_state (self);
}

void on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MemeWindow *self) {
    GtkWidget *image;
    const char *template_path;
    GError *error = NULL;

    if (self->template_select_mode) return;

    on_clear_clicked(self);

    if (!child) return;
  
    image = gtk_flow_box_child_get_child (child);
    template_path = g_object_get_data (G_OBJECT (image), "template-path");
    if (!template_path) return;

    g_clear_object (&self->template_image);
    if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; }
    free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);

    g_clear_pointer (&self->template_gif_path, g_free);
    self->template_is_gif = !g_str_has_prefix (template_path, "resource://") &&
                             g_str_has_suffix (template_path, ".gif");
    if (self->template_is_gif)
        self->template_gif_path = g_strdup (template_path);

    if (g_str_has_prefix (template_path, "resource://")) {
        self->template_image = gdk_pixbuf_new_from_resource (template_path + 11, &error);
    } else {
        self->template_image = gdk_pixbuf_new_from_file (template_path, &error);
    }

    if (self->template_image) {
        gtk_stack_set_visible_child_name (self->content_stack, "content");
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_emoji_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->bw_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->draw_mode_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), TRUE);
        self->zoom_level = 1.0;
        apply_zoom(self);
        render_meme (self);
        meme_window_start_gif_animation (self);
        adw_dialog_close (self->template_window);
    }
}

static void on_copy_import_finished(GObject *source_object, GAsyncResult *res, gpointer user_data){
    GFile * source_file = G_FILE(source_object);
    MemeWindow *self = MEME_WINDOW(g_object_get_data(G_OBJECT(source_file), "window-ptr"));
    char *dest_path = g_object_get_data(G_OBJECT(source_file), "dest-path");
    GError *error = NULL;
    
    if(g_file_copy_finish(source_file, res, &error)){
        add_file_to_gallery(self, dest_path);
        update_template_gallery_empty_state (self);
    }else{
        g_printerr("Error copying file: %s\n", error->message);
        g_clear_error(&error);
    }
    g_object_unref(source_file);
}

static void on_import_template_response(GObject *s, GAsyncResult *r, gpointer d){
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
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

void on_import_template_clicked (MemeWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Import Template");
    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_import_template_response, self);
    g_object_unref (dialog);
}

static void on_delete_confirm_response (GObject *s, GAsyncResult *r, gpointer d) {
    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(s);
    MemeWindow *self = MEME_WINDOW (d);
    const char *choice = adw_alert_dialog_choose_finish(dialog, r);
    if (g_strcmp0(choice, "delete") == 0) {
        GList *selected = gtk_flow_box_get_selected_children (self->template_gallery);
        GList *l;

        for (l = selected; l != NULL; l = l->next) {
            GtkFlowBoxChild *child = l->data;
            GtkWidget *image = gtk_flow_box_child_get_child (child);
            const char *path = g_object_get_data (G_OBJECT (image), "template-path");

            if (!path) continue;

            if (is_user_template (path)) {
                if (g_unlink (path) == 0)
                    gtk_flow_box_remove (self->template_gallery, GTK_WIDGET (child));
            } else {
                hide_template (self, path);
                gtk_flow_box_remove (self->template_gallery, GTK_WIDGET (child));
            }
        }
        g_list_free (selected);

        gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), FALSE);
        gtk_button_set_label (self->select_all_button, "Select All");
        update_template_gallery_empty_state (self);
    }
}

void on_delete_template_clicked (MemeWindow *self) {
    GList *selected = gtk_flow_box_get_selected_children (self->template_gallery);
    guint count = g_list_length (selected);
    char *message;
    AdwAlertDialog *dialog;

    g_list_free (selected);
    if (count == 0) return;

    message = g_strdup_printf ("Delete %u selected template%s?", count, count == 1 ? "" : "s");
    dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (message, NULL));
    g_free (message);

    adw_alert_dialog_add_responses(dialog, "cancel", "Cancel", "delete", "Delete", NULL);
    adw_alert_dialog_set_response_appearance(dialog, "delete", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_choose(dialog, GTK_WIDGET (self->template_window), NULL, on_delete_confirm_response, self);
}

static guint count_flowbox_children (GtkFlowBox *flowbox) {
    guint total = 0;
    GtkWidget *child;

    for (child = gtk_widget_get_first_child (GTK_WIDGET (flowbox));
         child != NULL;
         child = gtk_widget_get_next_sibling (child)) {
        total++;
    }
    return total;
}

static void update_template_gallery_empty_state (MemeWindow *self) {
    gboolean has_templates = count_flowbox_children (self->template_gallery) > 0;
    gtk_stack_set_visible_child_name (self->template_content_stack,
                                       has_templates ? "gallery" : "empty");
    gtk_widget_set_sensitive (GTK_WIDGET (self->select_all_button), has_templates);
}

void on_template_selection_changed (GtkFlowBox *flowbox, MemeWindow *self) {
    GList *selected;
    guint selected_count, total;

    if (!self->template_select_mode) return;

    selected = gtk_flow_box_get_selected_children (flowbox);
    selected_count = g_list_length (selected);
    g_list_free (selected);

    total = count_flowbox_children (flowbox);

    gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), selected_count > 0);
    gtk_button_set_label (self->select_all_button, (total > 0 && selected_count >= total) ? "Deselect All" : "Select All");
}

void on_select_all_clicked (MemeWindow *self) {
    guint total = count_flowbox_children (self->template_gallery);
    GList *selected = gtk_flow_box_get_selected_children (self->template_gallery);
    guint selected_count = g_list_length (selected);
    g_list_free (selected);

    if (total > 0 && selected_count >= total) {
        gtk_flow_box_unselect_all (self->template_gallery);
    } else {
        gtk_flow_box_select_all (self->template_gallery);
    }
}

static void set_template_select_mode (MemeWindow *self, gboolean active) {
    self->template_select_mode = active;

    gtk_flow_box_unselect_all (self->template_gallery);
    gtk_flow_box_set_selection_mode (self->template_gallery,
                                      active ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE);

    gtk_widget_set_visible (GTK_WIDGET (self->import_template_button), !active);
    gtk_widget_set_visible (GTK_WIDGET (self->select_all_button), active);
    gtk_widget_set_visible (GTK_WIDGET (self->delete_template_button), active);
    gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), FALSE);
    gtk_button_set_label (self->select_all_button, "Select All");

    if (active) {
        gtk_button_set_label (self->select_mode_button, "Cancel");
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->select_mode_button), "Cancel");
    } else {
        gtk_button_set_icon_name (self->select_mode_button, "document-edit-symbolic");
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->select_mode_button), "Manage Templates");
    }
}

void on_select_mode_clicked (MemeWindow *self) {
    set_template_select_mode (self, !self->template_select_mode);
}

gint sort_templates_by_mtime(GtkFlowBoxChild *child1, GtkFlowBoxChild *child2, gpointer user_data) {
    GtkWidget *pic1 = gtk_flow_box_child_get_child (child1);
    GtkWidget *pic2 = gtk_flow_box_child_get_child (child2);

    gint64 *mtime1 = g_object_get_data (G_OBJECT (pic1), "mtime");
    gint64 *mtime2 = g_object_get_data (G_OBJECT (pic2), "mtime");

    gint64 t1 = mtime1 ? *mtime1 : 0;
    gint64 t2 = mtime2 ? *mtime2 : 0;

    if (t1 > t2) return -1;
    if (t1 < t2) return 1;
    return 0;
}
