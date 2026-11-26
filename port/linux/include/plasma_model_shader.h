/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

/* Render-only plasma selection and shader additions. No tag data is changed. */
#ifndef PLASMA_MODEL_SHADER_H
#define PLASMA_MODEL_SHADER_H
struct xgpu_text;
struct nv2a_vertex_lighting;
void plasma_model_lighting_reset(void);
void plasma_model_lighting_slot(long light_index, short slot);
unsigned int plasma_model_lighting_mask(void);
void plasma_model_emit_uniform(struct xgpu_text *text);
void plasma_model_emit_vertex(struct xgpu_text *text, const unsigned long *instruction,
	unsigned long index, const struct nv2a_vertex_lighting *lighting);
void plasma_model_emit_pixel(struct xgpu_text *text);
#endif
