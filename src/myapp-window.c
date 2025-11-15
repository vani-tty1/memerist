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

#include "config.h"
#include "myapp-window.h"
#include <cairo.h>

typedef enum {
	TEXT_TYPE_NONE,
	TEXT_TYPE_TOP,
	TEXT_TYPE_BOTTOM
} TextType;

struct _MyappWindow
{
	AdwApplicationWindow parent_instance;

	GtkImage      *meme_preview;
	AdwEntryRow   *top_text_entry;
	AdwEntryRow   *bottom_text_entry;
	GtkButton     *export_button;
	GtkButton     *load_image_button;

	GdkPixbuf     *original_image;
	GdkPixbuf     *meme_pixbuf;
	char          *loaded_image_path;

	double         top_text_y;
	double         bottom_text_y;

	TextType       dragging_text;
	GtkGestureDrag *drag_gesture;
	double         drag_start_y;
};

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

static void on_text_changed (MyappWindow *self);
static void on_load_image_clicked (MyappWindow *self);
static void on_export_clicked (MyappWindow *self);
static void render_meme (MyappWindow *self);
static void draw_text_with_outline (cairo_t *cr, const char *text, double x, double y, double max_width);
static void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self);
static void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
static void on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);

static void
myapp_window_finalize (GObject *object)
{
	MyappWindow *self = MYAPP_WINDOW (object);

	g_clear_object (&self->original_image);
	g_clear_object (&self->meme_pixbuf);
	g_clear_object (&self->drag_gesture);
	g_clear_pointer (&self->loaded_image_path, g_free);

	G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static void
myapp_window_class_init (MyappWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = myapp_window_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/myapp-window.ui");

	gtk_widget_class_bind_template_child (widget_class, MyappWindow, meme_preview);
	gtk_widget_class_bind_template_child (widget_class, MyappWindow, top_text_entry);
	gtk_widget_class_bind_template_child (widget_class, MyappWindow, bottom_text_entry);
	gtk_widget_class_bind_template_child (widget_class, MyappWindow, export_button);
	gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_image_button);
}

static void
myapp_window_init (MyappWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	self->top_text_y = 0.1;
	self->bottom_text_y = 0.9;
	self->dragging_text = TEXT_TYPE_NONE;

	g_signal_connect_swapped (self->top_text_entry, "changed",
	                          G_CALLBACK (on_text_changed), self);
	g_signal_connect_swapped (self->bottom_text_entry, "changed",
	                          G_CALLBACK (on_text_changed), self);
	g_signal_connect_swapped (self->load_image_button, "clicked",
	                          G_CALLBACK (on_load_image_clicked), self);
	g_signal_connect_swapped (self->export_button, "clicked",
	                          G_CALLBACK (on_export_clicked), self);

	self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
	gtk_widget_add_controller (GTK_WIDGET (self->meme_preview),
	                           GTK_EVENT_CONTROLLER (self->drag_gesture));
	g_signal_connect (self->drag_gesture, "drag-begin",
	                 G_CALLBACK (on_drag_begin), self);
	g_signal_connect (self->drag_gesture, "drag-update",
	                 G_CALLBACK (on_drag_update), self);
	g_signal_connect (self->drag_gesture, "drag-end",
	                 G_CALLBACK (on_drag_end), self);
}

static void
on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self)
{
	int img_height;
	double relative_y;
	double top_text_abs_y;
	double bottom_text_abs_y;
	double threshold;

	if (self->original_image == NULL)
		return;

	img_height = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
	relative_y = y / img_height;

	top_text_abs_y = self->top_text_y;
	bottom_text_abs_y = self->bottom_text_y;

	threshold = 0.15;

	if (fabs (relative_y - top_text_abs_y) < threshold) {
		self->dragging_text = TEXT_TYPE_TOP;
		self->drag_start_y = y;
	} else if (fabs (relative_y - bottom_text_abs_y) < threshold) {
		self->dragging_text = TEXT_TYPE_BOTTOM;
		self->drag_start_y = y;
	} else {
		self->dragging_text = TEXT_TYPE_NONE;
	}
}

static void
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self)
{
	int img_height;
	double new_y;
	double relative_y;

	if (self->dragging_text == TEXT_TYPE_NONE || self->original_image == NULL)
		return;

	img_height = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
	new_y = self->drag_start_y + offset_y;
	relative_y = new_y / img_height;

	relative_y = CLAMP (relative_y, 0.05, 0.95);

	if (self->dragging_text == TEXT_TYPE_TOP) {
		self->top_text_y = relative_y;
	} else if (self->dragging_text == TEXT_TYPE_BOTTOM) {
		self->bottom_text_y = relative_y;
	}

	render_meme (self);
}

static void
on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self)
{
	self->dragging_text = TEXT_TYPE_NONE;
}

static void
on_text_changed (MyappWindow *self)
{
	if (self->original_image == NULL)
		return;

	render_meme (self);
}

static void
on_load_image_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	MyappWindow *self = MYAPP_WINDOW (user_data);
	GFile *file;
	GError *error = NULL;

	file = gtk_file_dialog_open_finish (dialog, result, &error);

	if (error != NULL) {
		if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
			g_warning ("Failed to open file: %s", error->message);
		g_error_free (error);
		return;
	}

	g_clear_pointer (&self->loaded_image_path, g_free);
	self->loaded_image_path = g_file_get_path (file);

	g_clear_object (&self->original_image);
	self->original_image = gdk_pixbuf_new_from_file (self->loaded_image_path, &error);

	if (error != NULL) {
		g_warning ("Failed to load image: %s", error->message);
		g_error_free (error);
		g_object_unref (file);
		return;
	}

	self->top_text_y = 0.1;
	self->bottom_text_y = 0.9;

	gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);

	render_meme (self);

	g_object_unref (file);
}

static void
on_load_image_clicked (MyappWindow *self)
{
	GtkFileDialog *dialog;
	GtkFileFilter *filter;
	GListStore *filters;

	dialog = gtk_file_dialog_new ();
	gtk_file_dialog_set_title (dialog, "Choose an Image");

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "Images");
	gtk_file_filter_add_mime_type (filter, "image/png");
	gtk_file_filter_add_mime_type (filter, "image/jpeg");
	gtk_file_filter_add_mime_type (filter, "image/gif");
	gtk_file_filter_add_mime_type (filter, "image/webp");

	filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
	g_list_store_append (filters, filter);
	gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

	gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_load_image_response, self);

	g_object_unref (filters);
	g_object_unref (filter);
}

static void
draw_text_with_outline (cairo_t *cr, const char *text, double x, double y, double max_width)
{
	cairo_text_extents_t extents;
	double font_size;

	cairo_select_font_face (cr, "Impact", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

	font_size = 60.0;
	cairo_set_font_size (cr, font_size);
	cairo_text_extents (cr, text, &extents);

	if (extents.width > max_width * 0.9) {
		font_size = (max_width * 0.9) / extents.width * font_size;
		cairo_set_font_size (cr, font_size);
		cairo_text_extents (cr, text, &extents);
	}

	x = x - extents.width / 2 - extents.x_bearing;
	y = y - extents.height / 2 - extents.y_bearing;

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 6.0);
	cairo_move_to (cr, x, y);
	cairo_text_path (cr, text);
	cairo_stroke (cr);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_move_to (cr, x, y);
	cairo_show_text (cr, text);
}

static void
render_meme (MyappWindow *self)
{
	const char *top_text;
	const char *bottom_text;
	int width;
	int height;
	cairo_surface_t *surface;
	cairo_t *cr;
	GdkTexture *texture;

	if (self->original_image == NULL)
		return;

	top_text = gtk_editable_get_text (GTK_EDITABLE (self->top_text_entry));
	bottom_text = gtk_editable_get_text (GTK_EDITABLE (self->bottom_text_entry));

	width = gdk_pixbuf_get_width (self->original_image);
	height = gdk_pixbuf_get_height (self->original_image);

	g_clear_object (&self->meme_pixbuf);
	self->meme_pixbuf = gdk_pixbuf_copy (self->original_image);

	surface = cairo_image_surface_create_for_data (
		gdk_pixbuf_get_pixels (self->meme_pixbuf),
		CAIRO_FORMAT_RGB24,
		width,
		height,
		gdk_pixbuf_get_rowstride (self->meme_pixbuf)
	);

	cr = cairo_create (surface);

	if (top_text && strlen (top_text) > 0) {
		char *upper_text = g_utf8_strup (top_text, -1);
		draw_text_with_outline (cr, upper_text, width / 2.0, height * self->top_text_y, width);
		g_free (upper_text);
	}

	if (bottom_text && strlen (bottom_text) > 0) {
		char *upper_text = g_utf8_strup (bottom_text, -1);
		draw_text_with_outline (cr, upper_text, width / 2.0, height * self->bottom_text_y, width);
		g_free (upper_text);
	}

	cairo_surface_flush (surface);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	texture = gdk_texture_new_for_pixbuf (self->meme_pixbuf);
	G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_image_set_from_paintable (self->meme_preview, GDK_PAINTABLE (texture));
	g_object_unref (texture);
}

static void
on_export_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	MyappWindow *self = MYAPP_WINDOW (user_data);
	GFile *file;
	GError *error = NULL;
	char *path;
	gboolean success;

	file = gtk_file_dialog_save_finish (dialog, result, &error);

	if (error != NULL) {
		if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
			g_warning ("Failed to save file: %s", error->message);
		g_error_free (error);
		return;
	}

	path = g_file_get_path (file);
	success = gdk_pixbuf_save (self->meme_pixbuf, path, "png", &error, NULL);

	if (!success) {
		g_warning ("Failed to save meme: %s", error->message);
		g_error_free (error);
	} else {
		g_print ("Meme saved to: %s\n", path);
	}

	g_free (path);
	g_object_unref (file);
}

static void
on_export_clicked (MyappWindow *self)
{
	GtkFileDialog *dialog;

	if (self->meme_pixbuf == NULL)
		return;

	dialog = gtk_file_dialog_new ();
	gtk_file_dialog_set_title (dialog, "Save Meme");
	gtk_file_dialog_set_initial_name (dialog, "meme.png");

	gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_export_response, self);
}
