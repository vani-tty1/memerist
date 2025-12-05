/* myapp-window.c */

#include "config.h"
#include "myapp-window.h"
#include <cairo.h>
#include <math.h>

typedef enum {
    DRAG_TYPE_NONE,
    DRAG_TYPE_TOP_TEXT,
    DRAG_TYPE_BOTTOM_TEXT,
    DRAG_TYPE_IMAGE_MOVE,
    DRAG_TYPE_IMAGE_RESIZE
} DragType;

typedef struct {
    double x;
    double y;
    double width;
    double height;
    double scale;
} ImageLayer;

struct _MyappWindow
{
    AdwApplicationWindow parent_instance;

    GtkImage       *meme_preview;
    AdwEntryRow    *top_text_entry;
    AdwEntryRow    *bottom_text_entry;
    GtkButton      *export_button;
    GtkButton      *load_image_button;
    GtkButton      *clear_button;
    GtkButton      *add_image_button;
    GtkFlowBox     *template_gallery;

    GdkPixbuf      *template_image;
    GdkPixbuf      *user_image;
    GdkPixbuf      *final_meme;

    ImageLayer      image_layer;

    double          top_text_y;
    double          top_text_x;
    double          bottom_text_y;
    double          bottom_text_x;

    DragType        drag_type;
    GtkGestureDrag *drag_gesture;
    double          drag_start_x;
    double          drag_start_y;
};

G_DEFINE_FINAL_TYPE (MyappWindow, myapp_window, ADW_TYPE_APPLICATION_WINDOW)

static void on_text_changed (MyappWindow *self);
static void on_load_image_clicked (MyappWindow *self);
static void on_clear_clicked (MyappWindow *self);
static void on_add_image_clicked (MyappWindow *self);
static void on_export_clicked (MyappWindow *self);
static void on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self);
static void render_meme (MyappWindow *self);
static void draw_text_with_outline (cairo_t *cr, const char *text, double x, double y, double max_width);
static void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self);
static void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
static void on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
static void populate_template_gallery (MyappWindow *self);

static void
myapp_window_finalize (GObject *object)
{
    MyappWindow *self = MYAPP_WINDOW (object);

    g_clear_object (&self->template_image);
    g_clear_object (&self->user_image);
    g_clear_object (&self->final_meme);
    g_clear_object (&self->drag_gesture);

    G_OBJECT_CLASS (myapp_window_parent_class)->finalize (object);
}

static void
myapp_window_class_init (MyappWindowClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = myapp_window_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Memerist/myapp-window.ui");

    gtk_widget_class_bind_template_child (widget_class, MyappWindow, meme_preview);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, top_text_entry);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, bottom_text_entry);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, export_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, load_image_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, clear_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, add_image_button);
    gtk_widget_class_bind_template_child (widget_class, MyappWindow, template_gallery);
}

static void
myapp_window_init (MyappWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->top_text_y = 0.1;
    self->top_text_x = 0.5;
    self->bottom_text_y = 0.9;
    self->bottom_text_x = 0.5;
    self->drag_type = DRAG_TYPE_NONE;

    self->image_layer.x = 0.5;
    self->image_layer.y = 0.5;
    self->image_layer.scale = 1.0;
    self->image_layer.width = 0;
    self->image_layer.height = 0;

    g_signal_connect_swapped (self->top_text_entry, "changed",
                              G_CALLBACK (on_text_changed), self);
    g_signal_connect_swapped (self->bottom_text_entry, "changed",
                              G_CALLBACK (on_text_changed), self);
    g_signal_connect_swapped (self->load_image_button, "clicked",
                              G_CALLBACK (on_load_image_clicked), self);
    g_signal_connect_swapped (self->clear_button, "clicked",
                              G_CALLBACK (on_clear_clicked), self);
    g_signal_connect_swapped (self->add_image_button, "clicked",
                              G_CALLBACK (on_add_image_clicked), self);
    g_signal_connect_swapped (self->export_button, "clicked",
                              G_CALLBACK (on_export_clicked), self);
    g_signal_connect (self->template_gallery, "child-activated",
                      G_CALLBACK (on_template_selected), self);

    self->drag_gesture = GTK_GESTURE_DRAG (gtk_gesture_drag_new ());
    gtk_widget_add_controller (GTK_WIDGET (self->meme_preview),
                               GTK_EVENT_CONTROLLER (self->drag_gesture));
    g_signal_connect (self->drag_gesture, "drag-begin",
                      G_CALLBACK (on_drag_begin), self);
    g_signal_connect (self->drag_gesture, "drag-update",
                      G_CALLBACK (on_drag_update), self);
    g_signal_connect (self->drag_gesture, "drag-end",
                      G_CALLBACK (on_drag_end), self);

    populate_template_gallery (self);
}

static void
populate_template_gallery (MyappWindow *self)
{
    const char *template_dir = "/usr/share/Memerist/templates";
    GDir *dir;
    GError *error = NULL;
    const char *filename;

    dir = g_dir_open (template_dir, 0, &error);
    if (error != NULL) {
        g_warning ("Failed to open templates directory: %s", error->message);
        g_error_free (error);
        return;
    }

    while ((filename = g_dir_read_name (dir)) != NULL) {
        char *full_path;
        GtkWidget *picture;

        if (!g_str_has_suffix (filename, ".png") &&
            !g_str_has_suffix (filename, ".jpg") &&
            !g_str_has_suffix (filename, ".jpeg"))
            continue;

        full_path = g_build_filename (template_dir, filename, NULL);
        picture = gtk_picture_new_for_filename (full_path);

        gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
        gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request (picture, 120, 120);

        g_object_set_data_full (G_OBJECT (picture), "template-path",
                                full_path, g_free);

        gtk_flow_box_append (self->template_gallery, picture);
    }
    g_dir_close (dir);
}

static void
on_template_selected (GtkFlowBox *flowbox, GtkFlowBoxChild *child, MyappWindow *self)
{
    GtkWidget *image;
    const char *template_path;
    GError *error = NULL;

    if (!child)
        return;

    image = gtk_flow_box_child_get_child (child);
    template_path = g_object_get_data (G_OBJECT (image), "template-path");

    if (!template_path)
        return;

    g_clear_object (&self->template_image);
    g_clear_object (&self->user_image);

    self->template_image = gdk_pixbuf_new_from_file (template_path, &error);

    if (error != NULL) {
        g_warning ("Failed to load template: %s", error->message);
        g_error_free (error);
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);

    render_meme (self);
}

static void
on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self)
{
    int img_height;
    int img_width;
    double relative_y;
    double relative_x;
    double top_text_abs_y;
    double bottom_text_abs_y;
    double threshold;

    if (self->template_image == NULL)
        return;

    img_height = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
    img_width = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));

    if (img_height <= 0 || img_width <= 0)
        return;

    relative_y = y / img_height;
    relative_x = x / img_width;

    top_text_abs_y = self->top_text_y;
    bottom_text_abs_y = self->bottom_text_y;

    threshold = 0.15;

    if (self->user_image != NULL) {
        double scaled_w = self->image_layer.width * self->image_layer.scale;
        double scaled_h = self->image_layer.height * self->image_layer.scale;

        double layer_left = self->image_layer.x - (scaled_w / 2.0 / img_width);
        double layer_right = self->image_layer.x + (scaled_w / 2.0 / img_width);
        double layer_top = self->image_layer.y - (scaled_h / 2.0 / img_height);
        double layer_bottom = self->image_layer.y + (scaled_h / 2.0 / img_height);

        if (relative_x >= layer_left && relative_x <= layer_right &&
            relative_y >= layer_top && relative_y <= layer_bottom) {

            double corner_threshold = 0.1;
            if ((fabs(relative_x - layer_right) < corner_threshold &&
                 fabs(relative_y - layer_bottom) < corner_threshold)) {
                self->drag_type = DRAG_TYPE_IMAGE_RESIZE;
            } else {
                self->drag_type = DRAG_TYPE_IMAGE_MOVE;
            }
            self->drag_start_x = x;
            self->drag_start_y = y;
            return;
        }
    }

    if (fabs (relative_y - top_text_abs_y) < threshold) {
        self->drag_type = DRAG_TYPE_TOP_TEXT;
        self->drag_start_x = x;
        self->drag_start_y = y;
    } else if (fabs (relative_y - bottom_text_abs_y) < threshold) {
        self->drag_type = DRAG_TYPE_BOTTOM_TEXT;
        self->drag_start_x = x;
        self->drag_start_y = y;
    } else {
        self->drag_type = DRAG_TYPE_NONE;
    }
}

static void
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self)
{
    int img_height;
    int img_width;
    double new_y, new_x;
    double relative_y, relative_x;

    if (self->drag_type == DRAG_TYPE_NONE || self->template_image == NULL)
        return;

    img_height = gtk_widget_get_height (GTK_WIDGET (self->meme_preview));
    img_width = gtk_widget_get_width (GTK_WIDGET (self->meme_preview));

    if (img_height <= 0 || img_width <= 0)
        return;

    if (self->drag_type == DRAG_TYPE_IMAGE_MOVE) {
        new_x = self->drag_start_x + offset_x;
        new_y = self->drag_start_y + offset_y;

        relative_x = CLAMP (new_x / img_width, 0.0, 1.0);
        relative_y = CLAMP (new_y / img_height, 0.05, 0.95);

        self->image_layer.x = relative_x;
        self->image_layer.y = relative_y;

    } else if (self->drag_type == DRAG_TYPE_IMAGE_RESIZE) {
        double scale_factor = 1.0 + (offset_x + offset_y) / 200.0;
        self->image_layer.scale = CLAMP (scale_factor, 0.1, 3.0);

    } else {
        new_x = self->drag_start_x + offset_x;
        new_y = self->drag_start_y + offset_y;

        relative_x = CLAMP (new_x / img_width, 0.0, 1.0);
        relative_y = CLAMP (new_y / img_height, 0.05, 0.95);

        if (self->drag_type == DRAG_TYPE_TOP_TEXT) {
            self->top_text_y = relative_y;
            self->top_text_x = relative_x;
        } else if (self->drag_type == DRAG_TYPE_BOTTOM_TEXT) {
            self->bottom_text_y = relative_y;
            self->bottom_text_x = relative_x;
        }
    }

    render_meme (self);
}

static void
on_drag_end (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self)
{
    self->drag_type = DRAG_TYPE_NONE;
}

static void
on_text_changed (MyappWindow *self)
{
    if (self->template_image == NULL)
        return;
    render_meme (self);
}

static void
on_load_image_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    MyappWindow *self = MYAPP_WINDOW (user_data);
    GFile *file;
    GError *error = NULL;
    char *path;

    file = gtk_file_dialog_open_finish (dialog, result, &error);

    if (error != NULL) {
        if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            g_warning ("Failed to open file: %s", error->message);
        g_error_free (error);
        return;
    }

    path = g_file_get_path (file);

    g_clear_object (&self->template_image);
    self->template_image = gdk_pixbuf_new_from_file (path, &error);

    if (error != NULL) {
        g_warning ("Failed to load image: %s", error->message);
        g_error_free (error);
        g_free (path);
        g_object_unref (file);
        return;
    }

    self->top_text_y = 0.1;
    self->top_text_x = 0.5;
    self->bottom_text_y = 0.9;
    self->bottom_text_x = 0.5;

    gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), TRUE);

    render_meme (self);

    g_free (path);
    g_object_unref (file);
}

static void
on_load_image_clicked (MyappWindow *self)
{
    GtkFileDialog *dialog;
    GtkFileFilter *filter;
    GListStore *filters;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Choose an Image");

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "Images");
    gtk_file_filter_add_mime_type (filter, "image/png");
    gtk_file_filter_add_mime_type (filter, "image/jpeg");
    gtk_file_filter_add_mime_type (filter, "image/gif");
    gtk_file_filter_add_mime_type (filter, "image/webp");

    filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_load_image_response, self);

    g_object_unref (filters);
    g_object_unref (filter);
}

static void
on_add_image_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    MyappWindow *self = MYAPP_WINDOW (user_data);
    GFile *file;
    GError *error = NULL;
    char *path;

    file = gtk_file_dialog_open_finish (dialog, result, &error);

    if (error != NULL) {
        if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            g_warning ("Failed to open file: %s", error->message);
        g_error_free (error);
        return;
    }

    path = g_file_get_path (file);

    g_clear_object (&self->user_image);
    self->user_image = gdk_pixbuf_new_from_file (path, &error);

    if (error != NULL) {
        g_warning ("Failed to load image: %s", error->message);
        g_error_free (error);
        g_free (path);
        g_object_unref (file);
        return;
    }

    self->image_layer.width = gdk_pixbuf_get_width (self->user_image);
    self->image_layer.height = gdk_pixbuf_get_height (self->user_image);
    self->image_layer.x = 0.5;
    self->image_layer.y = 0.5;
    self->image_layer.scale = 1.0;

    render_meme (self);

    g_free (path);
    g_object_unref (file);
}

static void
on_add_image_clicked (MyappWindow *self)
{
    GtkFileDialog *dialog;
    GtkFileFilter *filter;
    GListStore *filters;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Choose an Image to Add");

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, "Images");
    gtk_file_filter_add_mime_type (filter, "image/png");
    gtk_file_filter_add_mime_type (filter, "image/jpeg");
    gtk_file_filter_add_mime_type (filter, "image/gif");
    gtk_file_filter_add_mime_type (filter, "image/webp");

    filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

    gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, on_add_image_response, self);

    g_object_unref (filters);
    g_object_unref (filter);
}

static void
on_clear_clicked (MyappWindow *self)
{
    gtk_editable_set_text (GTK_EDITABLE (self->top_text_entry), "");
    gtk_editable_set_text (GTK_EDITABLE (self->bottom_text_entry), "");

    g_clear_object (&self->template_image);
    g_clear_object (&self->user_image);
    g_clear_object (&self->final_meme);

    gtk_image_clear (self->meme_preview);
    gtk_image_set_from_icon_name (self->meme_preview, "image-x-generic-symbolic");

    gtk_widget_set_sensitive (GTK_WIDGET (self->export_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->clear_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->add_image_button), FALSE);

    self->top_text_y = 0.1;
    self->top_text_x = 0.5;
    self->bottom_text_y = 0.9;
    self->bottom_text_x = 0.5;

    gtk_flow_box_unselect_all (self->template_gallery);
}

static void
draw_text_with_outline (cairo_t *cr, const char *text, double x, double y, double max_width)
{
    cairo_text_extents_t extents;
    double font_size;

    cairo_select_font_face (cr, "Impact", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    font_size = 60.0;
    cairo_set_font_size (cr, font_size);
    cairo_text_extents (cr, text, &extents);

    if (extents.width > max_width * 0.9) {
        font_size = (max_width * 0.9) / extents.width * font_size;
        cairo_set_font_size (cr, font_size);
        cairo_text_extents (cr, text, &extents);
    }

    x = x - extents.width / 2 - extents.x_bearing;
    y = y - extents.height / 2 - extents.y_bearing;

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, 6.0);
    cairo_move_to (cr, x, y);
    cairo_text_path (cr, text);
    cairo_stroke (cr);

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_move_to (cr, x, y);
    cairo_show_text (cr, text);
}

static void
render_meme (MyappWindow *self)
{
    const char *top_text;
    const char *bottom_text;
    int width;
    int height;
    cairo_surface_t *surface;
    cairo_t *cr;
    GdkTexture *texture;
    GdkPixbuf *rgba_pixbuf;

    if (self->template_image == NULL)
        return;

    top_text = gtk_editable_get_text (GTK_EDITABLE (self->top_text_entry));
    bottom_text = gtk_editable_get_text (GTK_EDITABLE (self->bottom_text_entry));

    width = gdk_pixbuf_get_width (self->template_image);
    height = gdk_pixbuf_get_height (self->template_image);

    if (!gdk_pixbuf_get_has_alpha (self->template_image)) {
        rgba_pixbuf = gdk_pixbuf_add_alpha (self->template_image, FALSE, 0, 0, 0);
    } else {
        rgba_pixbuf = gdk_pixbuf_copy (self->template_image);
    }

    surface = cairo_image_surface_create_for_data (
        gdk_pixbuf_get_pixels (rgba_pixbuf),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        gdk_pixbuf_get_rowstride (rgba_pixbuf)
    );

    cr = cairo_create (surface);

    if (self->user_image != NULL) {
        double scaled_width = self->image_layer.width * self->image_layer.scale;
        double scaled_height = self->image_layer.height * self->image_layer.scale;
        double draw_x = self->image_layer.x * width - scaled_width / 2.0;
        double draw_y = self->image_layer.y * height - scaled_height / 2.0;

        cairo_save (cr);
        cairo_translate (cr, draw_x, draw_y);
        cairo_scale (cr, self->image_layer.scale, self->image_layer.scale);

        gdk_cairo_set_source_pixbuf (cr, self->user_image, 0, 0);
        cairo_paint (cr);

        cairo_set_source_rgba (cr, 0, 0, 1, 0.3);
        cairo_set_line_width (cr, 2.0);
        cairo_rectangle (cr, 0, 0, self->image_layer.width, self->image_layer.height);
        cairo_stroke (cr);

        cairo_arc (cr, self->image_layer.width, self->image_layer.height, 8, 0, 2 * M_PI);
        cairo_fill (cr);

        cairo_restore (cr);
    }

    if (top_text && strlen (top_text) > 0) {
        char *upper_text = g_utf8_strup (top_text, -1);
        draw_text_with_outline (cr, upper_text, width * self->top_text_x, height * self->top_text_y, width);
        g_free (upper_text);
    }

    if (bottom_text && strlen (bottom_text) > 0) {
        char *upper_text = g_utf8_strup (bottom_text, -1);
        draw_text_with_outline (cr, upper_text, width * self->bottom_text_x, height * self->bottom_text_y, width);
        g_free (upper_text);
    }

    cairo_surface_flush (surface);
    cairo_destroy (cr);

    g_clear_object (&self->final_meme);
    self->final_meme = rgba_pixbuf;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    texture = gdk_texture_new_for_pixbuf (self->final_meme);
    G_GNUC_END_IGNORE_DEPRECATIONS

    gtk_image_set_from_paintable (self->meme_preview, GDK_PAINTABLE (texture));
    g_object_unref (texture);

    cairo_surface_destroy (surface);
}

static void
on_export_response (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    MyappWindow *self = MYAPP_WINDOW (user_data);
    GFile *file;
    GError *error = NULL;
    char *path;
    gboolean success;

    file = gtk_file_dialog_save_finish (dialog, result, &error);

    if (error != NULL) {
        if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            g_warning ("Failed to save file: %s", error->message);
        g_error_free (error);
        return;
    }

    path = g_file_get_path (file);
    success = gdk_pixbuf_save (self->final_meme, path, "png", &error, NULL);

    if (!success) {
        g_warning ("Failed to save meme: %s", error->message);
        g_error_free (error);
    } else {
        g_print ("Meme saved to: %s\n", path);
    }

    g_free (path);
    g_object_unref (file);
}

static void
on_export_clicked (MyappWindow *self)
{
    GtkFileDialog *dialog;

    if (self->final_meme == NULL)
        return;

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Save Meme");
    gtk_file_dialog_set_initial_name (dialog, "meme.png");

    gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, on_export_response, self);
}
