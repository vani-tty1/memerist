#include "meme-fileio.h"
#include "adwaita.h"
#include "meme-canvas.h"
#include <glib/gstdio.h>
#include <gio/gio.h>

typedef struct {
    GFile *dest_file;
    GdkPixbufAnimation *anim;
    GList *layers_copy;
    gboolean cinematic;
    gboolean deepfry;
} GifExportData;
// async gif handling functions, fucking hell why is it so hard to do async
// work
static void gif_export_data_free(gpointer data) {
    GifExportData *ctx = (GifExportData *)data;
    g_clear_object(&ctx->dest_file);
    g_clear_object(&ctx->anim);
    if (ctx->layers_copy) {
        meme_layer_list_free(ctx->layers_copy);
    }
    g_free(ctx);
}

static void export_gif_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    GifExportData *ctx = (GifExportData *)task_data;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GdkPixbufAnimationIter *iter = gdk_pixbuf_animation_get_iter(ctx->anim, NULL);
    int frame_count = 0;
    GString *im_args = g_string_new("magick -loop 0 ");

    do {
        GdkPixbuf *frame = gdk_pixbuf_animation_iter_get_pixbuf(iter);
        int delay_ms = gdk_pixbuf_animation_iter_get_delay_time(iter);


        // Render composite using the copied layers and flags
        GdkPixbuf *comp = meme_render_composite(frame, ctx->layers_copy, ctx->cinematic, ctx->deepfry);

        char tmp_path[64];
        snprintf(tmp_path, sizeof(tmp_path), "/tmp/meme_frame_%04d.png", frame_count++);
        gdk_pixbuf_save(comp, tmp_path, "png", NULL, NULL);
        g_object_unref(comp);

        g_string_append_printf(im_args, "-delay %d %s ", delay_ms / 10, tmp_path);
    } while (gdk_pixbuf_animation_iter_advance(iter, NULL) && frame_count < 200);
#pragma GCC diagnostic pop
    char *dest_path = g_file_get_path(ctx->dest_file);
    g_string_append(im_args, "-layers optimize ");
    g_string_append_printf(im_args, "%s", dest_path);

    g_spawn_command_line_sync(im_args->str, NULL, NULL, NULL, NULL);

    for (int i = 0; i < frame_count; i++) {
        char tmp_path[64];
        snprintf(tmp_path, sizeof(tmp_path), "/tmp/meme_frame_%04d.png", i);
        g_unlink(tmp_path);
    }

    g_free(dest_path);
    g_string_free(im_args, TRUE);
    g_object_unref(iter);

    g_task_return_boolean(task, TRUE);
}

static void on_gif_export_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    MemeWindow *self = MEME_WINDOW(user_data);
    GError *error = NULL;

    gtk_widget_set_visible(GTK_WIDGET(self->export_loading_screen), FALSE);

    if (g_task_propagate_boolean(G_TASK(res), &error)) {
        AdwToast *toast = adw_toast_new("GIF exported successfully!");
        adw_toast_overlay_add_toast(self->copy_clip_feedback, toast);
    } else {
        char *err_msg = g_strdup_printf("Failed to export GIF: %s", error->message);
        AdwToast *toast = adw_toast_new(err_msg);
        adw_toast_overlay_add_toast(self->copy_clip_feedback, toast);
        g_free(err_msg);
        g_error_free(error);
    }
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


static void 
encode_base64_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    GdkPixbuf *pixbuf = GDK_PIXBUF(task_data);
    gchar *buffer = NULL;
    gsize buffer_size = 0;

    if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size, "png", NULL, NULL)) {
        gchar *base64 = g_base64_encode((const guchar *)buffer, buffer_size);
        g_free(buffer);
        g_task_return_pointer(task, base64, g_free);
    } else {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to encode");
    }
}

static void 
pixbuf_to_base64_async (GdkPixbuf *pixbuf, GAsyncReadyCallback callback, gpointer user_data) {
    GTask *task = g_task_new(NULL, NULL, callback, user_data);
    g_task_set_task_data(task, g_object_ref(pixbuf), g_object_unref);
    g_task_run_in_thread(task, encode_base64_thread);
    g_object_unref(task);
}

static void
on_base64_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    EncodeCtx *ctx = user_data;
    GError *error = NULL;
    gchar *base64 = g_task_propagate_pointer (G_TASK (res), &error);

    if (base64) {
        g_key_file_set_string (ctx->save_ctx->keyfile, ctx->group, ctx->key, base64);
        g_free (base64);
    } else {
        g_warning ("Failed to encode image: %s", error ? error->message : "unknown");
        g_clear_error (&error);
    }

    g_free (ctx->group);
    g_free (ctx->key);
    g_free (ctx);

    ctx->save_ctx->pending--;
    if (ctx->save_ctx->pending == 0) {
        gchar *path = g_file_get_path (ctx->save_ctx->file);
        g_key_file_save_to_file (ctx->save_ctx->keyfile, path, NULL);
        g_free (path);
        g_key_file_free (ctx->save_ctx->keyfile);
        g_object_unref (ctx->save_ctx->file);
        g_free (ctx->save_ctx);
    }
}

static void on_save_project_response (GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
    MemeWindow *self = MEME_WINDOW (d);
    GFile *file = gtk_file_dialog_save_finish (dialog, r, NULL);
    if (!file) return;

    GKeyFile *keyfile = g_key_file_new ();

    // Count async encodes needed upfront
    int encode_count = (self->template_image != NULL) ? 1 : 0;
    for (GList *l = self->layers; l != NULL; l = l->next) {
        ImageLayer *layer = l->data;
        if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf)
            encode_count++;
    }

    SaveCtx *save_ctx = g_new0 (SaveCtx, 1);
    save_ctx->file    = file;
    save_ctx->keyfile = keyfile;
    save_ctx->pending = encode_count;

    g_key_file_set_boolean (keyfile, "Project", "deep_fry",  gtk_toggle_button_get_active (self->deep_fry_button));
    g_key_file_set_boolean (keyfile, "Project", "cinematic", gtk_toggle_button_get_active (self->cinematic_button));

    int i = 0;
    for (GList *l = self->layers; l != NULL; l = l->next, i++) {
        ImageLayer *layer = l->data;
        gchar group[32];
        g_snprintf (group, sizeof (group), "Layer%d", i);

        g_key_file_set_integer (keyfile, group, "type",       layer->type);
        g_key_file_set_double  (keyfile, group, "x",          layer->x);
        g_key_file_set_double  (keyfile, group, "y",          layer->y);
        g_key_file_set_double  (keyfile, group, "scale",      layer->scale);
        g_key_file_set_double  (keyfile, group, "rotation",   layer->rotation);
        g_key_file_set_double  (keyfile, group, "opacity",    layer->opacity);
        g_key_file_set_integer (keyfile, group, "blend_mode", layer->blend_mode);

        if (layer->type == LAYER_TYPE_TEXT && layer->text) {
            g_key_file_set_string (keyfile, group, "text",      layer->text);
            g_key_file_set_double (keyfile, group, "font_size", layer->font_size);
        } else if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf) {
            EncodeCtx *ctx  = g_new0 (EncodeCtx, 1);
            ctx->save_ctx   = save_ctx;
            ctx->group      = g_strdup (group);
            ctx->key        = g_strdup ("pixbuf");
            pixbuf_to_base64_async (layer->pixbuf, on_base64_ready, ctx);
        }
    }
    g_key_file_set_integer (keyfile, "Project", "layer_count", i);

    if (self->template_image) {
        EncodeCtx *ctx  = g_new0 (EncodeCtx, 1);
        ctx->save_ctx   = save_ctx;
        ctx->group      = g_strdup ("Project");
        ctx->key        = g_strdup ("template");
        pixbuf_to_base64_async (self->template_image, on_base64_ready, ctx);
    }

    if (encode_count == 0) {
        gchar *path = g_file_get_path (file);
        g_key_file_save_to_file (keyfile, path, NULL);
        g_free (path);
        g_key_file_free (keyfile);
        g_object_unref (file);
        g_free (save_ctx);
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
                layer->blend_mode = g_key_file_get_integer(keyfile, group, "blend_mode", NULL);
                
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
        if (g_str_has_suffix(path, ".gif")) {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            self->template_anim = gdk_pixbuf_animation_new_from_file(path, NULL);
            self->template_image = gdk_pixbuf_animation_get_static_image(self->template_anim);
            #pragma GCC diagnostic pop
            g_object_ref(self->template_image);
        } else {
            g_clear_object(&self->template_anim);
            self->template_image = gdk_pixbuf_new_from_file(path, NULL);
        }
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
    if (!file) return;

    const char *format = g_object_get_data(G_OBJECT(dialog), "export-format");
    if (!format) format = "png";

    if (g_strcmp0(format, "gif") == 0 && self->template_anim) {
		gchar *magick_path = g_find_program_in_path("magick");
        if (!magick_path) {
            AdwToast *toast = adw_toast_new("ImageMagick is required to export GIFs.");
            adw_toast_overlay_add_toast(self->copy_clip_feedback, toast);
            g_object_unref(file);
            return;
        }
        g_free(magick_path);
        gtk_widget_set_visible(GTK_WIDGET(self->export_loading_screen), TRUE);

        GifExportData *data = g_new0(GifExportData, 1);
        data->dest_file = g_object_ref(file);

        AdwToast *starting_toast = adw_toast_new("Exporting GIF... This may take a moment.");
        adw_toast_set_timeout(starting_toast, 3);
        adw_toast_overlay_add_toast(self->copy_clip_feedback, starting_toast);

        data->dest_file = g_object_ref(file);
        data->anim = g_object_ref(self->template_anim);
        data->layers_copy = meme_layer_list_copy(self->layers);
        data->cinematic = gtk_toggle_button_get_active(self->cinematic_button);
        data->deepfry = gtk_toggle_button_get_active(self->deep_fry_button);

        GTask *task = g_task_new(self, NULL, on_gif_export_ready, self);
        g_task_set_task_data(task, data, gif_export_data_free);

        g_task_run_in_thread(task, export_gif_thread);

        g_object_unref(task);
        g_object_unref(file);
        return;
    }

    if (self->final_meme) {
        GdkPixbuf *save = self->final_meme;
        if (gtk_toggle_button_get_active(self->crop_mode_button)) {
            int iw = gdk_pixbuf_get_width(save); int ih = gdk_pixbuf_get_height(save);
            save = gdk_pixbuf_new_subpixbuf(save, self->crop_x*iw, self->crop_y*ih, self->crop_w*iw, self->crop_h*ih);
        } else {
            g_object_ref(save);
        }
        
        // Remove alpha channel for JPEGs
        if (g_strcmp0(format, "jpeg") == 0 && gdk_pixbuf_get_has_alpha(save)) {
            int w = gdk_pixbuf_get_width(save);
            int h = gdk_pixbuf_get_height(save);
            GdkPixbuf *flat = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
            gdk_pixbuf_fill(flat, 0xFFFFFFFF);
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
    }
    g_object_unref(file);
}

static void on_format_chosen(GObject *s, GAsyncResult *r, gpointer d) {
    AdwAlertDialog *alert = ADW_ALERT_DIALOG(s);
    MemeWindow *self = MEME_WINDOW(d);
    const char *choice = adw_alert_dialog_choose_finish(alert, r);

    if (g_strcmp0(choice, "cancel") == 0 || !choice) return;

    const char *ext = ".png";
    if (g_strcmp0(choice, "jpeg") == 0) ext = ".jpg";
    else if (g_strcmp0(choice, "webp") == 0) ext = ".webp";
    else if (g_strcmp0(choice, "gif") == 0) ext = ".gif";

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
        "gif", "GIF",
        "jpeg", "JPG",
        "webp", "WebP",
        NULL);

    adw_alert_dialog_set_response_appearance(dialog, "png", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_response_appearance(dialog, "cancel", ADW_RESPONSE_DESTRUCTIVE);

    adw_alert_dialog_choose(dialog, GTK_WIDGET(self), NULL, on_format_chosen, self);
}
