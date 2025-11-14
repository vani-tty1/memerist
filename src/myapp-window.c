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

struct _MyappWindow
{
	AdwApplicationWindow parent_instance;

	/* Template widgets */
	GtkImage      *meme_preview;
	AdwEntryRow   *top_text_entry;
	AdwEntryRow   *bottom_text_entry;
	GtkButton     *export_button;
	GtkButton     *load_image_button;

	/* Internal data */
	GdkPixbuf     *original_image;
	GdkPixbuf     *meme_pixbuf;
};

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

static void
myapp_window_class_init (MyappWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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
}
