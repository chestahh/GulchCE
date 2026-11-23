/* Drive world receiver marking and ordinary draws through one GL state cache. */
#include "harness.h"
#include <string.h>

typedef unsigned int DWORD, GLenum, GLuint;
typedef int GLint, BOOL;
enum { GL_ZERO, GL_NEVER, GL_ALWAYS, GL_KEEP, GL_REPLACE, GL_STENCIL_TEST };
enum { D3DRS_STENCILENABLE, D3DRS_STENCILFUNC, D3DRS_STENCILREF, D3DRS_STENCILMASK,
	D3DRS_STENCILWRITEMASK, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS };
static DWORD D3D__RenderState[8] = {1, GL_NEVER, 3, 255, 3, GL_KEEP, GL_REPLACE, GL_KEEP};
static struct {
	unsigned char stencil_test;
	GLenum stencil_function, stencil_operations[3];
	GLint stencil_reference;
	GLuint stencil_value_mask, stencil_write_mask;
} gl_state;
static int marking, enabled, changes;
static GLint reference;
static GLenum function, operations[3];
static GLuint compare_mask, write_mask;
#define modern_environment_marking() marking
static void glEnable(GLenum cap) { enabled = 1; changes++; }
static void glDisable(GLenum cap) { enabled = 0; changes++; }
static void glStencilFunc(GLenum fn, GLint ref, GLuint mask)
{
	function = fn; reference = ref; compare_mask = mask; changes++;
}
static void glStencilMask(GLuint mask) { write_mask = mask; changes++; }
static void glStencilOp(GLenum fail, GLenum zfail, GLenum pass)
{
	operations[0] = fail; operations[1] = zfail; operations[2] = pass; changes++;
}
#include "under_test.inc"

int main(void)
{
	int first;
	memset(&gl_state, 0xff, sizeof(gl_state));
	marking = 1;
	apply_stencil_state(1);
#ifndef HALO_GLES
	CHECK(enabled && function == GL_NEVER && reference == 131 && compare_mask == 127 && write_mask == 128,
		"world draw compares stock bits and writes only the receiver bit");
	CHECK(operations[0] == GL_KEEP && operations[1] == GL_KEEP && operations[2] == GL_REPLACE,
		"passing world fragments mark coverage");
	first = changes;
	apply_stencil_state(1);
	CHECK(changes == first, "consecutive world draws reuse state");
	D3D__RenderState[D3DRS_STENCILENABLE] = 0;
	apply_stencil_state(1);
	CHECK(enabled && function == GL_ALWAYS && compare_mask == 0 && write_mask == 128, "world-only coverage");
	D3D__RenderState[D3DRS_STENCILENABLE] = 1;
#endif
	marking = 0;
	apply_stencil_state(1);
	CHECK(enabled && function == GL_NEVER && reference == 3 && compare_mask == 255 && write_mask == 3,
		"ordinary drawing restores the game's stencil values");
	CHECK(operations[0] == GL_KEEP && operations[1] == GL_REPLACE && operations[2] == GL_KEEP,
		"ordinary drawing restores the game's operations");
	first = changes;
	memset(&gl_state, 0xff, sizeof(gl_state));
	apply_stencil_state(1);
	CHECK(changes == first + 4, "an external pass invalidates all stencil state");
	apply_stencil_state(0);
	CHECK(!enabled, "ordinary drawing without depth disables the stencil test");
	return 0;
}
