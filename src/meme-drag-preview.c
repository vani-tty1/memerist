/* meme-drag-preview.c
 *
 * Copyright 2025 Giovanni
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "meme-drag-preview.h"
#include <math.h>
struct _MemeDragPreview
{
  GObject parent_instance;

  GdkTexture *below;
  GdkTexture *above;
  GdkTexture *layer;
  MemeDragLayerState state;
};

static void meme_drag_preview_snapshot (GdkPaintable *paintable,
                                        GdkSnapshot *gdk_snapshot,
                                        double width,
                                        double height);

static GskBlendMode
gsk_blend_mode_for (BlendMode mode)
{
  switch (mode)
    {
    case BLEND_MULTIPLY:
      return GSK_BLEND_MODE_MULTIPLY;
    case BLEND_SCREEN:
      return GSK_BLEND_MODE_SCREEN;
    case BLEND_OVERLAY:
      return GSK_BLEND_MODE_OVERLAY;
    case BLEND_NORMAL:
    default:
      return GSK_BLEND_MODE_DEFAULT;
    }
}

static void
snapshot_layer (MemeDragPreview *self, GtkSnapshot *snapshot, float img_w, float img_h)
{
  const MemeDragLayerState *s = &self->state;
  graphene_point_t pos = GRAPHENE_POINT_INIT ((float) (s->x * img_w),
                                              (float) (s->y * img_h));
  graphene_rect_t rect = GRAPHENE_RECT_INIT ((float) (-s->width / 2.0),
                                             (float) (-s->height / 2.0),
                                             (float) gdk_texture_get_width (self->layer),
                                             (float) gdk_texture_get_height (self->layer));

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &pos);
  gtk_snapshot_rotate (snapshot, (float) (s->rotation * 180.0 / G_PI));
  gtk_snapshot_scale (snapshot, (float) s->scale, (float) s->scale);

  if (s->opacity < 1.0)
    gtk_snapshot_push_opacity (snapshot, CLAMP (s->opacity, 0.0, 1.0));

  gtk_snapshot_append_scaled_texture (snapshot, self->layer,
                                      GSK_SCALING_FILTER_LINEAR, &rect);

  if (s->opacity < 1.0)
    gtk_snapshot_pop (snapshot);

  gtk_snapshot_restore (snapshot);
}

static void
meme_drag_preview_snapshot (GdkPaintable *paintable,
                            GdkSnapshot *gdk_snapshot,
                            double width,
                            double height)
{
  MemeDragPreview *self = MEME_DRAG_PREVIEW (paintable);
  GtkSnapshot *snapshot = GTK_SNAPSHOT (gdk_snapshot);
  float img_w = gdk_texture_get_width (self->below);
  float img_h = gdk_texture_get_height (self->below);
  graphene_rect_t full = GRAPHENE_RECT_INIT (0, 0, img_w, img_h);
  gboolean blend = self->state.blend_mode != BLEND_NORMAL;

  if (width <= 0 || height <= 0)
    return;

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, (float) (width / img_w), (float) (height / img_h));

  gtk_snapshot_push_clip (snapshot, &full);

  if (blend)
    gtk_snapshot_push_blend (snapshot, gsk_blend_mode_for (self->state.blend_mode));

  gtk_snapshot_append_texture (snapshot, self->below, &full);

  if (blend)
    gtk_snapshot_pop (snapshot);
  snapshot_layer (self, snapshot, img_w, img_h);

  if (blend)
    gtk_snapshot_pop (snapshot);

  if (self->above)
    gtk_snapshot_append_texture (snapshot, self->above, &full);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

static GdkPaintable *
meme_drag_preview_get_current_image (GdkPaintable *paintable)
{
  MemeDragPreview *self = MEME_DRAG_PREVIEW (paintable);
  int w = gdk_texture_get_width (self->below);
  int h = gdk_texture_get_height (self->below);
  GtkSnapshot *snapshot = gtk_snapshot_new ();
  graphene_size_t size = GRAPHENE_SIZE_INIT (w, h);

  meme_drag_preview_snapshot (paintable, (GdkSnapshot *) snapshot, w, h);
  return gtk_snapshot_free_to_paintable (snapshot, &size);
}

static GdkPaintableFlags
meme_drag_preview_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static int
meme_drag_preview_get_intrinsic_width (GdkPaintable *paintable)
{
  return gdk_texture_get_width (MEME_DRAG_PREVIEW (paintable)->below);
}

static int
meme_drag_preview_get_intrinsic_height (GdkPaintable *paintable)
{
  return gdk_texture_get_height (MEME_DRAG_PREVIEW (paintable)->below);
}

static void
meme_drag_preview_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = meme_drag_preview_snapshot;
  iface->get_current_image = meme_drag_preview_get_current_image;
  iface->get_flags = meme_drag_preview_get_flags;
  iface->get_intrinsic_width = meme_drag_preview_get_intrinsic_width;
  iface->get_intrinsic_height = meme_drag_preview_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (MemeDragPreview, meme_drag_preview, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, meme_drag_preview_paintable_init))

static void
meme_drag_preview_dispose (GObject *object)
{
  MemeDragPreview *self = MEME_DRAG_PREVIEW (object);

  g_clear_object (&self->below);
  g_clear_object (&self->above);
  g_clear_object (&self->layer);

  G_OBJECT_CLASS (meme_drag_preview_parent_class)->dispose (object);
}

static void
meme_drag_preview_class_init (MemeDragPreviewClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = meme_drag_preview_dispose;
}

static void
meme_drag_preview_init (MemeDragPreview *self)
{
}

MemeDragPreview *
meme_drag_preview_new (GdkTexture *below, GdkTexture *above, GdkTexture *layer)
{
  MemeDragPreview *self;

  g_return_val_if_fail (GDK_IS_TEXTURE (below), NULL);
  g_return_val_if_fail (GDK_IS_TEXTURE (layer), NULL);

  self = g_object_new (MEME_TYPE_DRAG_PREVIEW, NULL);
  self->below = g_object_ref (below);
  self->above = above ? g_object_ref (above) : NULL;
  self->layer = g_object_ref (layer);
  self->state.scale = 1.0;
  self->state.opacity = 1.0;
  return self;
}

void
meme_drag_preview_state_from_layer (MemeDragLayerState *state, const ImageLayer *layer)
{
  state->x = layer->x;
  state->y = layer->y;
  state->width = layer->width;
  state->height = layer->height;
  state->scale = layer->scale;
  state->rotation = layer->rotation;
  state->opacity = layer->opacity;
  state->blend_mode = layer->blend_mode;
}

static gboolean
state_equal (const MemeDragLayerState *a, const MemeDragLayerState *b)
{
  return a->x == b->x && a->y == b->y &&
         a->width == b->width && a->height == b->height &&
         a->scale == b->scale && a->rotation == b->rotation &&
         a->opacity == b->opacity && a->blend_mode == b->blend_mode;
}

void
meme_drag_preview_set_state (MemeDragPreview *self, const MemeDragLayerState *state)
{
  g_return_if_fail (MEME_IS_DRAG_PREVIEW (self));

  if (state_equal (&self->state, state))
    return;

  self->state = *state;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}
