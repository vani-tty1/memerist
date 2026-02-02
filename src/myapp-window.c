/* myapp-window.c
 *
 * Copyright 2025 Giovanni
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "myapp-window.h"
#include "adwaita.h"
#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#define CLAMP_U8(val) ((val) < 0 ? 0 : ((val) > 255 ? 255 : (val)))

typedef enum {
  DRAG_TYPE_NONE,
  DRAG_TYPE_IMAGE_MOVE,
  DRAG_TYPE_IMAGE_RESIZE,
  DRAG_TYPE_CROP_MOVE,
  DRAG_TYPE_CROP_RESIZE
} DragType;

// NEW: Handles for 8-way resizing
typedef enum {
  HANDLE_NONE,
  HANDLE_TOP_LEFT,
  HANDLE_TOP,
  HANDLE_TOP_RIGHT,
  HANDLE_RIGHT,
  HANDLE_BOTTOM_RIGHT,
  HANDLE_BOTTOM,
  HANDLE_BOTTOM_LEFT,
  HANDLE_LEFT,
  HANDLE_CENTER
} ResizeHandle;

typedef enum {
  BLEND_NORMAL,
  BLEND_MULTIPLY,
  BLEND_SCREEN,
  BLEND_OVERLAY
} BlendMode;

typedef enum {
  LAYER_TYPE_IMAGE,
  LAYER_TYPE_TEXT
} LayerType;

typedef struct {
  LayerType type;
  GdkPixbuf *pixbuf;
  char *text;
  double font_size;
  double x;
  double y;
  double width;
  double height;
  double scale;
  double rotation;
  double opacity;
  BlendMode blend_mode;
} ImageLayer;

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

  // UI Controls
  GtkToggleButton *cinematic_button;
  GtkScale        *layer_opacity_scale;
  GtkScale        *layer_rotation_scale;
  AdwComboRow     *blend_mode_row;
  GtkButton       *delete_layer_button;

  GdkPixbuf       *template_image;
  GdkPixbuf       *final_meme;

  GList           *layers;
  ImageLayer      *selected_layer;

  // Undo/Redo History
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
  double drag_obj_start_scale; // Used as Width for crop
  double drag_obj_start_h;     // Height for crop

  // NEW: Store which handle is being dragged
  ResizeHandle active_crop_handle;

  // Crop state
  double crop_x;
  double crop_y;
  double crop_w;
  double crop_h;
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
static void on_layer_control_changed (MyappWindow *self);
static void on_delete_layer_clicked (MyappWindow *self);
static void sync_ui_with_layer(MyappWindow *self);
static void on_apply_crop_clicked (MyappWindow *self);


static ImageLayer *
copy_image_layer (const ImageLayer *src) {
  ImageLayer *dst = g_new0 (ImageLayer, 1);
  *dst = *src;
  if (src->pixbuf) g_object_ref (src->pixbuf);
  if (src->text) dst->text = g_strdup (src->text);
  return dst;
}

static GList *
copy_layer_list (GList *src) {
  GList *dst = NULL;
  GList *l;
  for (l = src; l != NULL; l = l->next) {
    dst = g_list_append (dst, copy_image_layer ((ImageLayer *)l->data));
  }
  return dst;
}

static void
free_image_layer (gpointer data) {
  ImageLayer *layer = (ImageLayer *)data;
  if (layer) {
    if (layer->pixbuf) g_object_unref (layer->pixbuf);
    if (layer->text) g_free (layer->text);
    g_free (layer);
  }
}

static void
free_history_stack (GList **stack) {
  GList *l;
  for (l = *stack; l != NULL; l = l->next) {
    g_list_free_full ((GList *)l->data, (GDestroyNotify)free_image_layer);
  }
  g_list_free (*stack);
  *stack = NULL;
}

static void
push_undo (MyappWindow *self) {
  free_history_stack (&self->redo_stack);

  if (g_list_length (self->undo_stack) >= 20) {
      GList *last = g_list_last (self->undo_stack);
      g_list_free_full ((GList *)last->data, (GDestroyNotify)free_image_layer);
      self->undo_stack = g_list_delete_link (self->undo_stack, last);
  }

  self->undo_stack = g_list_prepend (self->undo_stack, copy_layer_list (self->layers));
}

static void
perform_undo (MyappWindow *self) {
  if (!self->undo_stack) return;

  self->redo_stack = g_list_prepend (self->redo_stack, self->layers);

  self->layers = (GList *)self->undo_stack->data;
  self->undo_stack = g_list_delete_link (self->undo_stack, self->undo_stack);

  self->selected_layer = NULL;
  sync_ui_with_layer (self);
  render_meme (self);
}

static void
perform_redo (MyappWindow *self) {
  if (!self->redo_stack) return;

  self->undo_stack = g_list_prepend (self->undo_stack, self->layers);

  self->layers = (GList *)self->redo_stack->data;
  self->redo_stack = g_list_delete_link (self->redo_stack, self->redo_stack);

  self->selected_layer = NULL;
  sync_ui_with_layer (self);
  render_meme (self);
}



static void
get_image_coordinates (MyappWindow *self, double widget_x, double widget_y, double *img_x, double *img_y) {
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
myapp_window_finalize (GObject *object) {
  MyappWindow *self = MYAPP_WINDOW (object);

  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);
  g_clear_object (&self->drag_gesture);

  if (self->layers) {
    g_list_free_full (self->layers, free_image_layer);
  }

  free_history_stack (&self->undo_stack);
  free_history_stack (&self->redo_stack);

  G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static void
myapp_window_class_init (MyappWindowClass *klass) {
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = myapp_window_finalize;


  gtk_widget_class_set_template_from_resource (widget_class,"/io/github/vani1_2/memerist/myapp-window.ui");

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
}

static gboolean
on_drop_file (GtkDropTarget *target, const GValue *value, double x, double y, MyappWindow *self) {
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

    free_history_stack (&self->undo_stack);
    free_history_stack (&self->redo_stack);

    if (self->template_image) {
      gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), TRUE);
      render_meme (self);
    }
    g_free (path);
    return TRUE;
  }
  return FALSE;
}

static void
on_layer_text_changed (MyappWindow *self) {
  if (self->selected_layer && self->selected_layer->type == LAYER_TYPE_TEXT) {
      g_free (self->selected_layer->text);
      self->selected_layer->text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->layer_text_entry)));
      self->selected_layer->font_size = gtk_spin_button_get_value (self->layer_font_size);
      render_meme (self);
  }
}

static void
on_add_text_clicked (MyappWindow *self) {
  ImageLayer *new_layer;

  push_undo (self);

  new_layer = g_new0 (ImageLayer, 1);
  new_layer->type = LAYER_TYPE_TEXT;
  new_layer->text = g_strdup ("Text");
  new_layer->font_size = 60.0;

  new_layer->x = 0.5;
  new_layer->y = 0.5;
  new_layer->scale = 1.0;
  new_layer->opacity = 1.0;
  new_layer->rotation = 0.0;
  new_layer->blend_mode = BLEND_NORMAL;

  self->layers = g_list_append (self->layers, new_layer);
  self->selected_layer = new_layer;
  sync_ui_with_layer(self);
  render_meme (self);
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, MyappWindow *self) {
  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
    perform_undo (self);
    return TRUE;
  }
  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) {
    perform_redo (self);
    return TRUE;
  }
  return FALSE;
}



static void
update_template_image (MyappWindow *self, GdkPixbuf *new_pixbuf) {
  if (!new_pixbuf) return;

  push_undo (self);

  if (self->template_image) g_object_unref (self->template_image);
  self->template_image = new_pixbuf;

  render_meme (self);
}

static void
on_rotate_clicked (GtkWidget *btn, MyappWindow *self) {
  gboolean clockwise;
  GdkPixbuf *new_pix;

  if (!self->template_image) return;

  clockwise = (btn == GTK_WIDGET (self->rotate_right_button));
  new_pix = gdk_pixbuf_rotate_simple (self->template_image,
      clockwise ? GDK_PIXBUF_ROTATE_CLOCKWISE : GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);

  update_template_image (self, new_pix);
}

static void
on_flip_clicked (GtkWidget *btn, MyappWindow *self) {
  gboolean horizontal;
  GdkPixbuf *new_pix;

  if (!self->template_image) return;

  horizontal = (btn == GTK_WIDGET (self->flip_h_button));
  new_pix = gdk_pixbuf_flip (self->template_image, horizontal);

  update_template_image (self, new_pix);
}

static void
on_crop_preset_clicked (GtkWidget *btn, MyappWindow *self) {
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

static void
on_crop_mode_toggled (GtkToggleButton *btn, MyappWindow *self) {
  gboolean active = gtk_toggle_button_get_active (btn);

  gtk_widget_set_visible (GTK_WIDGET (self->transform_group), active);
  gtk_widget_set_visible (GTK_WIDGET (self->layer_group), !active);
  gtk_widget_set_visible (GTK_WIDGET (self->templates_group), !active);

  if (active) {
    self->crop_x = 0.0;
    self->crop_y = 0.0;
    self->crop_w = 1.0;
    self->crop_h = 1.0;
    adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
  } else {
    // Reset cursor when leaving
    gtk_widget_set_cursor (GTK_WIDGET (self->meme_preview), NULL);
  }
  render_meme(self);
}

static void
on_apply_crop_clicked (MyappWindow *self) {
  if (!self->template_image) return;

  int iw = gdk_pixbuf_get_width(self->template_image);
  int ih = gdk_pixbuf_get_height(self->template_image);
  int x = self->crop_x * iw;
  int y = self->crop_y * ih;
  int w = self->crop_w * iw;
  int h = self->crop_h * ih;

  if (w <= 0 || h <= 0) return;

  push_undo(self);

  // Reposition layers relative to new crop
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

  update_template_image(self, new_pix); // This calls render_meme

  self->crop_x = 0; self->crop_y = 0; self->crop_w = 1; self->crop_h = 1;
  gtk_toggle_button_set_active(self->crop_mode_button, FALSE);
}


static void
myapp_window_init (MyappWindow *self) {
  GtkEventController *motion;
  GtkDropTarget *target;
  GtkEventController *key_controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->drag_type = DRAG_TYPE_NONE;
  self->layers = NULL;
  self->selected_layer = NULL;
  self->undo_stack = NULL;
  self->redo_stack = NULL;

  g_signal_connect (self->rotate_left_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
  g_signal_connect (self->rotate_right_button, "clicked", G_CALLBACK (on_rotate_clicked), self);
  g_signal_connect (self->flip_h_button, "clicked", G_CALLBACK (on_flip_clicked), self);
  g_signal_connect (self->flip_v_button, "clicked", G_CALLBACK (on_flip_clicked), self);
  g_signal_connect (self->crop_square_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
  g_signal_connect (self->crop_43_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);
  g_signal_connect (self->crop_169_button, "clicked", G_CALLBACK (on_crop_preset_clicked), self);

  //crop
  gtk_widget_set_sensitive (GTK_WIDGET (self->crop_mode_button), TRUE);
  g_signal_connect (self->crop_mode_button, "toggled", G_CALLBACK (on_crop_mode_toggled), self);
  
  g_signal_connect_swapped (self->add_text_button, "clicked", G_CALLBACK (on_add_text_clicked), self);
  g_signal_connect_swapped (self->layer_text_entry, "changed", G_CALLBACK (on_layer_text_changed), self);
  g_signal_connect_swapped (self->layer_font_size, "value-changed", G_CALLBACK (on_layer_text_changed), self);

  g_signal_connect_swapped (self->load_image_button, "clicked", G_CALLBACK (on_load_image_clicked), self);
  g_signal_connect_swapped (self->clear_button, "clicked", G_CALLBACK (on_clear_clicked), self);
  g_signal_connect_swapped (self->add_image_button, "clicked", G_CALLBACK (on_add_image_clicked), self);
  g_signal_connect_swapped (self->export_button, "clicked", G_CALLBACK (on_export_clicked), self);

  g_signal_connect_swapped (self->import_template_button, "clicked", G_CALLBACK (on_import_template_clicked), self);
  g_signal_connect_swapped (self->delete_template_button, "clicked", G_CALLBACK (on_delete_template_clicked), self);
  g_signal_connect (self->deep_fry_button, "toggled", G_CALLBACK (on_deep_fry_toggled), self);
  g_signal_connect (self->template_gallery, "child-activated", G_CALLBACK (on_template_selected), self);

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

  motion = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (GTK_WIDGET (self->meme_preview), motion);
  g_signal_connect (motion, "motion", G_CALLBACK (on_mouse_move), self);

  target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect (target, "drop", G_CALLBACK (on_drop_file), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (target));

  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), key_controller);

  populate_template_gallery (self);

}

static void
sync_ui_with_layer(MyappWindow *self) {
    gboolean sensitive = (self->selected_layer != NULL);
    gboolean is_text = (sensitive && self->selected_layer->type == LAYER_TYPE_TEXT);


    g_signal_handlers_block_by_func(self->layer_opacity_scale, on_layer_control_changed, self);
    g_signal_handlers_block_by_func(self->layer_rotation_scale, on_layer_control_changed, self);
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

    g_signal_handlers_unblock_by_func(self->layer_opacity_scale, on_layer_control_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_rotation_scale, on_layer_control_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_text_entry, on_layer_text_changed, self);
    g_signal_handlers_unblock_by_func(self->layer_font_size, on_layer_text_changed, self);
}

static void
on_layer_control_changed (MyappWindow *self) {
  if (self->selected_layer) {
     self->selected_layer->opacity = gtk_range_get_value(GTK_RANGE(self->layer_opacity_scale));
     self->selected_layer->rotation = gtk_range_get_value(GTK_RANGE(self->layer_rotation_scale));
     self->selected_layer->blend_mode = (BlendMode)adw_combo_row_get_selected(self->blend_mode_row);
     render_meme(self);
  }
}

static void
on_delete_layer_clicked (MyappWindow *self) {
  if (self->selected_layer) {
    push_undo (self);
    self->layers = g_list_remove(self->layers, self->selected_layer);
    free_image_layer(self->selected_layer);
    self->selected_layer = NULL;
    sync_ui_with_layer(self);
    render_meme(self);
  }
}

static char *
get_user_template_dir (void) {
return g_build_filename (g_get_user_data_dir (), "io.github.vani1_2.memerist", "templates", NULL);
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

  const char *res_path = "/io/github/vani1_2/memerist/templates";
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

  free_history_stack (&self->undo_stack);
  free_history_stack (&self->redo_stack);

  if (g_str_has_prefix (template_path, "resource://")) {
      self->template_image = gdk_pixbuf_new_from_resource (template_path + 11, &error);
  } else {
      self->template_image = gdk_pixbuf_new_from_file (template_path, &error);
  }

  if (error) { g_warning ("Load failed: %s", error->message); g_error_free (error); return; }

  gtk_stack_set_visible_child_name (self->content_stack, "content");

  gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), TRUE);
  render_meme (self);
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
  } else {
    g_error_free (error);
  }
  g_free (filename); g_free (user_dir_path); g_free (dest_path);
  g_object_unref (source_file); g_object_unref (dest_file);
}

static void
on_import_template_clicked (MyappWindow *self) {
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
on_delete_confirm_response (GObject *s, GAsyncResult *r, gpointer d) {
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
on_delete_template_clicked (MyappWindow *self) {
  GtkAlertDialog *dialog = gtk_alert_dialog_new ("Delete this template?");
  gtk_alert_dialog_set_buttons (dialog, (const char *[]) {"Cancel", "Delete", NULL});
  gtk_alert_dialog_set_default_button (dialog, 1);
  gtk_alert_dialog_choose (dialog, GTK_WINDOW (self), NULL, on_delete_confirm_response, self);
}


static ResizeHandle get_crop_handle_at_position (MyappWindow *self, double x, double y) {
    double handle_radius = 0.05; 
    
    if (fabs(x - self->crop_x) < handle_radius && fabs(y - self->crop_y) < handle_radius)
        return HANDLE_TOP_LEFT;
    if (fabs(x - (self->crop_x + self->crop_w)) < handle_radius && fabs(y - self->crop_y) < handle_radius)
        return HANDLE_TOP_RIGHT;
    if (fabs(x - self->crop_x) < handle_radius && fabs(y - (self->crop_y + self->crop_h)) < handle_radius)
        return HANDLE_BOTTOM_LEFT;
    if (fabs(x - (self->crop_x + self->crop_w)) < handle_radius && fabs(y - (self->crop_y + self->crop_h)) < handle_radius)
        return HANDLE_BOTTOM_RIGHT;

    if (fabs(y - self->crop_y) < handle_radius && x > self->crop_x && x < self->crop_x + self->crop_w)
        return HANDLE_TOP;
    if (fabs(y - (self->crop_y + self->crop_h)) < handle_radius && x > self->crop_x && x < self->crop_x + self->crop_w)
        return HANDLE_BOTTOM;
    if (fabs(x - self->crop_x) < handle_radius && y > self->crop_y && y < self->crop_y + self->crop_h)
        return HANDLE_LEFT;
    if (fabs(x - (self->crop_x + self->crop_w)) < handle_radius && y > self->crop_y && y < self->crop_y + self->crop_h)
        return HANDLE_RIGHT;

    if (x > self->crop_x && x < self->crop_x + self->crop_w &&
        y > self->crop_y && y < self->crop_y + self->crop_h)
        return HANDLE_CENTER;

    return HANDLE_NONE;
}

//follow mouse
static void
on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self) {
  double rel_x, rel_y;
  double img_w, img_h;
  double ww, wh, w_ratio, h_ratio, screen_scale;
  gboolean found_hover = FALSE;
  GList *l;
  ImageLayer *layer;
  double half_w_scaled, half_h_scaled;
  double left, right, top, bottom;
  double corner_x, corner_y;
  gboolean near_left, near_right, near_top, near_bottom;
  const char *cursor_name;

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

  // UPDATED: Cursor logic for 8-way cropping
  if (gtk_toggle_button_get_active(self->crop_mode_button)) {
    ResizeHandle handle = get_crop_handle_at_position(self, rel_x, rel_y);
    // const char *cursor_name = NULL;

    switch (handle) {
        case HANDLE_TOP_LEFT:     cursor_name = "nw-resize"; break;
        case HANDLE_TOP_RIGHT:    cursor_name = "ne-resize"; break;
        case HANDLE_BOTTOM_LEFT:  cursor_name = "sw-resize"; break;
        case HANDLE_BOTTOM_RIGHT: cursor_name = "se-resize"; break;
        case HANDLE_TOP:          cursor_name = "n-resize"; break;
        case HANDLE_BOTTOM:       cursor_name = "s-resize"; break;
        case HANDLE_LEFT:         cursor_name = "w-resize"; break;
        case HANDLE_RIGHT:        cursor_name = "e-resize"; break;
        case HANDLE_CENTER:       cursor_name = "move"; break;
        default:                  cursor_name = NULL; break;
    }
    gtk_widget_set_cursor_from_name(GTK_WIDGET(self->meme_preview), cursor_name);
    return; 
  }

  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
    layer = (ImageLayer *)l->data;

    half_w_scaled = (layer->width * layer->scale) / (2.0 * img_w);
    half_h_scaled = (layer->height * layer->scale) / (2.0 * img_h);

    left = layer->x - half_w_scaled;
    right = layer->x + half_w_scaled;
    top = layer->y - half_h_scaled;
    bottom = layer->y + half_h_scaled;

    corner_x = 20.0 / (img_w * screen_scale);
    corner_y = 20.0 / (img_h * screen_scale);

    if (layer == self->selected_layer) {
      near_left = fabs(rel_x - left) < corner_x;
      near_right = fabs(rel_x - right) < corner_x;
      near_top = fabs(rel_y - top) < corner_y;
      near_bottom = fabs(rel_y - bottom) < corner_y;

      if ((near_left || near_right) && (near_top || near_bottom)) {
        cursor_name = ((near_left && near_top) || (near_right && near_bottom)) ? "nw-se-resize" : "nesw-resize";
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self->meme_preview), cursor_name);
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

//drag logic
static void
on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self) {
  double rel_x, rel_y;
  double img_w, img_h;
  double ww, wh, w_ratio, h_ratio, screen_scale;
  GList *l;
  ImageLayer *layer;
  double half_w_scaled, half_h_scaled;
  double left, right, top, bottom;
  double corner_x, corner_y;
  gboolean near_corner;

  if (self->template_image == NULL) return;

  get_image_coordinates(self, x, y, &rel_x, &rel_y);
  img_w = gdk_pixbuf_get_width(self->template_image);
  img_h = gdk_pixbuf_get_height(self->template_image);

  ww = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));
  wh = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
  w_ratio = ww / img_w;
  h_ratio = wh / img_h;
  screen_scale = (w_ratio < h_ratio) ? w_ratio : h_ratio;

  if (gtk_toggle_button_get_active(self->crop_mode_button) && self->template_image) {
      self->active_crop_handle = get_crop_handle_at_position(self, rel_x, rel_y);

      if (self->active_crop_handle == HANDLE_CENTER) {
          self->drag_type = DRAG_TYPE_CROP_MOVE;
      } else if (self->active_crop_handle != HANDLE_NONE) {
          self->drag_type = DRAG_TYPE_CROP_RESIZE;
      } else {
          self->drag_type = DRAG_TYPE_NONE;
          return;
      }

      self->drag_start_x = rel_x;
      self->drag_start_y = rel_y;
      self->drag_obj_start_x = self->crop_x;
      self->drag_obj_start_y = self->crop_y;
      self->drag_obj_start_scale = self->crop_w;
      self->drag_obj_start_h = self->crop_h;
      return;
  }

  // LAYER DRAG LOGIC
  for (l = g_list_last(self->layers); l != NULL; l = l->prev) {
    layer = (ImageLayer *)l->data;

    half_w_scaled = (layer->width * layer->scale) / (2.0 * img_w);
    half_h_scaled = (layer->height * layer->scale) / (2.0 * img_h);

    left = layer->x - half_w_scaled;
    right = layer->x + half_w_scaled;
    top = layer->y - half_h_scaled;
    bottom = layer->y + half_h_scaled;

    corner_x = 20.0 / (img_w * screen_scale);
    corner_y = 20.0 / (img_h * screen_scale);

    if (layer == self->selected_layer) {
      near_corner =
        (fabs(rel_x - left) < corner_x || fabs(rel_x - right) < corner_x) &&
        (fabs(rel_y - top) < corner_y || fabs(rel_y - bottom) < corner_y);

      if (near_corner) {
        push_undo (self);
        self->drag_type = DRAG_TYPE_IMAGE_RESIZE;
        self->selected_layer = layer;
        self->drag_obj_start_scale = layer->scale;
        self->drag_start_x = rel_x * img_w;
        self->drag_start_y = rel_y * img_h;
        sync_ui_with_layer(self);
        render_meme(self);
        return;
      }
    }

    if (rel_x >= left && rel_x <= right && rel_y >= top && rel_y <= bottom) {
      push_undo (self);
      self->drag_type = DRAG_TYPE_IMAGE_MOVE;
      self->selected_layer = layer;
      self->drag_obj_start_x = layer->x;
      self->drag_obj_start_y = layer->y;
      self->drag_start_x = rel_x;
      self->drag_start_y = rel_y;
      sync_ui_with_layer(self);
      render_meme(self);
      return;
    }
  }


  if (self->selected_layer) {
      self->selected_layer = NULL;
      sync_ui_with_layer(self);
  }

  self->drag_start_x = rel_x;
  self->drag_start_y = rel_y;
  render_meme(self);
}

static void
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self) {
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

  if (self->drag_type == DRAG_TYPE_CROP_MOVE) {
      self->crop_x = CLAMP(self->drag_obj_start_x + delta_x, 0.0, 1.0 - self->crop_w);
      self->crop_y = CLAMP(self->drag_obj_start_y + delta_y, 0.0, 1.0 - self->crop_h);
      render_meme(self);
      return;
  }
  // UPDATED: 8-WAY RESIZE MATH
  else if (self->drag_type == DRAG_TYPE_CROP_RESIZE) {
      double new_x = self->drag_obj_start_x;
      double new_y = self->drag_obj_start_y;
      double new_w = self->drag_obj_start_scale;
      double new_h = self->drag_obj_start_h;

      // Horizontal
      if (self->active_crop_handle == HANDLE_LEFT || 
          self->active_crop_handle == HANDLE_TOP_LEFT || 
          self->active_crop_handle == HANDLE_BOTTOM_LEFT) {
          
          double max_right = self->drag_obj_start_x + self->drag_obj_start_scale;
          new_x = CLAMP(self->drag_obj_start_x + delta_x, 0.0, max_right - 0.05);
          new_w = max_right - new_x;
      } 
      else if (self->active_crop_handle == HANDLE_RIGHT || 
               self->active_crop_handle == HANDLE_TOP_RIGHT || 
               self->active_crop_handle == HANDLE_BOTTOM_RIGHT) {
               
          new_w = CLAMP(self->drag_obj_start_scale + delta_x, 0.05, 1.0 - new_x);
      }

      // Vertical
      if (self->active_crop_handle == HANDLE_TOP || 
          self->active_crop_handle == HANDLE_TOP_LEFT || 
          self->active_crop_handle == HANDLE_TOP_RIGHT) {
          
          double max_bottom = self->drag_obj_start_y + self->drag_obj_start_h;
          new_y = CLAMP(self->drag_obj_start_y + delta_y, 0.0, max_bottom - 0.05);
          new_h = max_bottom - new_y;
      } 
      else if (self->active_crop_handle == HANDLE_BOTTOM || 
               self->active_crop_handle == HANDLE_BOTTOM_LEFT || 
               self->active_crop_handle == HANDLE_BOTTOM_RIGHT) {
               
          new_h = CLAMP(self->drag_obj_start_h + delta_y, 0.05, 1.0 - new_y);
      }

      self->crop_x = new_x;
      self->crop_y = new_y;
      self->crop_w = new_w;
      self->crop_h = new_h;
      render_meme(self);
      return;
  }

  if (self->drag_type == DRAG_TYPE_IMAGE_MOVE && self->selected_layer) {
    self->selected_layer->x = CLAMP (self->drag_obj_start_x + delta_x, 0.0, 1.0);
    self->selected_layer->y = CLAMP (self->drag_obj_start_y + delta_y, 0.0, 1.0);
  }
  else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE && self->selected_layer) {
    double cx, cy, start_dx, start_dy, start_dist;
    double current_img_x, current_img_y, cur_dx, cur_dy, cur_dist;
    double ratio;

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
      ratio = cur_dist / start_dist;
      self->selected_layer->scale = CLAMP (self->drag_obj_start_scale * ratio, 0.1, 5.0);
    }
  }

  render_meme (self);
}

static void on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self) {
  self->drag_type = DRAG_TYPE_NONE;
}

static void on_text_changed (MyappWindow *self) { if (self->template_image) render_meme (self); }

static void
on_add_image_response (GObject *source, GAsyncResult *result, gpointer user_data) {
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
    push_undo (self);
    new_layer->width = gdk_pixbuf_get_width (new_layer->pixbuf);
    new_layer->height = gdk_pixbuf_get_height (new_layer->pixbuf);
    new_layer->x = 0.5;
    new_layer->y = 0.5;
    new_layer->scale = 1.0;
    new_layer->opacity = 1.0;
    new_layer->rotation = 0.0;
    new_layer->blend_mode = BLEND_NORMAL;

    self->layers = g_list_append (self->layers, new_layer);
    self->selected_layer = new_layer;
    sync_ui_with_layer(self);
    render_meme (self);
  } else {
    g_free (new_layer);
  }
  g_free (path); g_object_unref (file);
}

// function for add images
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
  g_clear_object (&self->template_image);
  g_clear_object (&self->final_meme);

  if (self->layers) {
    g_list_free_full (self->layers, free_image_layer);
    self->layers = NULL;
  }

  free_history_stack (&self->undo_stack);
  free_history_stack (&self->redo_stack);

  self->selected_layer = NULL;
  sync_ui_with_layer(self);

  gtk_image_clear (self->meme_preview);
  gtk_image_set_from_icon_name (self->meme_preview, "image-x-generic-symbolic");

  gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), FALSE);
  gtk_toggle_button_set_active (self->deep_fry_button, FALSE);
  gtk_toggle_button_set_active (self->cinematic_button, FALSE);
  gtk_flow_box_unselect_all (self->template_gallery);
}

static GdkPixbuf *
apply_saturation_contrast (GdkPixbuf *src, double sat, double contrast) {
  GdkPixbuf *copy;
  int w, h, stride, n_channels;
  int x, y;
  guchar *pixels, *row_ptr, *p;
  double r, g, b, gray;
  double one_minus_sat = 1.0 - sat;

  if (src == NULL) return NULL;

  copy = gdk_pixbuf_copy (src);
  if (copy == NULL) return NULL;

  w = gdk_pixbuf_get_width (copy);
  h = gdk_pixbuf_get_height (copy);
  stride = gdk_pixbuf_get_rowstride (copy);
  n_channels = gdk_pixbuf_get_n_channels (copy);
  pixels = gdk_pixbuf_get_pixels (copy);

  for (y = 0; y < h; y++) {
    row_ptr = pixels + (y * stride);
    for (x = 0; x < w; x++) {
      p = row_ptr + (x * n_channels);

      r = (double)p[0];
      g = (double)p[1];
      b = (double)p[2];

      gray = 0.299 * r + 0.587 * g + 0.114 * b;
      r = gray * one_minus_sat + r * sat;
      g = gray * one_minus_sat + g * sat;
      b = gray * one_minus_sat + b * sat;

      r = (r - 128.0) * contrast + 128.0;
      g = (g - 128.0) * contrast + 128.0;
      b = (b - 128.0) * contrast + 128.0;

      p[0] = CLAMP_U8((int)r);
      p[1] = CLAMP_U8((int)g);
      p[2] = CLAMP_U8((int)b);
    }
  }

  return copy;
}

//apply filter (Deep Fry)
static GdkPixbuf *
apply_deep_fry (GdkPixbuf *src) {
  GdkPixbuf *fried = gdk_pixbuf_copy (src);
  int width = gdk_pixbuf_get_width (fried);
  int height = gdk_pixbuf_get_height (fried);
  int n_channels = gdk_pixbuf_get_n_channels (fried);
  int rowstride = gdk_pixbuf_get_rowstride (fried);

  GdkPixbuf *shrunk, *final;
  guchar *pixels;
  double contrast = 2.0;
  int noise_level = 30;
  int x, y;
  int i, noise, val;
  guchar *row, *p;
  double c_val;

  pixels = gdk_pixbuf_get_pixels (fried);

  for (y = 0; y < height; y++) {
    row = pixels + y * rowstride;
    for (x = 0; x < width; x++) {
      p = row + x * n_channels;

      for (i = 0; i < 3; i++) {
        noise = (rand() % (noise_level * 2 + 1)) - noise_level;
        val = p[i] + noise;
        c_val = ((double)val - 128.0) * contrast + 128.0;
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

static void render_meme (MyappWindow *self) {
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;
  GdkPixbuf *composite_pixbuf;
  GList *l;
  ImageLayer *layer;
  double draw_x, draw_y;
  GdkPixbuf *cinematic;
  GdkPixbuf *fried;

  if (self->template_image == NULL) return;

  width = gdk_pixbuf_get_width (self->template_image);
  height = gdk_pixbuf_get_height (self->template_image);


  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  gdk_cairo_set_source_pixbuf (cr, self->template_image, 0.0, 0.0);
  cairo_paint (cr);

  for (l = self->layers; l != NULL; l = l->next) {
    layer = (ImageLayer *)l->data;
    draw_x = layer->x * width;
    draw_y = layer->y * height;

    cairo_save (cr);
    cairo_translate (cr, draw_x, draw_y);
    cairo_rotate (cr, layer->rotation);
    cairo_scale (cr, layer->scale, layer->scale);

    if (layer->blend_mode == BLEND_MULTIPLY) cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);
    else if (layer->blend_mode == BLEND_SCREEN) cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
    else if (layer->blend_mode == BLEND_OVERLAY) cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
    else cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (layer->type == LAYER_TYPE_IMAGE && layer->pixbuf) {
       gdk_cairo_set_source_pixbuf (cr, layer->pixbuf, -layer->width/2.0, -layer->height/2.0);
       if (layer->opacity < 1.0) cairo_paint_with_alpha (cr, layer->opacity);
       else cairo_paint (cr);
    }
    else if (layer->type == LAYER_TYPE_TEXT && layer->text) {
       cairo_text_extents_t extents;
       cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
       cairo_set_font_size (cr, layer->font_size);
       cairo_text_extents (cr, layer->text, &extents);

       layer->width = extents.width + 10;
       layer->height = extents.height + 10;

       cairo_move_to (cr, -(extents.width/2.0 + extents.x_bearing), -(extents.height/2.0 + extents.y_bearing));
       cairo_text_path (cr, layer->text);

       cairo_set_source_rgba (cr, 0, 0, 0, layer->opacity);
       cairo_set_line_width (cr, layer->font_size * 0.08);
       cairo_stroke_preserve (cr);

       cairo_set_source_rgba (cr, 1, 1, 1, layer->opacity);
       cairo_fill (cr);
    }
    cairo_restore (cr);
  }

  cairo_surface_flush (surface);
  cairo_destroy (cr);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  composite_pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_surface_destroy (surface);

  if (composite_pixbuf == NULL) return;


  if (self->drag_type == DRAG_TYPE_NONE) {
    if (gtk_toggle_button_get_active(self->cinematic_button)) {
        cinematic = apply_saturation_contrast(composite_pixbuf, 1.15, 1.05);
        if (cinematic) { g_object_unref(composite_pixbuf); composite_pixbuf = cinematic; }
    }
    if (gtk_toggle_button_get_active (self->deep_fry_button)) {
        fried = apply_deep_fry (composite_pixbuf);
        if (fried) { g_object_unref (composite_pixbuf); composite_pixbuf = fried; }
    }
  }


  g_clear_object (&self->final_meme);
  self->final_meme = g_object_ref(composite_pixbuf);

  cairo_surface_t *overlay_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_t *cr_ov = cairo_create(overlay_surf);


  gdk_cairo_set_source_pixbuf(cr_ov, composite_pixbuf, 0, 0);
  cairo_paint(cr_ov);


  if (gtk_toggle_button_get_active(self->crop_mode_button)) {
      double cx = self->crop_x * width;
      double cy = self->crop_y * height;
      double cw = self->crop_w * width;
      double ch = self->crop_h * height;

      // Darken outside
      cairo_set_source_rgba(cr_ov, 0, 0, 0, 0.6);
      if (cy > 0) { cairo_rectangle(cr_ov, 0, 0, width, cy); cairo_fill(cr_ov); }
      if (cy + ch < height) { cairo_rectangle(cr_ov, 0, cy + ch, width, height - (cy + ch)); cairo_fill(cr_ov); }
      if (cx > 0) { cairo_rectangle(cr_ov, 0, cy, cx, ch); cairo_fill(cr_ov); }
      if (cx + cw < width) { cairo_rectangle(cr_ov, cx + cw, cy, width - (cx + cw), ch); cairo_fill(cr_ov); }

      // Selection box
      cairo_set_source_rgba(cr_ov, 1, 1, 1, 0.9);
      cairo_set_line_width(cr_ov, 2.0);
      cairo_rectangle(cr_ov, cx, cy, cw, ch);
      cairo_stroke(cr_ov);

      // Rule of thirds lines
      cairo_set_source_rgba(cr_ov, 1, 1, 1, 0.3);
      cairo_set_line_width(cr_ov, 1.0);
      cairo_move_to(cr_ov, cx + cw/3.0, cy); cairo_line_to(cr_ov, cx + cw/3.0, cy + ch);
      cairo_move_to(cr_ov, cx + 2*cw/3.0, cy); cairo_line_to(cr_ov, cx + 2*cw/3.0, cy + ch);
      cairo_move_to(cr_ov, cx, cy + ch/3.0); cairo_line_to(cr_ov, cx + cw, cy + ch/3.0);
      cairo_move_to(cr_ov, cx, cy + 2*ch/3.0); cairo_line_to(cr_ov, cx + cw, cy + 2*ch/3.0);
      cairo_stroke(cr_ov);

      // NEW: Draw visual handles (dots)
      double handle_r = 5.0; // visual size
      cairo_set_source_rgba(cr_ov, 1, 1, 1, 1); 
      
      cairo_arc(cr_ov, cx, cy, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // TL
      cairo_arc(cr_ov, cx + cw, cy, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // TR
      cairo_arc(cr_ov, cx, cy + ch, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // BL
      cairo_arc(cr_ov, cx + cw, cy + ch, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // BR
      
      cairo_arc(cr_ov, cx + cw/2.0, cy, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // T
      cairo_arc(cr_ov, cx + cw/2.0, cy + ch, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // B
      cairo_arc(cr_ov, cx, cy + ch/2.0, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // L
      cairo_arc(cr_ov, cx + cw, cy + ch/2.0, handle_r, 0, 2*M_PI); cairo_fill(cr_ov); // R
  }
  else if (self->selected_layer) {
      ImageLayer *sl = self->selected_layer;
      double sx = sl->x * width;
      double sy = sl->y * height;
      double box_w = sl->width * sl->scale;
      double box_h = sl->height * sl->scale;

      cairo_save(cr_ov);
      cairo_translate(cr_ov, sx, sy);
      cairo_rotate(cr_ov, sl->rotation);
      cairo_set_source_rgba(cr_ov, 0.4, 0.2, 0.8, 0.8);
      cairo_set_line_width(cr_ov, 2.0);
      cairo_rectangle(cr_ov, -box_w/2.0, -box_h/2.0, box_w, box_h);
      cairo_stroke(cr_ov);
      cairo_restore(cr_ov);
  }

  cairo_destroy(cr_ov);
  g_object_unref(composite_pixbuf);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  composite_pixbuf = gdk_pixbuf_get_from_surface(overlay_surf, 0, 0, width, height);
  G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_surface_destroy(overlay_surf);

  if (composite_pixbuf) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    texture = gdk_texture_new_for_pixbuf (composite_pixbuf);
    G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_image_set_from_paintable (self->meme_preview, GDK_PAINTABLE (texture));
    g_object_unref (texture);
    g_object_unref (composite_pixbuf);
  }
}

//button for deep fry
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

  free_history_stack (&self->undo_stack);
  free_history_stack (&self->redo_stack);

  if (self->template_image) {
    gtk_stack_set_visible_child_name (self->content_stack, "content");

    gtk_widget_set_sensitive (GTK_WIDGET (self->add_text_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->deep_fry_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->cinematic_button), TRUE);
    render_meme (self);
  }
  g_free (path); g_object_unref (file);
}


//image click function
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
    GdkPixbuf *to_save = NULL;
    if (gtk_toggle_button_get_active(self->crop_mode_button)) {
        int iw = gdk_pixbuf_get_width(self->final_meme);
        int ih = gdk_pixbuf_get_height(self->final_meme);
        int x = (int)(self->crop_x * iw);
        int y = (int)(self->crop_y * ih);
        int w = (int)(self->crop_w * iw);
        int h = (int)(self->crop_h * ih);

        //sanity check???
        // damn
        if (w > 0 && h > 0 && x >= 0 && y >= 0 && (x + w) <= iw && (y + h) <= ih) {
            to_save = gdk_pixbuf_new_subpixbuf(self->final_meme, x, y, w, h);
        } else {

            to_save = g_object_ref(self->final_meme);
        }
    } else {
        to_save = g_object_ref(self->final_meme);
    }

    gdk_pixbuf_save (to_save, g_file_get_path (file), "png", NULL, NULL);

    if (to_save) g_object_unref (to_save);
    g_object_unref (file);
  }
}
//why is the gtk and libadwaita function are so long
// why cant they just be like .title("title") like
// rust does it, ts id so ass

static void on_export_clicked (MyappWindow *self) {
  GtkFileDialog *dialog;
  if (!self->final_meme) return;
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Save Meme");
  gtk_file_dialog_set_initial_name (dialog, "meme.png");
  gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_export_response, self);
}