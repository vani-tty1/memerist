/* myapp-application.c
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
#include "meme-application.h"
#include "adwaita.h"
#include "meme-window.h"
#include "meme-gpu.h"
#include "config.h"

struct _MemeApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (MemeApplication, meme_application, ADW_TYPE_APPLICATION)

MemeApplication *
meme_application_new (const char        *application_id,
                       GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (MEME_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       "resource-base-path", "/io/github/vani_tty1/memerist",
                       NULL);
}

static void
meme_application_activate (GApplication *app)
{
  GtkWindow *window;

  g_assert (MEME_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    window = g_object_new (MEME_TYPE_WINDOW,
                           "application", app,
                           NULL);

  gtk_window_present (window);
}

static void
meme_application_about_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
    MemeApplication *self = user_data;
    GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (self));
    
    g_autofree char *os_release_content = NULL;
    g_autoptr(GError) error = NULL;
    
    if (!g_file_get_contents("/etc/os-release", &os_release_content, NULL, &error)) {
        os_release_content = g_strdup("Could not read /etc/os-release");
        g_clear_error(&error);
    }
        
    g_autofree char *debug_text = g_strdup_printf (
        "Memerist %s\n"
        "GTK version: %d.%d.%d\n"
        "--- Runtime info ---\n"
        "%s",
        PACKAGE_VERSION,
        gtk_get_major_version (),
        gtk_get_minor_version (),
        gtk_get_micro_version (),
        os_release_content
    );
    
    AdwDialog *dialog = adw_about_dialog_new_from_appdata(
        "/io/github/vani_tty1/memerist/io.github.vani_tty1.memerist.metainfo.xml", 
        PACKAGE_VERSION
    );
    
    static const char *developers[] = {"vani-tty1", NULL};
    static const char *designers[] = {"vani-tty1", NULL};
    
    adw_about_dialog_set_developers(ADW_ABOUT_DIALOG(dialog), developers);
    adw_about_dialog_set_designers(ADW_ABOUT_DIALOG(dialog), designers);
    adw_about_dialog_set_version(ADW_ABOUT_DIALOG(dialog), PACKAGE_VERSION);
    adw_about_dialog_set_debug_info(ADW_ABOUT_DIALOG(dialog), debug_text);
    adw_about_dialog_set_debug_info_filename(ADW_ABOUT_DIALOG(dialog), "meme-debug.txt");
    
    adw_dialog_present(dialog, GTK_WIDGET(window));
}




static void
meme_application_quit_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  MemeApplication *self = user_data;

  g_assert (MEME_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}


static void
meme_application_shortcuts_action (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  MemeApplication *self = user_data;
  GtkWindow *parent = gtk_application_get_active_window (GTK_APPLICATION (self));
  GtkBuilder *builder;
  AdwDialog *shortcuts_window;
  builder = gtk_builder_new_from_resource ("/io/github/vani_tty1/memerist/shortcuts-dialog.ui");
  shortcuts_window = ADW_DIALOG (gtk_builder_get_object (builder, "shortcuts_dialog"));
  if (parent) {
        adw_dialog_present (ADW_DIALOG (shortcuts_window), GTK_WIDGET (parent));
  }
  g_object_unref (builder);
}

static void
meme_application_color_scheme_action (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  const char *scheme = g_variant_get_string (parameter, NULL);

  if (g_strcmp0 (scheme, "light") == 0) {
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
  } else if (g_strcmp0 (scheme, "dark") == 0) {
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_DARK);
  } else {
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_DEFAULT);
  }

  g_simple_action_set_state (action, parameter);
}

static const GActionEntry app_actions[] = {
  { "quit", meme_application_quit_action },
  { "about", meme_application_about_action },
  { "shortcuts", meme_application_shortcuts_action },
  { "color-scheme", meme_application_color_scheme_action, "s", "'default'", NULL },
};

static void
meme_application_startup (GApplication *app)
{

  G_APPLICATION_CLASS (meme_application_parent_class)->startup (app);

  meme_gpu_init (NULL);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   app);


  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.quit",
                                         (const char *[]) { "<Control>q", NULL });
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.shortcuts",
                                         (const char *[]) { "<Control>question", NULL });
}

static void
meme_application_shutdown (GApplication *app)
{
  meme_gpu_cleanup ();
  G_APPLICATION_CLASS (meme_application_parent_class)->shutdown (app);
}

static void
meme_application_class_init (MemeApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->startup  = meme_application_startup;
  app_class->activate = meme_application_activate;
  app_class->shutdown = meme_application_shutdown;
}

static void
meme_application_init (MemeApplication *self)
{}