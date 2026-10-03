/* meme-drag-preview.h
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

#pragma once

#include <gtk/gtk.h>
#include "meme-core.h"

G_BEGIN_DECLS

#define MEME_TYPE_DRAG_PREVIEW (meme_drag_preview_get_type ())
G_DECLARE_FINAL_TYPE (MemeDragPreview, meme_drag_preview, MEME, DRAG_PREVIEW, GObject)


typedef struct {
  double x, y;
  double width, height;
  double scale;
  double rotation;
  double opacity;
  BlendMode blend_mode;
} MemeDragLayerState;

MemeDragPreview *meme_drag_preview_new (GdkTexture *below,
                                        GdkTexture *above,
                                        GdkTexture *layer);

void meme_drag_preview_state_from_layer (MemeDragLayerState *state,
                                         const ImageLayer   *layer);

void meme_drag_preview_set_state (MemeDragPreview        *self,
                                  const MemeDragLayerState *state);

G_END_DECLS
