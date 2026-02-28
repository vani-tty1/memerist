#pragma once
#include <gtk/gtk.h>
#include "meme-window.h"

gboolean on_window_key_pressed (GtkEventControllerKey *controller,
                                guint keyval,
                                guint keycode,
                                GdkModifierType state,
                                MyappWindow *self);

