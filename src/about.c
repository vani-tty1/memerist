#include "config.h"
#include "gtk/gtk.h"
#include "about.h"

void show_about_dialog (GtkWindow *parent) {
  static const char *developers[] = {"vani-tty1", NULL};
  static const char *designers[] = {"vani-tty1", NULL};
    
  GBytes *notes_bytes = g_resources_lookup_data("/io/github/vani_tty1/memerist/release.html", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  const char *release_notes = notes_bytes ? g_bytes_get_data(notes_bytes, NULL) : "";
  
  
  g_autofree char *os_release_content = NULL;
  g_autoptr(GError) error = NULL;

  if (!g_file_get_contents("/etc/os-release", &os_release_content, NULL, &error)) {
    os_release_content = g_strdup("Could not read /etc/os-release");
    g_clear_error(&error);
  }
  
  g_autofree char *debug_text = g_strdup_printf (
    "Memerist %s\n"
    "GTK version: %d.%d.%d\n"
    "--- System OS Release ---\n"
    "%s",
    PACKAGE_VERSION,
    gtk_get_major_version (),
    gtk_get_minor_version (),
    gtk_get_micro_version (),
    os_release_content
  );


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
                        "debug-info", debug_text,
                        "debug-info-filename", "memerist-debug.txt",
                        "release-notes", release_notes,
                        NULL);
}
