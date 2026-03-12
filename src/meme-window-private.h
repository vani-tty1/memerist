#pragma once
#include "meme-window.h"
#include <adwaita.h>
#include "meme-core.h"
#include "meme-renderer.h"

struct _MyappWindow {
    AdwApplicationWindow parent_instance;
    AdwPreferencesGroup *layer_group;
    AdwPreferencesGroup *templates_group;
    AdwPreferencesGroup *transform_group;
    AdwOverlaySplitView *split_view;
    AdwToastOverlay *copy_clip_feedback;
    GtkStack *content_stack;
    GtkPicture *meme_preview;
    GtkImage *add_text_button;
    AdwActionRow *font_choose_row;
    GtkFontDialogButton *font_choose_btn;
    AdwEntryRow *layer_text_entry;
    AdwActionRow *layer_font_size_row;
    GtkSpinButton *layer_font_size;
    GtkMenuButton *main_menu_button;    
    GtkButton *export_button, *copy_clipboard_button, *zoom_in, *zoom_out;
    GtkButton *load_image_button, *pill_btn_open_image, *clear_button, *add_image_button;
    GtkButton *import_template_button, *delete_template_button;
    GtkToggleButton *deep_fry_button, *cinematic_button, *crop_mode_button;
    GtkFlowBox *template_gallery;
    GtkMenuButton *global_filters_button;
    GtkScale *layer_opacity_scale, *layer_rotation_scale;
    AdwComboRow *blend_mode_row;
    GtkButton *delete_layer_button;
    GtkButton *rotate_left_button, *rotate_right_button, *flip_h_button, *flip_v_button;
    GtkButton *crop_square_button, *crop_43_button, *crop_169_button;
    GtkButton *save_project_button, *load_project_button;   
    GdkPixbuf *template_image, *final_meme;
    GList *layers, *undo_stack, *redo_stack;
    ImageLayer *selected_layer; 
    DragType drag_type;
    
    GtkGestureDrag *drag_gesture;
    ResizeHandle active_crop_handle;    
    double drag_start_x, drag_start_y;
    double drag_obj_start_x, drag_obj_start_y, drag_obj_start_scale, drag_obj_start_h;
    double zoom_level;
    double crop_x, crop_y, crop_w, crop_h;
};

void sync_ui_with_layer(MyappWindow *self);
void render_meme(MyappWindow *self);
void on_clear_clicked(MyappWindow *self);
void apply_zoom(MyappWindow *self);
void update_template_image(MyappWindow *self, GdkPixbuf *new_pixbuf);