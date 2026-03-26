#include "keyboard-shortcuts.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glibconfig.h"
#include "meme-window.h"
#include <sys/stat.h>
#include "meme-fileio.h"


gboolean on_window_key_pressed (GtkEventControllerKey *controller,
                                guint keyval,
                                guint keycode,
                                GdkModifierType state,
                                MemeWindow *self) {
    // Ctrl + Z = Undo
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
        myapp_window_perform_undo (self);
        return TRUE;
    }

    // Ctrl + Y = Redo
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) {
        myapp_window_perform_redo (self);
        return TRUE;
    }
    // Ctrl + S = Save
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_s || keyval == GDK_KEY_S)) {
        myapp_window_save_project(self);
        return TRUE;
    }
    
    // Ctrl + E 
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_e || keyval == GDK_KEY_E)){
        on_export_clicked(self);
        return TRUE;
    }

    //Ctrl + Shift + C = copy to clipboard
    if ((state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) && 
        (keyval == GDK_KEY_s || keyval == GDK_KEY_C)) {
        on_copy_clipboard_clicked(self);
        return TRUE;
    }return FALSE;
}