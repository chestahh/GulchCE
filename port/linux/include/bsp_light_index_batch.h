/* Immutable triangle indices belonging to one BSP light pass on Windows. */
#ifndef BSP_LIGHT_INDEX_BATCH_H
#define BSP_LIGHT_INDEX_BATCH_H

void bsp_light_index_batch_begin(long triangle_buffer, long light);
void bsp_light_index_batch_set(const void *base, unsigned long size);

#endif
