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

void
meme_show_preferences_dialog (GtkWindow *parent)
{
  GtkBuilder *builder;
  AdwDialog *dialog;
  GtkWidget *reduce_quality_row;
  GtkWidget *accent_reset_button;
  GSimpleActionGroup *actions;
  GSettings *settings;

  builder = gtk_builder_new_from_resource ("/io/github/vani_tty1/memerist/preferences-dialog.ui");
  dialog = ADW_DIALOG (gtk_builder_get_object (builder, "preferences_dialog"));
  reduce_quality_row = GTK_WIDGET (gtk_builder_get_object (builder, "reduce_quality_row"));

  settings = g_settings_new ("io.github.vani_tty1.memerist");

  g_settings_bind (settings, "reduce-quality-on-drag",
                    reduce_quality_row, "active",
                    G_SETTINGS_BIND_DEFAULT);

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
