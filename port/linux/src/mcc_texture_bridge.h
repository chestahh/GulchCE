/* MCC immutable textures have identities independent of guest physical memory. */
#ifndef MCC_TEXTURE_BRIDGE_H
#define MCC_TEXTURE_BRIDGE_H

void *mcc_texture_bridge_find(void const *bitmap);
void mcc_texture_bridge_meter(void const *bitmap);
void *mcc_texture_bridge_register(void const *bitmap, unsigned long format,
    unsigned long size, void const *pixels, unsigned long bytes);
void *mcc_texture_bridge_register_linear(void const *bitmap, unsigned long format,
    unsigned long width, unsigned long height, unsigned long pitch, void const *pixels, unsigned long bytes);
void mcc_texture_bridge_dispose(void);
/* Returns estimated GPU bytes charged, or zero when skipped/unsuccessful. */
unsigned long mcc_texture_bridge_prepare(void const *bitmap, unsigned long budget);

#endif

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
