#include "config.h"
#include "about.h"

void
show_about_dialog (GtkWindow *parent)
{
  static const char *developers[] = {"vani-tty1", NULL};

  adw_show_about_dialog (GTK_WIDGET (parent),
                         "application-name", "Meme Editor",
                         "application-icon", "io.github.vani_tty1.memerist",
                         "developer-name", "vani-tty1",
                         "version", PACKAGE_VERSION,
                         "developers", developers,
                         "copyright", "©2026 vani-tty1",
                         NULL);
}