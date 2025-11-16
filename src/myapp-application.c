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

#include "config.h"
#include <glib/gi18n.h>
#include "myapp-application.h"
#include "myapp-window.h"

struct _MyappApplication
{
	AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (MyappApplication, myapp_application, ADW_TYPE_APPLICATION)

MyappApplication *
myapp_application_new (const char        *application_id,
                       GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);
	return g_object_new (MYAPP_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     "resource-base-path", "/org/gnome/Example",
	                     NULL);
}

static void
myapp_application_activate (GApplication *app)
{
	GtkWindow *window;

	g_assert (MYAPP_IS_APPLICATION (app));

	window = gtk_application_get_active_window (GTK_APPLICATION (app));
	if (window == NULL)
		window = g_object_new (MYAPP_TYPE_WINDOW,
		                       "application", app,
		                       NULL);

	gtk_window_present (window);
}

static void
myapp_application_class_init (MyappApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	app_class->activate = myapp_application_activate;
}

static void
myapp_application_about_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	static const char *developers[] = {"Giovanni", NULL};
	MyappApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (MYAPP_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "Meme Generator",
	                       "application-icon", "org.gnome.Memerist",
	                       "developer-name", "Giovanni",
	                       "translator-credits", _("translator-credits"),
	                       "version", "0.0.25-2.alpha",
	                       "developers", developers,
	                       "copyright", "© 2025 Giovanni",
	                       NULL);
}

static void
myapp_application_quit_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
	MyappApplication *self = user_data;

	g_assert (MYAPP_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static void
myapp_application_color_scheme_action (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
	AdwStyleManager *style_manager;
	const char *scheme;

	style_manager = adw_style_manager_get_default ();
	scheme = g_variant_get_string (parameter, NULL);

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
	{ "quit", myapp_application_quit_action },
	{ "about", myapp_application_about_action },
	{ "color-scheme", myapp_application_color_scheme_action, "s", "'default'", NULL },
};

static void
myapp_application_init (MyappApplication *self)
{
	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<control>q", NULL });
}
