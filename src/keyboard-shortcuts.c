#include "keyboard-shortcuts.h"
#include "meme-window.h"


gboolean on_window_key_pressed (GtkEventControllerKey *controller,
                                guint keyval,
                                guint keycode,
                                GdkModifierType state,
                                MyappWindow *self) {
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

  return FALSE;
}