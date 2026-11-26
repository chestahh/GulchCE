/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

/* Immutable triangle indices belonging to one BSP light pass on Windows. */
#ifndef BSP_LIGHT_INDEX_BATCH_H
#define BSP_LIGHT_INDEX_BATCH_H

void bsp_light_index_batch_begin(long triangle_buffer, long light);
void bsp_light_index_batch_set(const void *base, unsigned long size);

#endif
