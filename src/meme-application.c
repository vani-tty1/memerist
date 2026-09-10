/* meme-application.c
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
#include "meme-welcome-dialog.h"
#include "config.h"
#include <epoxy/gl.h>
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glibconfig.h"
#include <sys/stat.h>
#include "meme-fileio.h"

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
  gboolean is_new_window;

  g_assert (MEME_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  is_new_window = (window == NULL);
  if (window == NULL)
    window = g_object_new (MEME_TYPE_WINDOW,
                           "application", app,
                           NULL);

  gtk_window_present (window);

  if (is_new_window)
    meme_maybe_show_welcome_dialog (window);
}

static void
meme_application_about_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
    MemeApplication *self = user_data;
    GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (self));
    const char *vendor = "Unknown";
    const char *renderer = "Unknown";
    const char *version = "Unknown";
    GdkDisplay *display;
    GError *err = NULL;
    GdkGLContext *ctx;
    g_autofree char *debug_info = NULL;
    g_autofree char *os_release_content = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree char *debug_text = NULL;
    AdwDialog *dialog;
    static const char *developers[] = {"Giovanni Rafanan <giovannirafanan609@gmail.com>", NULL};
    static const char *designers[] = {"Giovanni Rafanan <giovannirafanan609@gmail.com>", NULL};

    display = gdk_display_get_default();
    ctx = gdk_display_create_gl_context(display, &err);

    if (ctx && gdk_gl_context_realize(ctx, &err)) {
        gdk_gl_context_make_current(ctx);
        vendor = (const char *)glGetString(GL_VENDOR);
        renderer = (const char *)glGetString(GL_RENDERER);
        version = (const char *)glGetString(GL_VERSION);
    } else {
        if (err) g_clear_error(&err);
    }

    debug_info = g_strdup_printf("GPU Vendor: %s\nGPU Renderer: %s\nOpenGL Version: %s",
                                  vendor ? vendor : "Unknown",
                                  renderer ? renderer : "Unknown",
                                  version ? version : "Unknown");

    if (ctx) {
        gdk_gl_context_clear_current();
        g_object_unref(ctx);
    }

    if (!g_file_get_contents("/etc/os-release", &os_release_content, NULL, &error)) {
        os_release_content = g_strdup("Could not read /etc/os-release");
        g_clear_error(&error);
    }

    debug_text = g_strdup_printf (
        "Memerist %s\n"
        "GTK version: %d.%d.%d\n"
        "--- GPU Info ---\n"
        "%s\n\n"
        "--- Runtime info ---\n"
        "%s",
        PACKAGE_VERSION,
        gtk_get_major_version (),
        gtk_get_minor_version (),
        gtk_get_micro_version (),
        debug_info,
        os_release_content
    );

    dialog = adw_about_dialog_new ();

    adw_about_dialog_set_application_name (ADW_ABOUT_DIALOG (dialog), "Memerist");
    adw_about_dialog_set_application_icon (ADW_ABOUT_DIALOG (dialog), "io.github.vani_tty1.memerist");
    adw_about_dialog_set_version (ADW_ABOUT_DIALOG (dialog), PACKAGE_VERSION);
    adw_about_dialog_set_comments (ADW_ABOUT_DIALOG (dialog), "Create memes with text overlays");
    adw_about_dialog_set_developer_name (ADW_ABOUT_DIALOG (dialog), "Giovanni Rafanan");
    adw_about_dialog_set_license_type (ADW_ABOUT_DIALOG (dialog), GTK_LICENSE_GPL_3_0);
    adw_about_dialog_set_copyright (ADW_ABOUT_DIALOG (dialog), "© 2025 Giovanni Rafanan");

    adw_about_dialog_set_website (ADW_ABOUT_DIALOG (dialog), "https://github.com/vani-tty1/memerist");
    adw_about_dialog_set_issue_url (ADW_ABOUT_DIALOG (dialog), "https://github.com/vani-tty1/memerist/issues");
    adw_about_dialog_set_support_url (ADW_ABOUT_DIALOG (dialog), "https://github.com/vani-tty1/memerist/discussions");

    adw_about_dialog_set_developers(ADW_ABOUT_DIALOG(dialog), developers);
    adw_about_dialog_set_designers(ADW_ABOUT_DIALOG(dialog), designers);
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

static void
meme_application_welcome_action (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  MemeApplication *self = user_data;
  GtkWindow *parent = gtk_application_get_active_window (GTK_APPLICATION (self));

  if (parent)
    meme_show_welcome_dialog (parent);
}

static const GActionEntry app_actions[] = {
  { "quit", meme_application_quit_action },
  { "about", meme_application_about_action },
  { "shortcuts", meme_application_shortcuts_action },
  { "welcome", meme_application_welcome_action },
  { "color-scheme", meme_application_color_scheme_action, "s", "'default'", NULL },
};

static void
meme_application_startup (GApplication *app)
{

  G_APPLICATION_CLASS (meme_application_parent_class)->startup (app);
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
  G_APPLICATION_CLASS (meme_application_parent_class)->shutdown (app);
}

static void
meme_application_open (GApplication  *app,
                        GFile        **files,
                        int            n_files,
                        const char    *hint)
{
  GtkWindow *window;

  g_assert (MEME_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    window = g_object_new (MEME_TYPE_WINDOW, "application", app, NULL);

  gtk_window_present (window);

  if (n_files > 0)
    meme_window_open_file (MEME_WINDOW (window), files[0]);
}

static void
meme_application_class_init (MemeApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->startup  = meme_application_startup;
  app_class->activate = meme_application_activate;
  app_class->open     = meme_application_open;
  app_class->shutdown = meme_application_shutdown;
}

static void
meme_application_init (MemeApplication *self)
{}



#define SHORTCUT_MODS (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK)

static gboolean
is_exact_mods (GdkModifierType state, GdkModifierType wanted) {
    return (state & SHORTCUT_MODS) == wanted;
}


static gboolean
is_editing_text (MemeWindow *self) {
    GtkWidget *focus = GTK_WIDGET (gtk_root_get_focus (GTK_ROOT (self)));
    if (!focus)
        return FALSE;

    if (GTK_IS_TEXT (focus) || GTK_IS_TEXT_VIEW (focus))
        return TRUE;
    if (self->layer_text_view &&
        gtk_widget_is_ancestor (focus, GTK_WIDGET (self->layer_text_view)))
        return TRUE;
    if (self->footer_layer_text_view &&
        gtk_widget_is_ancestor (focus, GTK_WIDGET (self->footer_layer_text_view)))
        return TRUE;

    return FALSE;
}

gboolean on_window_key_pressed (GtkEventControllerKey *controller,
                                guint keyval,
                                guint keycode,
                                GdkModifierType state,
                                MemeWindow *self) {
    gboolean editing_text = is_editing_text (self);

    // Ctrl + Z = Undo
    if (!editing_text && is_exact_mods (state, GDK_CONTROL_MASK) &&
        (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
        myapp_window_perform_undo (self);
        return TRUE;
    }

    // Ctrl + Y = Redo
    if (!editing_text && is_exact_mods (state, GDK_CONTROL_MASK) &&
        (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) {
        myapp_window_perform_redo (self);
        return TRUE;
    }

    // Ctrl + S = Save
    if (is_exact_mods (state, GDK_CONTROL_MASK) &&
        (keyval == GDK_KEY_s || keyval == GDK_KEY_S)) {
        myapp_window_save_project (self);
        return TRUE;
    }

    // Ctrl + E = Export
    if (is_exact_mods (state, GDK_CONTROL_MASK) &&
        (keyval == GDK_KEY_e || keyval == GDK_KEY_E)) {
        on_export_clicked (self);
        return TRUE;
    }
    
    if (is_exact_mods (state, GDK_SHIFT_MASK) &&
        keyval == GDK_KEY_Delete) {
        on_clear_clicked (self);
        return TRUE;
    }


    if (!editing_text &&
        (keyval == GDK_KEY_BackSpace || keyval == GDK_KEY_Delete)) {
        if (self->selected_layer) {
            gtk_widget_activate (GTK_WIDGET (self->delete_layer_button));
            return TRUE;
        }
    }


    if (is_exact_mods (state, GDK_CONTROL_MASK | GDK_SHIFT_MASK) &&
        (keyval == GDK_KEY_c || keyval == GDK_KEY_C)) {
        on_copy_clipboard_clicked (self);
        return TRUE;
    }

    if (!editing_text && is_exact_mods (state, GDK_CONTROL_MASK) &&
        (keyval == GDK_KEY_v || keyval == GDK_KEY_V)) {
        meme_window_paste_from_clipboard (self);
        return TRUE;
    }

    return FALSE;
}
