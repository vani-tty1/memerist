/* myapp-window.h
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

/* Almighty God, forgive me for all of my wicked sins, I forsake my escape.
 * I give up even Trisha, I belong here in hell and accept my just damnation,
 * But grant me the power to keep your enemy here with me.
 */

#include "config.h"
#include "myapp-window.h"
#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#define CLAMP_U8(val) ((val) < 0 ? 0 : ((val) > 255 ? 255 : (val)))

typedef enum {
  DRAG_TYPE_NONE,
  DRAG_TYPE_TOP_TEXT,
  DRAG_TYPE_BOTTOM_TEXT,
  DRAG_TYPE_IMAGE_MOVE,
  DRAG_TYPE_IMAGE_RESIZE
} DragType;

typedef struct {
  GdkPixbuf *pixbuf;
  double x;
  double y;
  double width;
  double height;
  double scale;
} ImageLayer;

struct _MyappWindow
{
  AdwApplicationWindow parent_instance;
  GtkStack        *content_stack;
  GtkImage        *meme_preview;
  AdwEntryRow     *top_text_entry;
  AdwEntryRow     *bottom_text_entry;
  GtkButton       *export_button;
  GtkButton       *load_image_button;
  GtkButton       *clear_button;
  GtkButton       *add_image_button;
  GtkButton       *import_template_button;
  GtkButton       *delete_template_button;
  GtkToggleButton *deep_fry_button;
  GtkSpinButton   *top_text_size;
  GtkSpinButton   *bottom_text_size;
  GtkFlowBox      *template_gallery;

  GdkPixbuf       *template_image;
  GdkPixbuf       *final_meme;

  GList           *layers;
  ImageLayer      *selected_layer;

  double          top_text_y;
  double          top_text_x;
  double          bottom_text_y;
  double          bottom_text_x;

  DragType        drag_type;
  GtkGestureDrag *drag_gesture;

  double          drag_start_x;
  double          drag_start_y;
  double          drag_obj_start_x;
  double          drag_obj_start_y;
  double          drag_obj_start_scale;
};

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

static void on_text_changed (MyappWindow *self);
static void on_load_image_clicked (MyappWindow *self);
static void on_clear_clicked (MyappWindow *self);
static void on_add_image_clicked (MyappWindow *self);
static void on_export_clicked (MyappWindow *self);
static void on_import_template_clicked (MyappWindow *self);
static void on_delete_template_clicked (MyappWindow *self);
static void on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self);
static void render_meme (MyappWindow *self);
static void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self);
static void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
static void on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
static void on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self);
static void populate_template_gallery (MyappWindow *self);
static void on_deep_fry_toggled (GtkToggleButton *btn, MyappWindow *self);

static void
get_image_coordinates (MyappWindow *self, double widget_x, double widget_y, double *img_x, double *img_y)
{
  double ww, wh, iw, ih, scale, draw_w, draw_h, off_x, off_y;
  double w_ratio, h_ratio;

  if (!self->template_image) {
    *img_x = 0; *img_y = 0; return;
  }

  ww = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));
  wh = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));

  if (ww <= 0 || wh <= 0) {
    *img_x = 0; *img_y = 0; return;
  }

  iw = gdk_pixbuf_get_width (self->template_image);
  ih = gdk_pixbuf_get_height (self->template_image);

  w_ratio = ww / iw;
  h_ratio = wh / ih;

  scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  draw_w = iw * scale;
  draw_h = ih * scale;

  off_x = (ww - draw_w) / 2.0;
  off_y = (wh - draw_h) / 2.0;

  *img_x = (widget_x - off_x) / draw_w;
  *img_y = (widget_y - off_y) / draw_h;
}

static void
free_image_layer (gpointer data)
{
  ImageLayer *layer = (ImageLayer *)data;
  if (layer->pixbuf)
    g_object_unref (layer->pixbuf);
  g_free (layer);
}

static void
myapp_window_finalize (GObject *object)
{
  MyappWindow *self = MYAPP_WINDOW (object);

  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);
  g_clear_object (&self->drag_gesture);

  if (self->layers) {
    g_list_free_full (self->layers, free_image_layer);
  }

  G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static void
myapp_window_class_init (MyappWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = myapp_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Memerist/myapp-window.ui");

  gtk_widget_class_bind_template_child (widget_class, MyappWindow, meme_preview);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, content_stack);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, top_text_entry);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, bottom_text_entry);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, export_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_image_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, clear_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_image_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, import_template_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, delete_template_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, deep_fry_button);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, top_text_size);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, bottom_text_size);
  gtk_widget_class_bind_template_child (widget_class, MyappWindow, template_gallery);
}

static gboolean
on_drop_file (GtkDropTarget *target, const GValue *value, double x, double y, MyappWindow *self)
{
  GSList *list;
  GFile *file;
  char *path;

  if (!G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) return FALSE;
  list = g_value_get_boxed (value);
  if (!list) return FALSE;

  file = (GFile *)list->data;
  path = g_file_get_path (file);

  if (path) {
    g_clear_object (&self->template_image);
    self->template_image = gdk_pixbuf_new_from_file (path, NULL);
    gtk_stack_set_visible_child_name (self->content_stack, "content");

    if (self->layers) {
      g_list_free_full (self->layers, free_image_layer);
      self->layers = NULL;
      self->selected_layer = NULL;
    }

    if (self->template_image) {
      self->top_text_y = 0.1; self->top_text_x = 0.5;
      self->bottom_text_y = 0.9; self->bottom_text_x = 0.5;
      gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
      render_meme (self);
    }
    g_free (path);
    return TRUE;
  }
  return FALSE;
}

static void
myapp_window_init (MyappWindow *self)
{
  GtkEventController *motion;
  GtkDropTarget *target;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->top_text_y = 0.1;
  self->top_text_x = 0.5;
  self->bottom_text_y = 0.9;
  self->bottom_text_x = 0.5;
  self->drag_type = DRAG_TYPE_NONE;
  self->layers = NULL;
  self->selected_layer = NULL;

  g_signal_connect_swapped (self->top_text_entry, "changed", G_CALLBACK (on_text_changed), self);
  g_signal_connect_swapped (self->bottom_text_entry, "changed", G_CALLBACK (on_text_changed), self);
  g_signal_connect_swapped (self->top_text_size, "value-changed", G_CALLBACK (on_text_changed), self);
  g_signal_connect_swapped (self->bottom_text_size, "value-changed", G_CALLBACK (on_text_changed), self);
  g_signal_connect_swapped (self->load_image_button, "clicked", G_CALLBACK (on_load_image_clicked), self);
  g_signal_connect_swapped (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
  g_signal_connect_swapped (self->add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
  g_signal_connect_swapped (self->export_button, "clicked", G_CALLBACK (on_export_clicked), self);
  g_signal_connect_swapped (self->import_template_button, "clicked", G_CALLBACK (on_import_template_clicked), self);
  g_signal_connect_swapped (self->delete_template_button, "clicked", G_CALLBACK (on_delete_template_clicked), self);
  g_signal_connect (self->deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
  g_signal_connect (self->template_gallery, "child-activated", G_CALLBACK (on_template_selected), self);

  self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
  gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), GTK_EVENT_CONTROLLER (self->drag_gesture));
  g_signal_connect (self->drag_gesture, "drag-begin", G_CALLBACK (on_drag_begin), self);
  g_signal_connect (self->drag_gesture, "drag-update", G_CALLBACK (on_drag_update), self);
  g_signal_connect (self->drag_gesture, "drag-end", G_CALLBACK (on_drag_end), self);

  motion = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), motion);
  g_signal_connect (motion, "motion", G_CALLBACK (on_mouse_move), self);

  target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect (target, "drop", G_CALLBACK (on_drop_file), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (target));

  populate_template_gallery (self);
}

static char *
get_user_template_dir (void)
{
  return g_build_filename (g_get_user_data_dir (), "Memerist", "templates", NULL);
}

static gboolean
is_user_template (const char *path)
{
  g_autofree char *user_dir = get_user_template_dir ();
  return g_str_has_prefix (path, user_dir);
}

static void
add_file_to_gallery (MyappWindow *self, const char *full_path)
{
  GtkWidget *picture = gtk_picture_new_for_filename (full_path);
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
  gtk_widget_set_size_request (picture, 120, 120);
  g_object_set_data_full (G_OBJECT (picture), "template-path", g_strdup (full_path), g_free);
  gtk_flow_box_append (self->template_gallery, picture);
}

static void
scan_directory_for_templates (MyappWindow *self, const char *dir_path)
{
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
populate_template_gallery (MyappWindow *self)
{
  char *user_dir;
#ifdef TEMPLATE_DIR
  scan_directory_for_templates (self, TEMPLATE_DIR);
#else
  scan_directory_for_templates (self, "/usr/share/Memerist/templates");
#endif
  user_dir = get_user_template_dir ();
  g_mkdir_with_parents (user_dir, 0755);
  scan_directory_for_templates (self, user_dir);
  g_free (user_dir);
}

static void
on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self)
{
  GtkWidget *image;
  const char *template_path;
  GError *error = NULL;

  if (!child) { gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), FALSE); return; }
  image = gtk_flow_box_child_get_child (child);
  template_path = g_object_get_data (G_OBJECT (image), "template-path");
  if (!template_path) return;

  gtk_widget_set_sensitive (GTK_WIDGET (self->delete_template_button), is_user_template (template_path));
  g_clear_object (&self->template_image);

  if (self->layers) {
    g_list_free_full (self->layers, free_image_layer);
    self->layers = NULL;
    self->selected_layer = NULL;
  }

  self->template_image = gdk_pixbuf_new_from_file (template_path, &error);
  if (error) { g_warning ("Load failed: %s", error->message); g_error_free (error); return; }

  gtk_stack_set_visible_child_name (self->content_stack, "content");

  gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
  render_meme (self);
}

static void
on_import_template_response (GObject *s, GAsyncResult *r, gpointer d)
{
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
  } else {
    g_error_free (error);
  }
  g_free (filename); g_free (user_dir_path); g_free (dest_path);
  g_object_unref (source_file); g_object_unref (dest_file);
}

static void
on_import_template_clicked (MyappWindow *self)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GtkFileFilter *filter = gtk_file_filter_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

  gtk_file_dialog_set_title (dialog, "Import Template");
  gtk_file_filter_add_mime_type (filter, "image/*");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_import_template_response, self);
  g_object_unref (filters); g_object_unref (filter);
}

static void
on_delete_confirm_response (GObject *s, GAsyncResult *r, gpointer d)
{
  GtkAlertDialog *dialog = GTK_ALERT_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GError *error = NULL;
  if (gtk_alert_dialog_choose_finish (dialog, r, &error) == 1) {
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
  g_clear_error (&error);
}

static void
on_delete_template_clicked (MyappWindow *self)
{
  GtkAlertDialog *dialog = gtk_alert_dialog_new ("Delete this template?");
  gtk_alert_dialog_set_buttons (dialog, (const char *[]) {"Cancel", "Delete", NULL});
  gtk_alert_dialog_set_default_button (dialog, 1);
  gtk_alert_dialog_choose (dialog, GTK_WINDOW (self), NULL, on_delete_confirm_response, self);
}

static void
on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self)
{
  double rel_x, rel_y;
  double img_w, img_h;
  double ww, wh, w_ratio, h_ratio, screen_scale;
  gboolean found_hover = FALSE;
  GList *l;

  if (self->template_image == NULL) {
    gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
    return;
  }

  get_image_coordinates(self, x, y, &rel_x, &rel_y);
  img_w = gdk_pixbuf_get_width(self->template_image);
  img_h = gdk_pixbuf_get_height(self->template_image);

  ww = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));
  wh = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
  w_ratio = ww / img_w;
  h_ratio = wh / img_h;
  screen_scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
    ImageLayer *layer = (ImageLayer *)l->data;
    double half_w, half_h, left, right, top, bottom;
    double corner_x, corner_y;

    half_w = (layer->width * layer->scale) / 2.0 / img_w;
    half_h = (layer->height * layer->scale) / 2.0 / img_h;

    left = layer->x - half_w;
    right = layer->x + half_w;
    top = layer->y - half_h;
    bottom = layer->y + half_h;

    corner_x = 20.0 / (img_w * screen_scale);
    corner_y = 20.0 / (img_h * screen_scale);

    if (layer == self->selected_layer) {
      gboolean near_left = fabs(rel_x - left) < corner_x;
      gboolean near_right = fabs(rel_x - right) < corner_x;
      gboolean near_top = fabs(rel_y - top) < corner_y;
      gboolean near_bottom = fabs(rel_y - bottom) < corner_y;

      if ((near_left || near_right) && (near_top || near_bottom)) {
        if ((near_left && near_top) || (near_right && near_bottom))
          gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "nw-se-resize");
        else
          gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "nesw-resize");

        found_hover = TRUE;
        break;
      }
    }

    if (rel_x >= left && rel_x <= right && rel_y >= top && rel_y <= bottom) {
      gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), "move");
      found_hover = TRUE;
      break;
    }
  }

  if (!found_hover) {
    gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
  }
}

static void
on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self)
{
  double rel_x, rel_y;
  double img_w, img_h;
  double ww, wh, w_ratio, h_ratio, screen_scale;
  GList *l;
  double top_fs, bottom_fs;
  double top_threshold, bottom_threshold;
  double dist_top, dist_bottom;

  if (self->template_image == NULL) return;

  get_image_coordinates(self, x, y, &rel_x, &rel_y);
  img_w = gdk_pixbuf_get_width(self->template_image);
  img_h = gdk_pixbuf_get_height(self->template_image);

  ww = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));
  wh = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
  w_ratio = ww / img_w;
  h_ratio = wh / img_h;
  screen_scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
    ImageLayer *layer = (ImageLayer *)l->data;
    double half_w, half_h, left, right, top, bottom;
    double corner_x, corner_y;

    half_w = (layer->width * layer->scale) / 2.0 / img_w;
    half_h = (layer->height * layer->scale) / 2.0 / img_h;

    left = layer->x - half_w;
    right = layer->x + half_w;
    top = layer->y - half_h;
    bottom = layer->y + half_h;

    corner_x = 20.0 / (img_w * screen_scale);
    corner_y = 20.0 / (img_h * screen_scale);

    if (layer == self->selected_layer) {
      gboolean near_corner =
        (fabs(rel_x - left) < corner_x || fabs(rel_x - right) < corner_x) &&
        (fabs(rel_y - top) < corner_y || fabs(rel_y - bottom) < corner_y);

      if (near_corner) {
        self->drag_type = DRAG_TYPE_IMAGE_RESIZE;
        self->selected_layer = layer;
        self->drag_obj_start_scale = layer->scale;
        self->drag_start_x = rel_x * img_w;
        self->drag_start_y = rel_y * img_h;
        render_meme(self);
        return;
      }
    }

    if (rel_x >= left && rel_x <= right && rel_y >= top && rel_y <= bottom) {
      self->drag_type = DRAG_TYPE_IMAGE_MOVE;
      self->selected_layer = layer;
      self->drag_obj_start_x = layer->x;
      self->drag_obj_start_y = layer->y;
      self->drag_start_x = rel_x;
      self->drag_start_y = rel_y;
      render_meme(self);
      return;
    }
  }

  top_fs = gtk_spin_button_get_value (self->top_text_size);
  bottom_fs = gtk_spin_button_get_value (self->bottom_text_size);
  top_threshold = MAX(0.1, (top_fs / img_h) * 0.6);
  bottom_threshold = MAX(0.1, (bottom_fs / img_h) * 0.6);
  dist_top = fabs(rel_y - self->top_text_y);
  dist_bottom = fabs(rel_y - self->bottom_text_y);

  if (dist_top < top_threshold) {
    self->drag_type = DRAG_TYPE_TOP_TEXT;
    self->drag_obj_start_x = self->top_text_x;
    self->drag_obj_start_y = self->top_text_y;
    self->selected_layer = NULL;
  } else if (dist_bottom < bottom_threshold) {
    self->drag_type = DRAG_TYPE_BOTTOM_TEXT;
    self->drag_obj_start_x = self->bottom_text_x;
    self->drag_obj_start_y = self->bottom_text_y;
    self->selected_layer = NULL;
  } else {
    self->drag_type = DRAG_TYPE_NONE;
    self->selected_layer = NULL;
  }

  self->drag_start_x = rel_x;
  self->drag_start_y = rel_y;
  render_meme(self);
}

static void
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self)
{
  double delta_x, delta_y;
  double img_w, img_h;
  double ww, wh, w_ratio, h_ratio, scale;

  if (self->drag_type == DRAG_TYPE_NONE || self->template_image == NULL) return;

  img_w = gdk_pixbuf_get_width(self->template_image);
  img_h = gdk_pixbuf_get_height(self->template_image);

  ww = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));
  wh = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
  w_ratio = ww / img_w;
  h_ratio = wh / img_h;
  scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  delta_x = (offset_x / scale) / img_w;
  delta_y = (offset_y / scale) / img_h;

  if (self->drag_type == DRAG_TYPE_IMAGE_MOVE && self->selected_layer) {
    self->selected_layer->x = CLAMP (self->drag_obj_start_x + delta_x, 0.0, 1.0);
    self->selected_layer->y = CLAMP (self->drag_obj_start_y + delta_y, 0.0, 1.0);
  }
  else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE && self->selected_layer) {
    double cx, cy, start_dx, start_dy, start_dist;
    double current_img_x, current_img_y, cur_dx, cur_dy, cur_dist;

    cx = self->selected_layer->x * img_w;
    cy = self->selected_layer->y * img_h;

    start_dx = self->drag_start_x - cx;
    start_dy = self->drag_start_y - cy;
    start_dist = sqrt(start_dx*start_dx + start_dy*start_dy);

    current_img_x = self->drag_start_x + (offset_x / scale);
    current_img_y = self->drag_start_y + (offset_y / scale);

    cur_dx = current_img_x - cx;
    cur_dy = current_img_y - cy;
    cur_dist = sqrt(cur_dx*cur_dx + cur_dy*cur_dy);

    if (start_dist > 5.0) {
      double ratio = cur_dist / start_dist;
      self->selected_layer->scale = CLAMP (self->drag_obj_start_scale * ratio, 0.1, 5.0);
    }
  }
  else if (self->drag_type == DRAG_TYPE_TOP_TEXT) {
    self->top_text_x = CLAMP (self->drag_obj_start_x + delta_x, 0.0, 1.0);
    self->top_text_y = CLAMP (self->drag_obj_start_y + delta_y, 0.05, 0.95);
  }
  else if (self->drag_type == DRAG_TYPE_BOTTOM_TEXT) {
    self->bottom_text_x = CLAMP (self->drag_obj_start_x + delta_x, 0.0, 1.0);
    self->bottom_text_y = CLAMP (self->drag_obj_start_y + delta_y, 0.05, 0.95);
  }

  render_meme (self);
}

static void on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self) {
  self->drag_type = DRAG_TYPE_NONE;
}

static void on_text_changed (MyappWindow *self) { if (self->template_image) render_meme (self); }

static void
on_add_image_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  MyappWindow *self = MYAPP_WINDOW (user_data);
  GFile *file;
  GError *error = NULL;
  char *path;
  ImageLayer *new_layer;

  file = gtk_file_dialog_open_finish (dialog, result, &error);
  if (error) { g_error_free (error); return; }
  path = g_file_get_path (file);

  new_layer = g_new0 (ImageLayer, 1);
  new_layer->pixbuf = gdk_pixbuf_new_from_file (path, &error);

  if (!error) {
    new_layer->width = gdk_pixbuf_get_width (new_layer->pixbuf);
    new_layer->height = gdk_pixbuf_get_height (new_layer->pixbuf);
    new_layer->x = 0.5;
    new_layer->y = 0.5;
    new_layer->scale = 1.0;

    self->layers = g_list_append (self->layers, new_layer);
    self->selected_layer = new_layer;
    render_meme (self);
  } else {
    g_free (new_layer);
  }
  g_free (path); g_object_unref (file);
}

static void on_add_image_clicked (MyappWindow *self) {
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GtkFileFilter *filter = gtk_file_filter_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  gtk_file_dialog_set_title (dialog, "Add Overlay Image");
  gtk_file_filter_add_mime_type (filter, "image/*");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_add_image_response, self);
  g_object_unref (filters); g_object_unref (filter);
}

static void on_clear_clicked (MyappWindow *self) {
  gtk_stack_set_visible_child_name (self->content_stack, "empty");
  gtk_editable_set_text (GTK_EDITABLE (self->top_text_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->bottom_text_entry), "");
  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);

  if (self->layers) {
    g_list_free_full (self->layers, free_image_layer);
    self->layers = NULL;
  }
  self->selected_layer = NULL;

  gtk_image_clear (self->meme_preview);
  gtk_image_set_from_icon_name (self->meme_preview, "image-x-generic-symbolic");
  gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), FALSE);
  gtk_toggle_button_set_active (self->deep_fry_button, FALSE);
  gtk_flow_box_unselect_all (self->template_gallery);
}

static void
draw_text_with_outline (cairo_t *cr, const char *text, double x, double y, double font_size)
{
  cairo_text_extents_t extents;
  cairo_select_font_face (cr, "Impact", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, font_size);
  cairo_text_extents (cr, text, &extents);
  x = x - extents.width / 2 - extents.x_bearing;
  y = y - extents.height / 2 - extents.y_bearing;
  cairo_set_source_rgb (cr, 0, 0, 0); cairo_set_line_width (cr, font_size * 0.1);
  cairo_move_to (cr, x, y); cairo_text_path (cr, text); cairo_stroke (cr);
  cairo_set_source_rgb (cr, 1, 1, 1); cairo_move_to (cr, x, y); cairo_show_text (cr, text);
}

static GdkPixbuf *
apply_deep_fry (GdkPixbuf *src)
{
  int width = gdk_pixbuf_get_width (src);
  int height = gdk_pixbuf_get_height (src);
  int n_channels = gdk_pixbuf_get_n_channels (src);
  int rowstride = gdk_pixbuf_get_rowstride (src);
  GdkPixbuf *fried, *shrunk, *final;
  guchar *pixels;
  double contrast = 2.0;
  int noise_level = 30;

  fried = gdk_pixbuf_copy (src);
  pixels = gdk_pixbuf_get_pixels (fried);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      guchar *p = pixels + y * rowstride + x * n_channels;
      for (int i = 0; i < 3; i++) {
        int noise = (rand() % (noise_level * 2 + 1)) - noise_level;
        int val = p[i] + noise;
        double c_val = ((double)val - 128.0) * contrast + 128.0;
        p[i] = CLAMP_U8 ((int)c_val);
      }
    }
  }

  shrunk = gdk_pixbuf_scale_simple (fried, MAX (width * 0.25, 1), MAX (height * 0.25, 1), GDK_INTERP_NEAREST);
  final = gdk_pixbuf_scale_simple (shrunk, width, height, GDK_INTERP_NEAREST);

  g_object_unref (fried);
  g_object_unref (shrunk);
  return final;
}




// Stop and think, You are about to enter a place called the void.
// The rendering logic is so fragile, any code change here can destroy a whole function
// Do not edit carelessly, consider what you're about to do Tarnished.







static void
render_meme (MyappWindow *self)
{
  const char *top_text, *bottom_text;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;
  GdkPixbuf *composite_pixbuf;
  GList *l;

  if (self->template_image == NULL) return;

  width = gdk_pixbuf_get_width (self->template_image);
  height = gdk_pixbuf_get_height (self->template_image);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  cairo_save (cr);
  gdk_cairo_set_source_pixbuf (cr, self->template_image, 0.0, 0.0);
  cairo_paint (cr);
  cairo_restore (cr);

  for (l = self->layers; l != NULL; l = l->next) {
    ImageLayer *layer = (ImageLayer *)l->data;
    double scaled_w, scaled_h, draw_x, draw_y;

    if (!layer->pixbuf) continue;

    scaled_w = layer->width * layer->scale;
    scaled_h = layer->height * layer->scale;
    draw_x = layer->x * width - scaled_w / 2.0;
    draw_y = layer->y * height - scaled_h / 2.0;

    cairo_save (cr);
    cairo_translate (cr, draw_x + scaled_w / 2.0, draw_y + scaled_h / 2.0);

    cairo_save (cr);
    cairo_scale (cr, layer->scale, layer->scale);
    gdk_cairo_set_source_pixbuf (cr, layer->pixbuf, -layer->width/2.0, -layer->height/2.0);
    cairo_paint (cr);
    cairo_restore (cr);

    if (layer == self->selected_layer) {
      double hw = scaled_w / 2.0;
      double hh = scaled_h / 2.0;
      double handles_x[4];
      double handles_y[4];
      int i;

      handles_x[0] = -hw; handles_x[1] = hw; handles_x[2] = -hw; handles_x[3] = hw;
      handles_y[0] = -hh; handles_y[1] = -hh; handles_y[2] = hh; handles_y[3] = hh;

      cairo_set_source_rgba (cr, 0.4, 0.2, 0.8, 0.8);
      cairo_set_line_width (cr, 2.0);
      cairo_rectangle (cr, -hw, -hh, scaled_w, scaled_h);
      cairo_stroke (cr);

      cairo_set_source_rgb (cr, 1, 1, 1);

      for (i = 0; i < 4; i++) {
        cairo_new_sub_path (cr);
        cairo_arc (cr, handles_x[i], handles_y[i], 6, 0, 2 * M_PI);
        cairo_fill_preserve (cr);
        cairo_set_source_rgba (cr, 0.4, 0.2, 0.8, 1.0);
        cairo_stroke (cr);
      }
    }
    cairo_restore (cr);
  }

  top_text = gtk_editable_get_text (GTK_EDITABLE (self->top_text_entry));
  bottom_text = gtk_editable_get_text (GTK_EDITABLE (self->bottom_text_entry));

  if (top_text && strlen (top_text) > 0) {
    char *upper_text = g_utf8_strup (top_text, -1);
    draw_text_with_outline (cr, upper_text, width * self->top_text_x, height * self->top_text_y, gtk_spin_button_get_value (self->top_text_size));
    g_free (upper_text);
  }
  if (bottom_text && strlen (bottom_text) > 0) {
    char *upper_text = g_utf8_strup (bottom_text, -1);
    draw_text_with_outline (cr, upper_text, width * self->bottom_text_x, height * self->bottom_text_y, gtk_spin_button_get_value (self->bottom_text_size));
    g_free (upper_text);
  }

  cairo_surface_flush (surface);
  cairo_destroy (cr);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  composite_pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (gtk_toggle_button_get_active (self->deep_fry_button)) {
    GdkPixbuf *fried = apply_deep_fry (composite_pixbuf);
    g_object_unref (composite_pixbuf);
    composite_pixbuf = fried;
  }

  g_clear_object (&self->final_meme);
  self->final_meme = composite_pixbuf;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  texture = gdk_texture_new_for_pixbuf (self->final_meme);
  G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_image_set_from_paintable (self->meme_preview, GDK_PAINTABLE (texture));
  g_object_unref (texture);
  cairo_surface_destroy (surface);
}

static void on_deep_fry_toggled (GtkToggleButton *btn, MyappWindow *self) {
  if (self->template_image) render_meme (self);
}

static void on_load_image_response (GObject *s, GAsyncResult *r, gpointer d) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GFile *file;
  char *path;

  file = gtk_file_dialog_open_finish (dialog, r, NULL);
  if (!file) return;
  path = g_file_get_path (file);
  g_clear_object (&self->template_image);
  self->template_image = gdk_pixbuf_new_from_file (path, NULL);
  if (self->layers) { g_list_free_full (self->layers, free_image_layer); self->layers = NULL; self->selected_layer = NULL; }
  if (self->template_image) {
    gtk_stack_set_visible_child_name (self->content_stack, "content");
    self->top_text_y = 0.1; self->top_text_x = 0.5;
    self->bottom_text_y = 0.9; self->bottom_text_x = 0.5;
    gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
    render_meme (self);
  }
  g_free (path); g_object_unref (file);
}

static void on_load_image_clicked (MyappWindow *self) {
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GtkFileFilter *filter = gtk_file_filter_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  gtk_file_dialog_set_title (dialog, "Choose Base Image");
  gtk_file_filter_add_mime_type (filter, "image/*");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_load_image_response, self);
  g_object_unref (filters); g_object_unref (filter);
}

static void on_export_response (GObject *s, GAsyncResult *r, gpointer d) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (s);
  MyappWindow *self = MYAPP_WINDOW (d);
  GFile *file = gtk_file_dialog_save_finish (dialog, r, NULL);
  if (file && self->final_meme) {
    gdk_pixbuf_save (self->final_meme, g_file_get_path (file), "png", NULL, NULL);
    g_object_unref (file);
  }
}
static void on_export_clicked (MyappWindow *self) {
  GtkFileDialog *dialog;
  if (!self->final_meme) return;
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Save Meme");
  gtk_file_dialog_set_initial_name (dialog, "meme.png");
  gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_export_response, self);
}
