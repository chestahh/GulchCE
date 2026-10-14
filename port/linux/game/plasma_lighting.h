#ifndef PLASMA_LIGHTING_H
#define PLASMA_LIGHTING_H

void plasma_lighting_prepare(long light_index, long object_index);
void plasma_lighting_frame_end(void);
void plasma_lighting_reset(void);

/* object_lights.c: validate the owner before refreshing an attachment. */
int light_port_refresh_attachment(long light_index, long object_index);

#endif
