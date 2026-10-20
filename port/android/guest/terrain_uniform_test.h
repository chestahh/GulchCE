/* Sample expected CPU constants against GL uniform readback after preparation.
Included after the renderer's program/device declarations; no state is changed. */
static void android_terrain_uniform_check(GLuint program, const char *name,
	const float *expected, unsigned long *checked, unsigned long *mismatched)
{
	GLint location = glGetUniformLocation(program, name);
	float actual[4] = { 0, 0, 0, 0 };
	int component;
	BOOL different = FALSE;

	if (location < 0)
		return;
	glGetUniformfv(program, location, actual);
	(*checked)++;
	for (component = 0; component < 4; component++)
	{
		if (actual[component] != expected[component] &&
			memcmp(&actual[component], &expected[component], sizeof(float)))
			different = TRUE;
	}
	if (different)
	{
		(*mismatched)++;
		if (*mismatched <= 4)
			android_texture_test_log("uniform_mismatch: program=%u name=%s expected=%g,%g,%g,%g actual=%g,%g,%g,%g",
				program, name, expected[0], expected[1], expected[2], expected[3],
				actual[0], actual[1], actual[2], actual[3]);
	}
}

static void android_terrain_uniform_capture(struct program_entry *entry)
{
	static unsigned long cursor;
	unsigned long checked = 0, mismatched = 0, index, reg;
	char name[32];

	if (!android_terrain_test_sampling())
		return;
	/* Keep the environment transform registers in each sample, rotating
	through the remaining registers so matrix/lighting coverage also grows. */
	for (index = 0; index < 16; index++)
	{
		reg = index < 8 ? XGPU_VERTEX_CONSTANT_BIAS - 84 + index :
			(cursor * 8 + index - 8) % XGPU_VERTEX_CONSTANT_COUNT;
		snprintf(name, sizeof(name), "c[%lu]", reg);
		android_terrain_uniform_check(entry->program, name, device.constants[reg], &checked, &mismatched);
	}
	cursor++;
	for (index = 0; index < 8; index++)
	{
		snprintf(name, sizeof(name), "ps_c0[%lu]", index);
		android_terrain_uniform_check(entry->program, name, draw_uniforms.ps_c0[index], &checked, &mismatched);
		snprintf(name, sizeof(name), "ps_c1[%lu]", index);
		android_terrain_uniform_check(entry->program, name, draw_uniforms.ps_c1[index], &checked, &mismatched);
	}
	for (index = 0; index < 4; index++)
	{
		snprintf(name, sizeof(name), "texture_scale[%lu]", index);
		android_terrain_uniform_check(entry->program, name, draw_uniforms.texture_scale[index], &checked, &mismatched);
		snprintf(name, sizeof(name), "bump_matrix[%lu]", index);
		android_terrain_uniform_check(entry->program, name, draw_uniforms.bump_matrix[index], &checked, &mismatched);
		snprintf(name, sizeof(name), "bump_luminance[%lu]", index);
		android_terrain_uniform_check(entry->program, name, draw_uniforms.bump_luminance[index], &checked, &mismatched);
	}
	android_terrain_uniform_check(entry->program, "ps_final_c0", draw_uniforms.ps_final_c0, &checked, &mismatched);
	android_terrain_uniform_check(entry->program, "ps_final_c1", draw_uniforms.ps_final_c1, &checked, &mismatched);
	android_terrain_uniform_check(entry->program, "fog_color", draw_uniforms.fog_color, &checked, &mismatched);
	android_texture_test_log("terrain_uniforms: program=%u checked=%lu mismatched=%lu "
		"constants_serial=%llu program_constants_serial=%llu uniforms_serial=%lu program_uniforms_serial=%lu "
		"zfunc=%lx blend=%lx/%lx",
		entry->program, checked, mismatched, constants_serial, entry->constants_serial,
		(unsigned long)draw_uniforms_serial, (unsigned long)entry->uniforms_serial,
		(unsigned long)D3D__RenderState[D3DRS_ZFUNC], (unsigned long)D3D__RenderState[D3DRS_SRCBLEND],
		(unsigned long)D3D__RenderState[D3DRS_DESTBLEND]);
	android_state_test_stage("after uniform readback");
}
