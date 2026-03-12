/* meme-canvas.h
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
#include "meme-window-private.h"

void on_mouse_move (GtkEventControllerMotion *controller, double x, double y, MyappWindow *self);
void on_drag_begin (GtkGestureDrag *gesture, double x, double y, MyappWindow *self);
void on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, MyappWindow *self);
void on_drag_end (GtkGestureDrag *g, double x, double y, MyappWindow *self);