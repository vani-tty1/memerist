#include "meme-core.h"

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
