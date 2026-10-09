#include "meme-accent.h"
#include <adwaita.h>

typedef struct {
  const char *id;
  const char *bg;
  const char *light;
  const char *dark;
} AccentColor;

static const AccentColor accent_colors[] = {
  { "blue", "#3584e4", "#0461be", "#78aeed" },
  { "teal", "#2190a4", "#007184", "#7bdff4" },
  { "green", "#3a944a", "#15772e", "#8de6b1" },
  { "yellow", "#c88800", "#905400", "#f8e45c" },
  { "orange", "#ed5b00", "#c64600", "#ffa348" },
  { "red", "#e62d42", "#c01c28", "#ff7b63" },
  { "pink", "#d56199", "#a2326c", "#ffa0d0" },
  { "purple", "#9141ac", "#7a2f94", "#dc8add" },
  { "slate", "#6f8396", "#4f6376", "#c0d5ea" },
};

static GSettings *settings;
static GtkCssProvider *provider;

static void
apply_accent (void)
{
  g_autofree char *id = g_settings_get_string (settings, "accent-color");
  gboolean dark = adw_style_manager_get_dark (adw_style_manager_get_default ());
  g_autofree char *css = NULL;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (accent_colors); i++)
    {
      const AccentColor *c = &accent_colors[i];

      if (g_strcmp0 (c->id, id) != 0)
        continue;

      css = g_strdup_printf ("@define-color accent_bg_color %s;\n"
                             "@define-color accent_fg_color #ffffff;\n"
                             "@define-color accent_color %s;\n",
                             c->bg, dark ? c->dark : c->light);
      break;
    }

  gtk_css_provider_load_from_string (provider, css ? css : "");
}

static void
on_accent_changed (void)
{
  apply_accent ();
}

void
meme_accent_init (void)
{
  if (provider != NULL)
    return;

  settings = g_settings_new ("io.github.vani_tty1.memerist");
  provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
  g_signal_connect (settings, "changed::accent-color", G_CALLBACK (on_accent_changed), NULL);
  g_signal_connect (adw_style_manager_get_default (), "notify::dark", G_CALLBACK (on_accent_changed), NULL);

  apply_accent ();
}
