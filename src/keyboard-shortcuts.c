#include "keyboard-shortcuts.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glibconfig.h"
#include "meme-window.h"
#include <sys/stat.h>
#include "meme-fileio.h"


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

    return FALSE;
}
