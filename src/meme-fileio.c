#include "meme-fileio.h"
#include "adwaita.h"
#include "meme-canvas.h"
#include <glib/gstdio.h>

static gchar *pixbuf_to_base64(GdkPixbuf *pixbuf) {
    if (!pixbuf) return NULL;
    gchar *buffer = NULL; gsize buffer_size = 0;
    if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size, "png", NULL, NULL)) {
        gchar *base64 = g_base64_encode((const guchar *)buffer, buffer_size);
        g_free(buffer); return base64;
    }
    return NULL;
}

static GdkPixbuf *base64_to_pixbuf(const gchar *base64) {
    if (!base64) return NULL;
    gsize out_len = 0;
    guchar *decoded = g_base64_decode(base64, &out_len);
    if (!decoded) return NULL;
    GInputStream *stream = g_memory_input_stream_new_from_data(decoded, out_len, g_free);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    g_object_unref(stream); return pixbuf;
}

static void on_save_project_response(GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    GFile *file = gtk_file_dialog_save_finish(dialog, r, NULL);
    if (file) {
        GKeyFile *keyfile = g_key_file_new();
        if (self->template_image) {
            gchar *b64 = pixbuf_to_base64(self->template_image);
            g_key_file_set_string(keyfile, "Project", "template", b64);
            g_free(b64);
        }
        g_key_file_set_boolean(keyfile, "Project", "deep_fry", gtk_toggle_button_get_active(self->deep_fry_button));
        g_key_file_set_boolean(keyfile, "Project", "cinematic", gtk_toggle_button_get_active(self->cinematic_button));
        
        int i = 0;
        for (GList *l = self->layers; l != NULL; l = l->next, i++) {
            ImageLayer *layer = (ImageLayer *)l->data;
            gchar group[32]; g_snprintf(group, sizeof(group), "Layer%d", i);
            g_key_file_set_integer(keyfile, group, "type", layer->type);
            g_key_file_set_double(keyfile, group, "x", layer->x);
            g_key_file_set_double(keyfile, group, "y", layer->y);
            g_key_file_set_double(keyfile, group, "scale", layer->scale);
            g_key_file_set_double(keyfile, group, "rotation", layer->rotation);
            g_key_file_set_double(keyfile, group, "opacity", layer->opacity);
            g_key_file_set_integer(keyfile, group, "blend_mode", layer->blend_mode);

            if (layer->type == LAYER_TYPE_TEXT && layer->text) {
                g_key_file_set_string(keyfile, group, "text", layer->text);
                g_key_file_set_double(keyfile, group, "font_size", layer->font_size);
            } else if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf) {
                gchar *b64 = pixbuf_to_base64(layer->pixbuf);
                g_key_file_set_string(keyfile, group, "pixbuf", b64);
                g_free(b64);
            }
        }
        g_key_file_set_integer(keyfile, "Project", "layer_count", i);
        g_key_file_save_to_file(keyfile, g_file_get_path(file), NULL);
        g_key_file_free(keyfile); g_object_unref(file);
    }
}

void myapp_window_save_project(MemeWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Memerist Project");
    gtk_file_filter_add_pattern(filter, "*.meme");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_initial_name(dialog, "project.meme");
    gtk_file_dialog_save(dialog, GTK_WINDOW(self), NULL, on_save_project_response, self);
    g_object_unref(filter); g_object_unref(filters); g_object_unref(dialog);
}

static void on_project_load_contents_finished(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GFile *file = G_FILE(source_object);
    MemeWindow *self = MEME_WINDOW(user_data);
    char *contents = NULL; gsize length = 0; GError *error = NULL;
    
    if(g_file_load_contents_finish(file, res, &contents, &length, NULL, &error)){
        GKeyFile *keyfile = g_key_file_new();
        if(g_key_file_load_from_data(keyfile, contents, length, G_KEY_FILE_NONE, &error)){
            on_clear_clicked(self);
            gchar *b64_template = g_key_file_get_string(keyfile, "Project", "template", NULL);
            if(b64_template){ self->template_image = base64_to_pixbuf(b64_template); g_free(b64_template); }
            
            gtk_toggle_button_set_active(self->deep_fry_button, g_key_file_get_boolean(keyfile, "Project", "deep_fry", NULL));
            gtk_toggle_button_set_active(self->cinematic_button, g_key_file_get_boolean(keyfile, "Project", "cinematic", NULL));
            
            int count = g_key_file_get_integer(keyfile, "Project", "layer_count", NULL);
            for(int i = 0; i < count; i++){
                gchar group[32]; g_snprintf(group, sizeof(group), "Layer%d", i);
                ImageLayer *layer = g_new0(ImageLayer, 1);
                layer->type = g_key_file_get_integer(keyfile, group, "type", NULL);
                layer->x = g_key_file_get_double(keyfile, group, "x", NULL);
                layer->y = g_key_file_get_double(keyfile, group, "y", NULL);
                layer->scale = g_key_file_get_double(keyfile, group, "scale", NULL);
                layer->rotation = g_key_file_get_double(keyfile, group, "rotation", NULL);
                layer->opacity = g_key_file_get_double(keyfile, group, "opacity", NULL);
                layer->blend_mode = g_key_file_get_double(keyfile, group, "blend_mode", NULL);
                
                if(layer->type == LAYER_TYPE_TEXT){
                    layer->text = g_key_file_get_string(keyfile,group, "text", NULL);
                    layer->font_size = g_key_file_get_double(keyfile, group, "font_size", NULL);
                }else if(layer->type == LAYER_TYPE_IMAGE){
                    gchar *b64 = g_key_file_get_string(keyfile, group, "pixbuf", NULL);
                    if(b64){
                        layer->pixbuf = base64_to_pixbuf(b64);
                        layer->width = gdk_pixbuf_get_width(layer->pixbuf);
                        layer->height = gdk_pixbuf_get_height(layer->pixbuf);
                        g_free(b64);
                    }
                }
                self->layers = g_list_append(self->layers, layer);
            }
            if(self->template_image){
                gtk_stack_set_visible_child_name(self->content_stack, "content");
                gtk_widget_set_sensitive(GTK_WIDGET(self->add_text_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->export_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->clear_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->add_image_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), TRUE);
                render_meme(self);
            }
        }
        g_key_file_free(keyfile); g_free(contents);
    }
    g_object_unref(file);
}

static void on_load_project_response(GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    GFile *file = gtk_file_dialog_open_finish(dialog, r, NULL);
    if(file) g_file_load_contents_async(file, NULL, on_project_load_contents_finished, self);
}

void on_load_project_clicked(MemeWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Memerist Project");
    gtk_file_filter_add_pattern(filter, "*.meme");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_open(dialog, GTK_WINDOW(self), NULL, on_load_project_response, self);
    g_object_unref(filter); g_object_unref(filters); g_object_unref(dialog);
}

static void on_load_image_response(GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    GFile *file = gtk_file_dialog_open_finish(dialog, r, NULL);
    if (file) {  
        char *path = g_file_get_path(file);
        g_clear_object(&self->template_image);
        self->template_image = gdk_pixbuf_new_from_file(path, NULL);
        if (self->layers) { meme_layer_list_free(self->layers); self->layers = NULL; self->selected_layer = NULL; }
        free_history_stack(&self->undo_stack); free_history_stack(&self->redo_stack);
        if (self->template_image) {
            gtk_stack_set_visible_child_name(self->content_stack, "content");
            gtk_widget_set_sensitive(GTK_WIDGET(self->add_text_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->export_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->clear_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->add_image_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->deep_fry_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->cinematic_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->global_filters_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_in), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->zoom_out), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(self->copy_clipboard_button), TRUE);
            self->zoom_level = 1.0; apply_zoom(self); render_meme(self);
        }   
        g_free(path); g_object_unref(file);
    }
}

void on_load_image_clicked(MemeWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_open(dialog, GTK_WINDOW(self), NULL, on_load_image_response, self);
    g_object_unref(dialog);
}

static void on_add_image_response(GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    GFile *file = gtk_file_dialog_open_finish(dialog, r, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        ImageLayer *new_layer = g_new0(ImageLayer, 1);
        new_layer->pixbuf = gdk_pixbuf_new_from_file(path, NULL);
        if (new_layer->pixbuf) {
            push_undo(self);
            new_layer->width = gdk_pixbuf_get_width(new_layer->pixbuf);
            new_layer->height = gdk_pixbuf_get_height(new_layer->pixbuf);
            new_layer->x=0.5; new_layer->y=0.5; new_layer->scale=1.0; new_layer->opacity=1.0;
            self->layers = g_list_append(self->layers, new_layer);
            self->selected_layer = new_layer;
            sync_ui_with_layer(self); render_meme(self);
        }
        g_free(path); g_object_unref(file);
    }
}

void on_add_image_clicked(MemeWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_open(dialog, GTK_WINDOW(self), NULL, on_add_image_response, self);
    g_object_unref(dialog);
}

static void on_export_file_response(GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    GFile *file = gtk_file_dialog_save_finish(dialog, r, NULL);

    if (file && self->final_meme) {
        GdkPixbuf *save = self->final_meme;
        if (gtk_toggle_button_get_active(self->crop_mode_button)) {
            int iw = gdk_pixbuf_get_width(save); int ih = gdk_pixbuf_get_height(save);
            save = gdk_pixbuf_new_subpixbuf(save, self->crop_x*iw, self->crop_y*ih, self->crop_w*iw, self->crop_h*ih);
        } else g_object_ref(save);
        
        const char *format = g_object_get_data(G_OBJECT(dialog), "export-format");
        if (!format) format = "png";
        if (g_strcmp0(format, "jpeg") == 0 && gdk_pixbuf_get_has_alpha(save)) {
            int w = gdk_pixbuf_get_width(save);
            int h = gdk_pixbuf_get_height(save);
            GdkPixbuf *flat = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
            gdk_pixbuf_fill(flat, 0xFFFFFFFF); // Solid white
            gdk_pixbuf_composite(save, flat, 0, 0, w, h, 0, 0, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
            g_object_unref(save);
            save = flat;
        }
        GError *error = NULL;
        GFileOutputStream *stream = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
        if (stream) {
            if (g_strcmp0(format, "jpeg") == 0) {
                gdk_pixbuf_save_to_stream(save, G_OUTPUT_STREAM(stream), format, NULL, &error, "quality", "100", NULL);
            } else {
                gdk_pixbuf_save_to_stream(save, G_OUTPUT_STREAM(stream), format, NULL, &error, NULL);
            }
            g_output_stream_close(G_OUTPUT_STREAM(stream), NULL, NULL);
            g_object_unref(stream);
        }
        g_object_unref(save);
        g_object_unref(file);
    }
}

static void on_format_chosen(GObject *s, GAsyncResult *r, gpointer d) {
    AdwAlertDialog *alert = ADW_ALERT_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    const char *choice = adw_alert_dialog_choose_finish(alert, r);

    if (g_strcmp0(choice, "cancel") == 0 || !choice) return;

    const char *ext = ".png";
    if (g_strcmp0(choice, "jpeg") == 0) ext = ".jpg";
    else if (g_strcmp0(choice, "webp") == 0) ext = ".webp";

    char *filename = g_strdup_printf("meme%s", ext);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_initial_name(dialog, filename);

    g_object_set_data(G_OBJECT(dialog), "export-format", (gpointer)choice);

    gtk_file_dialog_save(dialog, GTK_WINDOW(self), NULL, on_export_file_response, self);

    g_free(filename);
    g_object_unref(dialog);
}

void on_export_clicked(MemeWindow *self) {
    if (!self->final_meme) return;

    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new("Export Format", "Choose an image format."));
    adw_alert_dialog_add_responses(dialog,
        "cancel", "Cancel",
        "png", "PNG",
        "jpeg", "JPG",
        "webp", "WebP",
        NULL);

    adw_alert_dialog_set_response_appearance(dialog, "png", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_response_appearance(dialog, "cancel", ADW_RESPONSE_DESTRUCTIVE);

    adw_alert_dialog_choose(dialog, GTK_WIDGET(self), NULL, on_format_chosen, self);
}
