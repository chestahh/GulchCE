/* A host-ABI boundary: compiler allocations never cross the DLL boundary. */
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static int initialized;

/* Called on the render thread. Output is caller-owned, bounded SPIR-V. */
EXPORT int halo_compile_spirv(
	int fragment, const char *source, unsigned *words, unsigned capacity, char *error, unsigned error_size)
{
	glslang_input_t input;
	glslang_shader_t *shader = NULL;
	glslang_program_t *program = NULL;
	size_t count;
	int result = 0;

	if (!initialized)
		initialized = glslang_initialize_process();
	if (!initialized)
		return 0;
	memset(&input, 0, sizeof(input));
	input.language = GLSLANG_SOURCE_GLSL;
	input.stage = fragment ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX;
	input.client = GLSLANG_CLIENT_VULKAN;
	input.client_version = GLSLANG_TARGET_VULKAN_1_1;
	input.target_language = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_3;
	input.code = source;
	input.default_version = 450;
	input.default_profile = GLSLANG_CORE_PROFILE;
	input.messages = GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT;
	input.resource = glslang_default_resource();
	shader = glslang_shader_create(&input);
	if (!shader)
		goto done;
	if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
	{
		snprintf(error, error_size, "%s", glslang_shader_get_info_log(shader));
		goto done;
	}
	program = glslang_program_create();
	if (!program)
		goto done;
	glslang_program_add_shader(program, shader);
	if (!glslang_program_link(program, input.messages))
	{
		snprintf(error, error_size, "%s", glslang_program_get_info_log(program));
		goto done;
	}
	glslang_program_SPIRV_generate(program, input.stage);
	count = glslang_program_SPIRV_get_size(program);
	if (!count || count > capacity)
	{
		snprintf(error, error_size, "SPIR-V output exceeds capacity or is empty");
		goto done;
	}
	glslang_program_SPIRV_get(program, words);
	result = (int)count;
done:
	if (program)
		glslang_program_delete(program);
	if (shader)
		glslang_shader_delete(shader);
	return result;
}

EXPORT void halo_compiler_shutdown(void)
{
	if (initialized)
		glslang_finalize_process();
	initialized = 0;
}
