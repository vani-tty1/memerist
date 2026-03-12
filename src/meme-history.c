/* meme-history.c
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


#include "meme-history.h"

void free_history_stack (GList **stack) {
    GList *l;
    for (l = *stack; l != NULL; l = l->next) meme_layer_list_free ((GList *)l->data);
    g_list_free (*stack);
    *stack = NULL;
}

void push_undo (MyappWindow *self) {
    free_history_stack (&self->redo_stack);
    if (g_list_length (self->undo_stack) >= 20) {
        GList *last = g_list_last (self->undo_stack);
        meme_layer_list_free ((GList *)last->data);
        self->undo_stack = g_list_delete_link (self->undo_stack, last);
    }
    self->undo_stack = g_list_prepend (self->undo_stack, meme_layer_list_copy (self->layers));
}

void myapp_window_perform_undo(MyappWindow *self) {
    if (!self->undo_stack) return;
    self->redo_stack = g_list_prepend (self->redo_stack, self->layers);
    self->layers = (GList *)self->undo_stack->data;
    self->undo_stack = g_list_delete_link (self->undo_stack, self->undo_stack);
    self->selected_layer = NULL;
    sync_ui_with_layer (self);
    render_meme (self);
}

void myapp_window_perform_redo (MyappWindow *self) {
    if (!self->redo_stack) return;
    self->undo_stack = g_list_prepend (self->undo_stack, self->layers);
    self->layers = (GList *)self->redo_stack->data;
    self->redo_stack = g_list_delete_link (self->redo_stack, self->redo_stack);
    self->selected_layer = NULL;
    sync_ui_with_layer (self);
    render_meme (self);
}