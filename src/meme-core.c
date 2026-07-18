#include "meme-core.h"
#include <MagickWand/MagickWand.h>

ImageLayer * meme_layer_copy (const ImageLayer *src) {
    ImageLayer *dst = g_new0 (ImageLayer, 1);
    *dst = *src;
    if (src->pixbuf) g_object_ref (src->pixbuf);
    if (src->text) dst->text = g_strdup (src->text);
    if (src->font_family) dst->font_family = g_strdup(src->font_family);
    return dst;
}

void meme_layer_free (gpointer data) {
  ImageLayer *layer = (ImageLayer *)data;
  if (layer) {
	g_clear_object(&layer->pixbuf);
    g_clear_pointer(&layer->text, g_free);
    g_clear_pointer(&layer->font_family, g_free);
    g_free(layer);
  }
}

GList * meme_layer_list_copy (GList *src) {
    GList *dst = NULL;
    GList *l;
    for (l = src; l != NULL; l = l->next) {
        dst = g_list_append (dst, meme_layer_copy ((ImageLayer *)l->data));
    }
    return dst;
}

void meme_layer_list_free (GList *list) {
    g_list_free_full (list, (GDestroyNotify)meme_layer_free);
}

static MagickWand *pixbuf_to_wand(GdkPixbuf *pb) {
    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    int channels = gdk_pixbuf_get_n_channels(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    const guchar *pixels = gdk_pixbuf_get_pixels(pb);

    MagickWand *wand = NewMagickWand();
    PixelWand *bg = NewPixelWand();
    MagickNewImage(wand, w, h, bg);
    DestroyPixelWand(bg);

    if (rowstride == w * channels) {
        MagickImportImagePixels(wand, 0, 0, w, h, channels == 4 ? "RGBA" : "RGB", CharPixel, pixels);
    } else {
        for (int y = 0; y < h; y++) {
            MagickImportImagePixels(wand, 0, y, w, 1, channels == 4 ? "RGBA" : "RGB", CharPixel, pixels + y * rowstride);
        }
    }
    return wand;
}

static GdkPixbuf *wand_to_pixbuf(MagickWand *wand) {
    int w = MagickGetImageWidth(wand);
    int h = MagickGetImageHeight(wand);
    gboolean has_alpha = MagickGetImageAlphaChannel(wand) == MagickTrue;

    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, w, h);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    guchar *pixels = gdk_pixbuf_get_pixels(pb);

    if (rowstride == w * (has_alpha ? 4 : 3)) {
        MagickExportImagePixels(wand, 0, 0, w, h, has_alpha ? "RGBA" : "RGB", CharPixel, pixels);
    } else {
        for (int y = 0; y < h; y++) {
            MagickExportImagePixels(wand, 0, y, w, 1, has_alpha ? "RGBA" : "RGB", CharPixel, pixels + y * rowstride);
        }
    }
    return pb;
}

GdkPixbuf *meme_core_apply_saturation_contrast(GdkPixbuf *src, double sat, double contrast) {
    MagickWand *wand;
    GdkPixbuf *out;

    MagickWandGenesis();
    wand = pixbuf_to_wand(src);
    MagickModulateImage(wand, 100.0, sat * 100.0, 100.0);
    if (contrast != 1.0) {
        MagickBrightnessContrastImage(wand, 0.0, (contrast - 1.0) * 50.0);
    }
    out = wand_to_pixbuf(wand);
    DestroyMagickWand(wand);
    MagickWandTerminus();
    return out;
}

GdkPixbuf *meme_core_apply_deep_fry(GdkPixbuf *src) {
    MagickWand *wand;
    int w, h;
    GdkPixbuf *out;

    MagickWandGenesis();
    wand = pixbuf_to_wand(src);
    w = MagickGetImageWidth(wand);
    h = MagickGetImageHeight(wand);

    MagickResizeImage(wand, MAX(w / 4, 1), MAX(h / 4, 1), PointFilter);
    MagickResizeImage(wand, w, h, PointFilter);

    MagickAddNoiseImage(wand, LaplacianNoise, 1.5);
    MagickModulateImage(wand, 100.0, 300.0, 100.0);
    MagickBrightnessContrastImage(wand, 0.0, 80.0);

    out = wand_to_pixbuf(wand);
    DestroyMagickWand(wand);
    MagickWandTerminus();
    return out;
}

GdkPixbuf *meme_core_apply_effects(GdkPixbuf *composite, gboolean cinematic, gboolean deep_fry) {
    GdkPixbuf *result;
    gboolean own;

    if (!cinematic && !deep_fry) {
        g_object_ref(composite);
        return composite;
    }

    result = composite;
    own = FALSE;

    if (cinematic) {
        GdkPixbuf *tmp = meme_core_apply_saturation_contrast(result, 1.15, 1.05);
        if (own) g_object_unref(result);
        result = tmp;
        own = TRUE;
    }

    if (deep_fry) {
        GdkPixbuf *tmp = meme_core_apply_deep_fry(result);
        if (own) g_object_unref(result);
        result = tmp;
        own = TRUE;
    }

    if (!own) g_object_ref(result);
    return result;
}
