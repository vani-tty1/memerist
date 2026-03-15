#include "adwaita.h"
#include "config.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <stdio.h>
#include "about.h"

void show_about_dialog(GtkWindow *parent) {
    g_autofree char *os_release_content = NULL;
    g_autoptr(GError) error = NULL;
    
    if (!g_file_get_contents("/etc/os-release", &os_release_content, NULL, &error)) {
        os_release_content = g_strdup("Could not read /etc/os-release");
        g_clear_error(&error);
    }
      
    g_autofree char *debug_text = g_strdup_printf (
        "Memerist %s\n"
        "GTK version: %d.%d.%d\n"
        "--- Runtime info ---\n"
        "%s",
        PACKAGE_VERSION,
        gtk_get_major_version (),
        gtk_get_minor_version (),
        gtk_get_micro_version (),
        os_release_content
    );
    
    AdwDialog *dialog = adw_about_dialog_new_from_appdata(
        "/io/github/vani_tty1/memerist/io.github.vani_tty1.memerist.metainfo.xml", 
        PACKAGE_VERSION
    );
    
    static const char *developers[] = {"vani-tty1", NULL};
    static const char *designers[] = {"vani-tty1", NULL};
    
    
    adw_about_dialog_set_developers(ADW_ABOUT_DIALOG(dialog), developers);
    adw_about_dialog_set_designers(ADW_ABOUT_DIALOG(dialog), designers);
    adw_about_dialog_set_version(ADW_ABOUT_DIALOG(dialog), PACKAGE_VERSION);
    adw_about_dialog_set_debug_info(ADW_ABOUT_DIALOG(dialog), debug_text);
    adw_about_dialog_set_debug_info_filename(ADW_ABOUT_DIALOG(dialog), "meme-debug.txt");
    
    adw_dialog_present(dialog, GTK_WIDGET(parent));
}