#include "config.h"
#include "gtk/gtk.h"
#include "about.h"

void
show_about_dialog (GtkWindow *parent) {
  static const char *developers[] = {"vani-tty1", NULL};
  static const char *designers[] = {"vani-tty1", NULL};

  adw_show_about_dialog (GTK_WIDGET (parent),
                        "application-name", "Memerist",
                        "application-icon", "io.github.vani_tty1.memerist",
                        "developer-name", "vani-tty1",
                        "version", PACKAGE_VERSION,
                        "developers", developers,
                        "designers", designers,
                        "copyright", "©2026 vani-tty1",
                        "website", "https://github.com/vani-tty1/memerist",
                        "issue-url", "https://github.com/vani-tty1/memerist/issues",
                        "support-url", "https://github.com/vani-tty1/memerist/discussions",
                        "license-type", GTK_LICENSE_GPL_3_0,
                        "debug-info", "Memerist 0.3.3\nGTK4/libadwaita",
                        "debug-info-filename", "memerist-debug.txt",
                        NULL);
}