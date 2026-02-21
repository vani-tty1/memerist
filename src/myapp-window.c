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





#include "myapp-window.h"
#include "adwaita.h"
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "meme-core.h"
#include "meme-renderer.h"

struct _MyappWindow {
  AdwApplicationWindow parent_instance;
  AdwPreferencesGroup *layer_group;
  AdwPreferencesGroup *templates_group;
  AdwPreferencesGroup *transform_group;
  AdwOverlaySplitView *split_view;
  GtkStack        *content_stack;
  GtkImage        *meme_preview;
  GtkImage        *add_text_button;
  AdwEntryRow     *layer_text_entry;
  AdwActionRow    *layer_font_size_row;
  GtkSpinButton   *layer_font_size;

  GtkButton       *export_button;
  GtkButton       *load_image_button;
  GtkButton       *clear_button;
  GtkButton       *add_image_button;
  GtkButton       *import_template_button;
  GtkButton       *delete_template_button;
  GtkToggleButton *deep_fry_button;
  GtkFlowBox      *template_gallery;

  GtkToggleButton *cinematic_button;
  GtkScale        *layer_opacity_scale;
  GtkScale        *layer_rotation_scale;
  AdwComboRow     *blend_mode_row;
  GtkButton       *delete_layer_button;

  GdkPixbuf       *template_image;
  GdkPixbuf       *final_meme;

  GList           *layers;
  ImageLayer      *selected_layer;

  GList           *undo_stack;
  GList           *redo_stack;

  DragType        drag_type;
  GtkGestureDrag *drag_gesture;

  GtkToggleButton *crop_mode_button;
  GtkButton *rotate_left_button;
  GtkButton *rotate_right_button;
  GtkButton *flip_h_button;
  GtkButton *flip_v_button;
  GtkButton *crop_square_button;
  GtkButton *crop_43_button;
  GtkButton *crop_169_button;
   
  double drag_start_x;
  double drag_start_y;
  double drag_obj_start_x;
  double drag_obj_start_y;
  double drag_obj_start_scale; 
  double drag_obj_start_h;
  GtkButton *save_project_button;
  GtkButton *load_project_button;

  ResizeHandle active_crop_handle;

  double crop_x;
  double crop_y;
  double crop_w;
  double crop_h;
};

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

static void sync_ui_with_layer(MyappWindow *self);
static void render_meme (MyappWindow *self);
static void populate_template_gallery (MyappWindow *self);
static void on_clear_clicked (MyappWindow *self);


static gchar *pixbuf_to_base64 (GdkPixbuf *pixbuf) {
    if (!pixbuf) return NULL;
    gchar *buffer = NULL;
    gsize buffer_size = 0;
    if (gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &buffer_size, "png", NULL, NULL)) {
        gchar *base64 = g_base64_encode ((const guchar *)buffer, buffer_size);
        g_free (buffer);
        return base64;
    }
    return NULL;
}

static GdkPixbuf *base64_to_pixbuf (const gchar *base64) {
    if (!base64) return NULL;
    gsize out_len = 0;
    guchar *decoded = g_base64_decode (base64, &out_len);
    if (!decoded) return NULL;
    GInputStream *stream = g_memory_input_stream_new_from_data (decoded, out_len, g_free);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
    g_object_unref (stream);
    return pixbuf;
}

static void on_save_project_response (GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
    MyappWindow *self = MYAPP_WINDOW (d);
    GFile *file = gtk_file_dialog_save_finish (dialog, r, NULL);
    
    if (file) {
        GKeyFile *keyfile = g_key_file_new ();

        // Save global properties
        if (self->template_image) {
            gchar *b64 = pixbuf_to_base64 (self->template_image);
            g_key_file_set_string (keyfile, "Project", "template", b64);
            g_free (b64);
        }
        g_key_file_set_boolean (keyfile, "Project", "deep_fry", gtk_toggle_button_get_active (self->deep_fry_button));
        g_key_file_set_boolean (keyfile, "Project", "cinematic", gtk_toggle_button_get_active (self->cinematic_button));
        
        // Save layers
        int i = 0;
        for (GList *l = self->layers; l != NULL; l = l->next, i++) {
            ImageLayer *layer = (ImageLayer *)l->data;
            gchar group[32];
            g_snprintf (group, sizeof(group), "Layer%d", i);

            g_key_file_set_integer (keyfile, group, "type", layer->type);
            g_key_file_set_double (keyfile, group, "x", layer->x);
            g_key_file_set_double (keyfile, group, "y", layer->y);
            g_key_file_set_double (keyfile, group, "scale", layer->scale);
            g_key_file_set_double (keyfile, group, "rotation", layer->rotation);
            g_key_file_set_double (keyfile, group, "opacity", layer->opacity);
            g_key_file_set_integer (keyfile, group, "blend_mode", layer->blend_mode);

            if (layer->type == LAYER_TYPE_TEXT && layer->text) {
                g_key_file_set_string (keyfile, group, "text", layer->text);
                g_key_file_set_double (keyfile, group, "font_size", layer->font_size);
            } else if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf) {
                gchar *b64 = pixbuf_to_base64 (layer->pixbuf);
                g_key_file_set_string (keyfile, group, "pixbuf", b64);
                g_free (b64);
            }
        }
        g_key_file_set_integer (keyfile, "Project", "layer_count", i);

        g_key_file_save_to_file (keyfile, g_file_get_path (file), NULL);
        g_key_file_free (keyfile);
        g_object_unref (file);
    }
}

static void on_load_project_response (GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
    MyappWindow *self = MYAPP_WINDOW (d);
    GFile *file = gtk_file_dialog_open_finish (dialog, r, NULL);
    
    if (file) {
        GKeyFile *keyfile = g_key_file_new ();
        if (g_key_file_load_from_file (keyfile, g_file_get_path (file), G_KEY_FILE_NONE, NULL)) {
            on_clear_clicked (self); // Wipe current state

            gchar *b64_template = g_key_file_get_string (keyfile, "Project", "template", NULL);
            if (b64_template) {
                self->template_image = base64_to_pixbuf (b64_template);
                g_free (b64_template);
            }

            gtk_toggle_button_set_active (self->deep_fry_button, g_key_file_get_boolean (keyfile, "Project", "deep_fry", NULL));
            gtk_toggle_button_set_active (self->cinematic_button, g_key_file_get_boolean (keyfile, "Project", "cinematic", NULL));

            int count = g_key_file_get_integer (keyfile, "Project", "layer_count", NULL);
            for (int i = 0; i < count; i++) {
                gchar group[32];
                g_snprintf (group, sizeof(group), "Layer%d", i);

                ImageLayer *layer = g_new0 (ImageLayer, 1);
                layer->type = g_key_file_get_integer (keyfile, group, "type", NULL);
                layer->x = g_key_file_get_double (keyfile, group, "x", NULL);
                layer->y = g_key_file_get_double (keyfile, group, "y", NULL);
                layer->scale = g_key_file_get_double (keyfile, group, "scale", NULL);
                layer->rotation = g_key_file_get_double (keyfile, group, "rotation", NULL);
                layer->opacity = g_key_file_get_double (keyfile, group, "opacity", NULL);
                layer->blend_mode = g_key_file_get_integer (keyfile, group, "blend_mode", NULL);

                if (layer->type == LAYER_TYPE_TEXT) {
                    layer->text = g_key_file_get_string (keyfile, group, "text", NULL);
                    layer->font_size = g_key_file_get_double (keyfile, group, "font_size", NULL);
                } else if (layer->type == LAYER_TYPE_IMAGE) {
                    gchar *b64 = g_key_file_get_string (keyfile, group, "pixbuf", NULL);
                    if (b64) {
                        layer->pixbuf = base64_to_pixbuf (b64);
                        layer->width = gdk_pixbuf_get_width (layer->pixbuf);
                        layer->height = gdk_pixbuf_get_height (layer->pixbuf);
                        g_free (b64);
                    }
                }
                self->layers = g_list_append (self->layers, layer);
            }

            if (self->template_image) {
                gtk_stack_set_visible_child_name (self->content_stack, "content");
                gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->crop_mode_button), TRUE);
                gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
                render_meme (self);
            }
        }
        g_key_file_free (keyfile);
        g_object_unref (file);
    }
}

static void on_save_project_clicked (MyappWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    GtkFileFilter *filter = gtk_file_filter_new ();
    
    gtk_file_filter_set_name (filter, "Memerist Project");
    gtk_file_filter_add_pattern (filter, "*.meme");
    
    GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
    
    gtk_file_dialog_set_initial_name (dialog, "project.meme");
    gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_save_project_response, self);
    
    g_object_unref (filter);
    g_object_unref (filters);
}

static void on_load_project_clicked (MyappWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    GtkFileFilter *filter = gtk_file_filter_new ();
    
    gtk_file_filter_set_name (filter, "Memerist Project");
    gtk_file_filter_add_pattern (filter, "*.meme");
    
    GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
    
    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_load_project_response, self);
    
    g_object_unref (filter);
    g_object_unref (filters);
}
















static void free_history_stack (GList **stack) {
  GList *l;
  for (l = *stack; l != NULL; l = l->next) {
    meme_layer_list_free ((GList *)l->data);
  }
  g_list_free (*stack);
  *stack = NULL;
}

static void push_undo (MyappWindow *self) {
  free_history_stack (&self->redo_stack);
  if (g_list_length (self->undo_stack) >= 20) {
      GList *last = g_list_last (self->undo_stack);
      meme_layer_list_free ((GList *)last->data);
      self->undo_stack = g_list_delete_link (self->undo_stack, last);
  }
  self->undo_stack = g_list_prepend (self->undo_stack, meme_layer_list_copy (self->layers));
}

static void perform_undo (MyappWindow *self) {
  if (!self->undo_stack) return;
  self->redo_stack = g_list_prepend (self->redo_stack, self->layers);
  self->layers = (GList *)self->undo_stack->data;
  self->undo_stack = g_list_delete_link (self->undo_stack, self->undo_stack);
  self->selected_layer = NULL;
  sync_ui_with_layer (self);
  render_meme (self);
}

static void perform_redo (MyappWindow *self) {
  if (!self->redo_stack) return;
  self->undo_stack = g_list_prepend (self->undo_stack, self->layers);
  self->layers = (GList *)self->redo_stack->data;
  self->redo_stack = g_list_delete_link (self->redo_stack, self->redo_stack);
  self->selected_layer = NULL;
  sync_ui_with_layer (self);
  render_meme (self);
}


static void render_meme (MyappWindow *self) {
    if (!self->template_image) return;

    
    if (self->final_meme) g_object_unref(self->final_meme);
    self->final_meme = meme_render_composite(
        self->template_image,
        self->layers,
        gtk_toggle_button_get_active(self->cinematic_button),
        gtk_toggle_button_get_active(self->deep_fry_button)
    );

    GdkTexture *tex = meme_render_editor_overlay(
        self->final_meme,
        self->layers,
        self->selected_layer,
        gtk_toggle_button_get_active(self->crop_mode_button),
        self->crop_x, self->crop_y, self->crop_w, self->crop_h
    );

    gtk_image_set_from_paintable(self->meme_preview, GDK_PAINTABLE(tex));
    g_object_unref(tex);
}

static void on_text_changed (MyappWindow *self) { if (self->template_image) render_meme (self); }
static void on_deep_fry_toggled (GtkToggleButton *btn, MyappWindow *self) { render_meme (self); }

static void on_layer_text_changed (MyappWindow *self) {
  if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
      g_free (self->selected_layer->text);
      self->selected_layer->text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->layer_text_entry)));
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

static void update_template_image (MyappWindow *self, GdkPixbuf *new_pixbuf) {
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

static void sync_ui_with_layer(MyappWindow *self) {
    gboolean sensitive = (self->selected_layer != NULL);
    gboolean is_text = (sensitive && self->selected_layer->type == LAYER_TYPE_TEXT);

    g_signal_handlers_block_by_func(self->layer_opacity_scale, on_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_rotation_scale, on_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_text_entry, on_layer_text_changed, self);
    g_signal_handlers_block_by_func(self->layer_font_size, on_layer_text_changed, self);

    if (sensitive) {
        gtk_range_set_value(GTK_RANGE(self->layer_opacity_scale), self->selected_layer->opacity);
        gtk_range_set_value(GTK_RANGE(self->layer_rotation_scale), self->selected_layer->rotation);
        adw_combo_row_set_selected(self->blend_mode_row, self->selected_layer->blend_mode);
        if (is_text) {
             gtk_editable_set_text(GTK_EDITABLE(self->layer_text_entry), self->selected_layer->text);
             gtk_spin_button_set_value(self->layer_font_size, self->selected_layer->font_size);
        }
    }
    gtk_widget_set_visible(GTK_WIDGET(self->layer_text_entry), is_text);
    gtk_widget_set_visible(GTK_WIDGET(self->layer_font_size_row), is_text);
    gtk_widget_set_sensitive(GTK_WIDGET(self->layer_opacity_scale), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->layer_rotation_scale), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->blend_mode_row), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(self->delete_layer_button), sensitive);

    g_signal_handlers_unblock_by_func(self->layer_opacity_scale, on_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_rotation_scale, on_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_text_entry, on_layer_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_font_size, on_layer_text_changed, self);
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


static void on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self) {
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
        case HANDLE_NONE: // Explicitly handle this to satisfy -Wswitch-enum
        default: cursor = NULL; break;
    }
     gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), cursor);
     return;
  }

  // Layer hover
  GList *l;
  gboolean found = FALSE;
  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
      ImageLayer *layer = (ImageLayer *)l->data;
      double half_w = (layer->width * layer->scale) / (2.0 * img_w);
      double half_h = (layer->height * layer->scale) / (2.0 * img_h);
      if (ix >= layer->x - half_w && ix <= layer->x + half_w &&
          iy >= layer->y - half_h && iy <= layer->y + half_h) {
          gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "move");
          found = TRUE;
          break;
      }
  }
  if (!found) gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
}

static void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self) {
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
         self->drag_start_x = ix * img_w; self->drag_start_y = iy * img_h; // Abs pixel coords for resize logic
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

static void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self) {
  double dx, dy, img_w, img_h;
  double ww, wh, wr, hr, s;
  if (self->drag_type == DRAG_TYPE_NONE || !self->template_image) return;

  img_w = gdk_pixbuf_get_width(self->template_image);
  img_h = gdk_pixbuf_get_height(self->template_image);
  ww = gtk_widget_get_width(GTK_WIDGET(self->meme_preview));
  wh = gtk_widget_get_height(GTK_WIDGET(self->meme_preview));
  wr = ww/img_w; hr = wh/img_h;
  s = (wr < hr) ? wr : hr;

  dx = (offset_x / s) / img_w;
  dy = (offset_y / s) / img_h;

  if (self->drag_type == DRAG_TYPE_CROP_MOVE) {
      self->crop_x = CLAMP(self->drag_obj_start_x + dx, 0.0, 1.0 - self->crop_w);
      self->crop_y = CLAMP(self->drag_obj_start_y + dy, 0.0, 1.0 - self->crop_h);
  }
  else if (self->drag_type == DRAG_TYPE_CROP_RESIZE) {
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
  }
  else if (self->drag_type == DRAG_TYPE_IMAGE_MOVE && self->selected_layer) {
      self->selected_layer->x = CLAMP(self->drag_obj_start_x + dx, 0.0, 1.0);
      self->selected_layer->y = CLAMP(self->drag_obj_start_y + dy, 0.0, 1.0);
  }
  else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE && self->selected_layer) {
      double cx = self->selected_layer->x * img_w, cy = self->selected_layer->y * img_h;
      double sdx = self->drag_start_x - cx, sdy = self->drag_start_y - cy;
      double cdx = (self->drag_start_x + offset_x/s) - cx, cdy = (self->drag_start_y + offset_y/s) - cy;
      double dist_s = sqrt(sdx*sdx + sdy*sdy), dist_c = sqrt(cdx*cdx + cdy*cdy);
      if (dist_s > 5.0) self->selected_layer->scale = CLAMP(self->drag_obj_start_scale * (dist_c/dist_s), 0.1, 5.0);
  }
  render_meme(self);
}

static void on_drag_end (GtkGestureDrag *g, double x, double y, MyappWindow *self) { self->drag_type = DRAG_TYPE_NONE; }

//File Handling
static void on_load_image_response (GObject *s, GAsyncResult *r, gpointer d) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GFile *file = gtk_file_dialog_open_finish (dialog, r, NULL);
  if (file) {
      char *path = g_file_get_path (file);
      g_clear_object (&self->template_image);
      self->template_image = gdk_pixbuf_new_from_file (path, NULL);
      if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; self->selected_layer = NULL; }
      free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);
      if (self->template_image) {
          gtk_stack_set_visible_child_name (self->content_stack, "content");
          gtk_widget_set_sensitive(GTK_WIDGET(self->add_text_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->export_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->clear_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->add_image_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->deep_fry_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->cinematic_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), TRUE);
          gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), TRUE);
                    
          render_meme(self);
      }
      g_free (path); g_object_unref (file);
  }
}

static void on_load_image_clicked (MyappWindow *self) {
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_load_image_response, self);
}

static void on_add_image_response (GObject *s, GAsyncResult *r, gpointer d) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
    MyappWindow *self = MYAPP_WINDOW (d);
    GFile *file = gtk_file_dialog_open_finish (dialog, r, NULL);
    if (file) {
        char *path = g_file_get_path (file);
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

static void on_add_image_clicked (MyappWindow *self) {
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_add_image_response, self);
}

static void on_export_response (GObject *s, GAsyncResult *r, gpointer d) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GFile *file = gtk_file_dialog_save_finish (dialog, r, NULL);
  if (file && self->final_meme) {
      GdkPixbuf *save = self->final_meme;
      if (gtk_toggle_button_get_active(self->crop_mode_button)) {
          int iw = gdk_pixbuf_get_width(save); int ih = gdk_pixbuf_get_height(save);
          save = gdk_pixbuf_new_subpixbuf(save, self->crop_x*iw, self->crop_y*ih, self->crop_w*iw, self->crop_h*ih);
      } else {
          g_object_ref(save);
      }
      gdk_pixbuf_save (save, g_file_get_path (file), "png", NULL, NULL);
      g_object_unref (save); g_object_unref (file);
  }
}

static void on_export_clicked (MyappWindow *self) {
  if (!self->final_meme) return;
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_initial_name (dialog, "meme.png");
  gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_export_response, self);
}

static void on_clear_clicked (MyappWindow *self) {
  gtk_stack_set_visible_child_name (self->content_stack, "empty");
  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);
  if (self->layers) { meme_layer_list_free (self->layers); self->layers = NULL; }
  free_history_stack (&self->undo_stack); free_history_stack (&self->redo_stack);
  self->selected_layer = NULL;
  sync_ui_with_layer(self);
  gtk_image_clear (self->meme_preview);
  gtk_toggle_button_set_active (self->deep_fry_button, FALSE);
  gtk_toggle_button_set_active (self->cinematic_button, FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(self->crop_mode_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(self->save_project_button), FALSE);
}


static void myapp_window_finalize (GObject *object) {
  MyappWindow *self = MYAPP_WINDOW (object);
  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);
  g_clear_object (&self->drag_gesture);
  if (self->layers) meme_layer_list_free (self->layers);
  free_history_stack (&self->undo_stack);
  free_history_stack (&self->redo_stack);
  G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static void myapp_window_class_init (MyappWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = myapp_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/vani_tty1/memerist/myapp-window.ui");
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_group);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, templates_group);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, transform_group);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, meme_preview);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, content_stack);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_text_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_text_entry);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_font_size);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, layer_font_size_row);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, export_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_image_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, clear_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_image_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, import_template_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, delete_template_button);
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
}



static char *
get_user_template_dir (void) {
  return g_build_filename (g_get_user_data_dir (), "io.github.vani_tty1.memerist", "templates", NULL);
}

static gboolean
is_user_template (const char *path) {
  g_autofree char *user_dir = get_user_template_dir ();
  return g_str_has_prefix (path, user_dir);
}

static void
add_file_to_gallery (MyappWindow *self, const char *full_path) {
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

static void
scan_directory_for_templates (MyappWindow *self, const char *dir_path) {
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

static void
scan_resources_for_templates (MyappWindow *self) {
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

static void
populate_template_gallery (MyappWindow *self) {
  char *user_dir;
  gtk_flow_box_remove_all(self->template_gallery);
  
  scan_resources_for_templates (self);
  user_dir = get_user_template_dir ();
  g_mkdir_with_parents (user_dir, 0755);
  scan_directory_for_templates (self, user_dir);
  g_free (user_dir);
}

static void
on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self) {
  GtkWidget *image;
  const char *template_path;
  GError *error = NULL;

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
      render_meme (self);
  }
}

static void
on_import_template_response (GObject *s, GAsyncResult *r, gpointer d) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GFile *source_file, *dest_file;
  GError *error = NULL;
  char *filename, *user_dir_path, *dest_path;

  source_file = gtk_file_dialog_open_finish (dialog, r, &error);
  if (error) { g_error_free (error); return; }

  filename = g_file_get_basename (source_file);
  user_dir_path = get_user_template_dir ();
  g_mkdir_with_parents (user_dir_path, 0755);
  dest_path = g_build_filename (user_dir_path, filename, NULL);
  dest_file = g_file_new_for_path (dest_path);

  if (g_file_copy (source_file, dest_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
    add_file_to_gallery (self, dest_path);
  }
  g_free (filename); g_free (user_dir_path); g_free (dest_path);
  g_object_unref (source_file); g_object_unref (dest_file);
}

static void
on_import_template_clicked (MyappWindow *self) {
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Import Template");
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_import_template_response, self);
}

static void
on_delete_confirm_response (GObject *s, GAsyncResult *r, gpointer d) {
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

static void
on_delete_template_clicked (MyappWindow *self) {
  GtkAlertDialog *dialog = gtk_alert_dialog_new ("Delete this template?");
  gtk_alert_dialog_set_buttons (dialog, (const char *[]) {"Cancel", "Delete", NULL});
  gtk_alert_dialog_set_default_button (dialog, 1);
  gtk_alert_dialog_choose (dialog, GTK_WINDOW (self), NULL, on_delete_confirm_response, self);
}

static gboolean on_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, MyappWindow *self) {
  // Ctrl + Z = Undo
  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
    perform_undo (self);
    return TRUE;
  }
  // Ctrl + Y = Redo
  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) {
    perform_redo (self);
    return TRUE;
  }
  return FALSE;
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
  g_signal_connect_swapped (self->layer_text_entry, "changed", G_CALLBACK (on_layer_text_changed), self);
  g_signal_connect_swapped (self->layer_font_size, "value-changed", G_CALLBACK (on_layer_text_changed), self);
  
  g_signal_connect_swapped (self->load_image_button, "clicked", G_CALLBACK (on_load_image_clicked), self);
  g_signal_connect_swapped (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
  g_signal_connect_swapped (self->add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
  g_signal_connect_swapped (self->export_button, "clicked", G_CALLBACK (on_export_clicked), self);
  g_signal_connect_swapped (self->save_project_button, "clicked", G_CALLBACK (on_save_project_clicked), self);
  g_signal_connect_swapped (self->load_project_button, "clicked", G_CALLBACK (on_load_project_clicked), self);
  

  g_signal_connect_swapped (self->import_template_button, "clicked", G_CALLBACK (on_import_template_clicked), self);
  g_signal_connect_swapped (self->delete_template_button, "clicked", G_CALLBACK (on_delete_template_clicked), self);
  g_signal_connect (self->template_gallery, "child-activated", G_CALLBACK (on_template_selected), self);

  g_signal_connect (self->deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
  g_signal_connect_swapped (self->cinematic_button, "toggled", G_CALLBACK (on_text_changed), self);
  
  g_signal_connect_swapped (self->layer_opacity_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
  g_signal_connect_swapped (self->layer_rotation_scale, "value-changed", G_CALLBACK (on_layer_control_changed), self);
  g_signal_connect_swapped (self->blend_mode_row, "notify::selected", G_CALLBACK (on_layer_control_changed), self);
  g_signal_connect_swapped (self->delete_layer_button, "clicked", G_CALLBACK (on_delete_layer_clicked), self);

  self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
  gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), GTK_EVENT_CONTROLLER (self->drag_gesture));
  g_signal_connect (self->drag_gesture, "drag-begin", G_CALLBACK (on_drag_begin), self);
  g_signal_connect (self->drag_gesture, "drag-update", G_CALLBACK (on_drag_update), self);
  g_signal_connect (self->drag_gesture, "drag-end", G_CALLBACK (on_drag_end), self);

  GtkEventController *motion = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), motion);
  g_signal_connect (motion, "motion", G_CALLBACK (on_mouse_move), self);
  
  GtkEventController *key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), key_controller);
  
  populate_template_gallery (self);
}