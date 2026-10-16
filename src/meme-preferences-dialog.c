/* meme-preferences-dialog.c
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

#include <glib/gi18n.h>
#include <adwaita.h>
#include "meme-preferences-dialog.h"

static void
update_reset_sensitivity (GSettings *settings, const char *key, GtkWidget *button)
{
  g_autofree char *id = g_settings_get_string (settings, "accent-color");

  gtk_widget_set_sensitive (button, g_strcmp0 (id, "default") != 0);
}

static void
on_reset_clicked (GtkButton *button, GSettings *settings)
{
  g_settings_reset (settings, "accent-color");
}

typedef struct {
  GSettings *settings;
  GtkWidget *text_color_btn;
  GtkWidget *stroke_color_btn;
  GtkWidget *font_btn;
} TextPrefs;

static void on_text_color_set (GObject *object, GParamSpec *pspec, TextPrefs *prefs);
static void on_font_set (GObject *object, GParamSpec *pspec, TextPrefs *prefs);

static void
set_color_button (GtkWidget *button, GSettings *settings, const char *key,
                  gpointer handler, TextPrefs *prefs)
{
  g_autofree char *str = g_settings_get_string (settings, key);
  GdkRGBA rgba;

  if (!gdk_rgba_parse (&rgba, str))
    return;

  g_signal_handlers_block_by_func (button, handler, prefs);
  gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (button), &rgba);
  g_signal_handlers_unblock_by_func (button, handler, prefs);
}

static void
text_prefs_load (TextPrefs *prefs)
{
  g_autofree char *font = g_settings_get_string (prefs->settings, "text-font");
  PangoFontDescription *desc = pango_font_description_from_string (font[0] ? font : "Sans Bold");

  set_color_button (prefs->text_color_btn, prefs->settings, "text-color", on_text_color_set, prefs);
  set_color_button (prefs->stroke_color_btn, prefs->settings, "text-stroke-color", on_text_color_set, prefs);

  g_signal_handlers_block_by_func (prefs->font_btn, on_font_set, prefs);
  gtk_font_dialog_button_set_font_desc (GTK_FONT_DIALOG_BUTTON (prefs->font_btn), desc);
  g_signal_handlers_unblock_by_func (prefs->font_btn, on_font_set, prefs);
  pango_font_description_free (desc);
}

static void
on_text_color_set (GObject *object, GParamSpec *pspec, TextPrefs *prefs)
{
  const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (object));
  const char *key = (GTK_WIDGET (object) == prefs->text_color_btn) ? "text-color" : "text-stroke-color";
  g_autofree char *str = NULL;

  if (!rgba)
    return;

  str = gdk_rgba_to_string (rgba);
  g_settings_set_string (prefs->settings, key, str);
}

static void
on_font_set (GObject *object, GParamSpec *pspec, TextPrefs *prefs)
{
  PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc (GTK_FONT_DIALOG_BUTTON (object));
  g_autofree char *str = NULL;

  if (!desc)
    return;

  str = pango_font_description_to_string (desc);
  g_settings_set_string (prefs->settings, "text-font", str);
}

static void
on_text_reset_clicked (GtkButton *button, TextPrefs *prefs)
{
  g_settings_reset (prefs->settings, "text-color");
  g_settings_reset (prefs->settings, "text-stroke-color");
  g_settings_reset (prefs->settings, "text-font");
  g_settings_reset (prefs->settings, "text-font-size");
  text_prefs_load (prefs);
}

void
meme_show_preferences_dialog (GtkWindow *parent)
{
  GtkBuilder *builder;
  AdwDialog *dialog;
  GtkWidget *reduce_quality_row;
  GtkWidget *accent_reset_button;
  GtkWidget *remember_text_row, *font_size_row, *text_reset_button;
  GSimpleActionGroup *actions;
  GSettings *settings;
  TextPrefs *text_prefs;

  builder = gtk_builder_new_from_resource ("/io/github/vani_tty1/memerist/preferences-dialog.ui");
  dialog = ADW_DIALOG (gtk_builder_get_object (builder, "preferences_dialog"));
  reduce_quality_row = GTK_WIDGET (gtk_builder_get_object (builder, "reduce_quality_row"));

  settings = g_settings_new ("io.github.vani_tty1.memerist");

  g_settings_bind (settings, "reduce-quality-on-drag",
                    reduce_quality_row, "active",
                    G_SETTINGS_BIND_DEFAULT);

  remember_text_row = GTK_WIDGET (gtk_builder_get_object (builder, "remember_text_row"));
  font_size_row = GTK_WIDGET (gtk_builder_get_object (builder, "default_font_size_row"));
  text_reset_button = GTK_WIDGET (gtk_builder_get_object (builder, "text_reset_button"));

  g_settings_bind (settings, "remember-text-style",
                   remember_text_row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "text-font-size",
                   font_size_row, "value",
                   G_SETTINGS_BIND_DEFAULT);

  text_prefs = g_new0 (TextPrefs, 1);
  text_prefs->settings = settings;
  text_prefs->text_color_btn = GTK_WIDGET (gtk_builder_get_object (builder, "default_text_color_btn"));
  text_prefs->stroke_color_btn = GTK_WIDGET (gtk_builder_get_object (builder, "default_stroke_color_btn"));
  text_prefs->font_btn = GTK_WIDGET (gtk_builder_get_object (builder, "default_font_btn"));

  text_prefs_load (text_prefs);
  g_signal_connect (text_prefs->text_color_btn, "notify::rgba", G_CALLBACK (on_text_color_set), text_prefs);
  g_signal_connect (text_prefs->stroke_color_btn, "notify::rgba", G_CALLBACK (on_text_color_set), text_prefs);
  g_signal_connect (text_prefs->font_btn, "notify::font-desc", G_CALLBACK (on_font_set), text_prefs);
  g_signal_connect (text_reset_button, "clicked", G_CALLBACK (on_text_reset_clicked), text_prefs);
  g_object_set_data_full (G_OBJECT (dialog), "meme-preferences-text", text_prefs, g_free);

  accent_reset_button = GTK_WIDGET (gtk_builder_get_object (builder, "accent_reset_button"));
  actions = g_simple_action_group_new ();
  g_action_map_add_action (G_ACTION_MAP (actions), g_settings_create_action (settings, "accent-color"));
  gtk_widget_insert_action_group (GTK_WIDGET (dialog), "prefs", G_ACTION_GROUP (actions));
  g_object_unref (actions);

  g_signal_connect_object (settings, "changed::accent-color",
                           G_CALLBACK (update_reset_sensitivity), accent_reset_button, 0);
  g_signal_connect (accent_reset_button, "clicked", G_CALLBACK (on_reset_clicked), settings);
  update_reset_sensitivity (settings, "accent-color", accent_reset_button);

  g_object_set_data_full (G_OBJECT (dialog), "meme-preferences-settings",
                           settings, g_object_unref);

  adw_dialog_present (dialog, GTK_WIDGET (parent));
  g_object_unref (builder);
}
