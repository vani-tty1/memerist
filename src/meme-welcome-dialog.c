#include <glib/gi18n.h>
#include <adwaita.h>
#include "meme-welcome-dialog.h"

typedef struct {
    const char *icon_name;
    const char *image_resource;
    int         icon_size;
    const char *title;
    const char *subtitle;
} WelcomePage;

static const WelcomePage welcome_pages[] = {
    {
        "io.github.vani_tty1.memerist", NULL, 160,
        N_("Welcome to Memerist"),
        N_("Create and edit memes quickly, right on your desktop or your phone."),
    },
    {
        NULL, "/io/github/vani_tty1/memerist/screenshot/welcome_ss.png", 120,
        N_("Start from a Template"),
        N_("Pick one of the built-in templates, or import your own image to get going."),
    },
    {
        NULL, "/io/github/vani_tty1/memerist/screenshot/filters.png", 120,
        N_("Add Text & Filters"),
        N_("Drop in captions, crop and rotate, and apply filters like deep-fry or black & white."),
    },
    {
        NULL, "/io/github/vani_tty1/memerist/screenshot/export.png", 120,
        N_("Export & Share"),
        N_("Export your finished meme as an image, or copy it straight to the clipboard."),
    },
};

typedef struct {
    AdwDialog   *dialog;
    AdwCarousel *carousel;
    GtkButton   *next_button;
    GtkButton   *previous_button;
    GSettings   *settings;
} WelcomeDialogData;

static void
welcome_dialog_data_free (gpointer p) {
    WelcomeDialogData *data = p;

    g_clear_object (&data->settings);
    g_free (data);
}

static GtkWidget *
build_welcome_page (const WelcomePage *page) {
    GtkWidget *box;
    GtkWidget *visual;
    GtkWidget *title;
    GtkWidget *subtitle;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (box, 24);
    gtk_widget_set_margin_bottom (box, 24);
    gtk_widget_set_margin_start (box, 32);
    gtk_widget_set_margin_end (box, 32);

    if (page->image_resource) {
        visual = gtk_picture_new_for_resource (page->image_resource);
        gtk_picture_set_content_fit (GTK_PICTURE (visual), GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request (visual, -1, 260);
    } else {
        visual = gtk_image_new_from_icon_name (page->icon_name);
        gtk_image_set_pixel_size (GTK_IMAGE (visual), page->icon_size);
        gtk_widget_set_halign (visual, GTK_ALIGN_CENTER);
    }
    gtk_box_append (GTK_BOX (box), visual);

    title = gtk_label_new (_(page->title));
    gtk_widget_add_css_class (title, "title-1");
    gtk_label_set_wrap (GTK_LABEL (title), TRUE);
    gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_CENTER);
    gtk_box_append (GTK_BOX (box), title);

    subtitle = gtk_label_new (_(page->subtitle));
    gtk_widget_add_css_class (subtitle, "dim-label");
    gtk_widget_add_css_class (subtitle, "body");
    gtk_label_set_wrap (GTK_LABEL (subtitle), TRUE);
    gtk_label_set_justify (GTK_LABEL (subtitle), GTK_JUSTIFY_CENTER);
    gtk_box_append (GTK_BOX (box), subtitle);

    return box;
}

static void
finish_welcome_dialog (WelcomeDialogData *data) {
    g_settings_set_boolean (data->settings, "first-run-complete", TRUE);
    adw_dialog_close (data->dialog);
}

static void
on_welcome_next_clicked (GtkButton *button, WelcomeDialogData *data) {
    guint n_pages = adw_carousel_get_n_pages (data->carousel);
    guint current = (guint) (adw_carousel_get_position (data->carousel) + 0.5);

    if (current + 1 < n_pages) {
        GtkWidget *next_page = adw_carousel_get_nth_page (data->carousel, current + 1);
        adw_carousel_scroll_to (data->carousel, next_page, TRUE);
    } else {
        finish_welcome_dialog (data);
    }
}

static void
on_welcome_previous_clicked (GtkButton *button, WelcomeDialogData *data) {
    guint current = (guint) (adw_carousel_get_position (data->carousel) + 0.5);

    if (current > 0) {
        GtkWidget *prev_page = adw_carousel_get_nth_page (data->carousel, current - 1);
        adw_carousel_scroll_to (data->carousel, prev_page, TRUE);
    }
}

static void
on_welcome_skip_clicked (GtkButton *button, WelcomeDialogData *data) {
    finish_welcome_dialog (data);
}

static void
on_welcome_page_changed (AdwCarousel *carousel, guint index, WelcomeDialogData *data) {
    guint n_pages = adw_carousel_get_n_pages (carousel);

    gtk_button_set_label (data->next_button,
                           (index + 1 >= n_pages) ? _("Get Started") : _("Next"));
    gtk_widget_set_visible (GTK_WIDGET (data->previous_button), index > 0);
}

void
meme_show_welcome_dialog (GtkWindow *parent) {
    GtkBuilder *builder;
    AdwDialog *dialog;
    AdwCarousel *carousel;
    GtkButton *next_button;
    GtkButton *previous_button;
    GtkButton *skip_button;
    WelcomeDialogData *data;
    guint i;

    builder = gtk_builder_new_from_resource ("/io/github/vani_tty1/memerist/welcome-dialog.ui");
    dialog = ADW_DIALOG (gtk_builder_get_object (builder, "welcome_dialog"));
    carousel = ADW_CAROUSEL (gtk_builder_get_object (builder, "welcome_carousel"));
    next_button = GTK_BUTTON (gtk_builder_get_object (builder, "next_button"));
    previous_button = GTK_BUTTON (gtk_builder_get_object (builder, "previous_button"));
    skip_button = GTK_BUTTON (gtk_builder_get_object (builder, "skip_button"));

    for (i = 0; i < G_N_ELEMENTS (welcome_pages); i++)
        adw_carousel_append (carousel, build_welcome_page (&welcome_pages[i]));

    data = g_new0 (WelcomeDialogData, 1);
    data->dialog = dialog;
    data->carousel = carousel;
    data->next_button = next_button;
    data->previous_button = previous_button;
    data->settings = g_settings_new ("io.github.vani_tty1.memerist");
    g_object_set_data_full (G_OBJECT (dialog), "welcome-data", data, welcome_dialog_data_free);

    g_signal_connect (next_button, "clicked", G_CALLBACK (on_welcome_next_clicked), data);
    g_signal_connect (previous_button, "clicked", G_CALLBACK (on_welcome_previous_clicked), data);
    g_signal_connect (skip_button, "clicked", G_CALLBACK (on_welcome_skip_clicked), data);
    g_signal_connect (carousel, "page-changed", G_CALLBACK (on_welcome_page_changed), data);

    adw_dialog_present (dialog, GTK_WIDGET (parent));
    g_object_unref (builder);
}

void
meme_maybe_show_welcome_dialog (GtkWindow *parent) {
    g_autoptr (GSettings) settings = g_settings_new ("io.github.vani_tty1.memerist");

    if (g_settings_get_boolean (settings, "first-run-complete"))
        return;

    meme_show_welcome_dialog (parent);
}
