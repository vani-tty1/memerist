#pragma once
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib.h>



//Initialise (or re-use) the shared GL context.
//Returns TRUE if GPU acceleration is available.
// Fallsback to CPU if it fails
gboolean meme_gpu_init(GdkDisplay *display);

void meme_gpu_cleanup(void);


GdkPixbuf *meme_gpu_apply_saturation_contrast(GdkPixbuf *src,
                                              double sat,
                                              double contrast);


GdkPixbuf *meme_gpu_apply_deep_fry(GdkPixbuf *src);


GdkPixbuf *meme_gpu_apply_effects(GdkPixbuf *composite,
                                  gboolean cinematic,
                                  gboolean deep_fry);



typedef struct {
    GdkPixbuf *pixbuf; 
    double x;
    double y;
    double rotation; 
    double scale;
    double opacity;
    int blend_mode;
} MemeGpuLayer;


GdkPixbuf *meme_gpu_composite_layers(GdkPixbuf *bg,
                                     MemeGpuLayer *layers,
                                     int n_layers);

// Returns TRUE if GPU acceleration was successful.
gboolean meme_gpu_is_available(void);