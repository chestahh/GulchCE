/* The element buffer belongs to the GL vertex array. Model a binding change
   between draws and check the actual triangle indices on both paths. */
#include "harness.h"
#include <stdlib.h>
#include <string.h>

typedef unsigned short WORD;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
enum { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, GL_BUFFER_SIZE, GL_TRIANGLES, GL_UNSIGNED_SHORT };
static struct { GLuint index_buffer, sequential_quad_buffer; } device;
static WORD cached[98304];
static const WORD *streamed;
static GLuint bound;
static unsigned expected, draws, uploads, bytes, failed, deletes;

static void glGenBuffers(int n, GLuint *name) { *name = 9; }
static void glBindBuffer(int t, GLuint name) { bound = name; }
static void state_element_array_buffer(GLuint name) { bound = name; }
static void glBufferData(int t, unsigned size, const void *data, int usage)
{
    if (size > sizeof(cached)) abort();
    bytes = failed ? 0 : size;
    memcpy(cached, data, bytes);
}
static void glGetBufferParameteriv(int target, int pname, GLint *out) { *out = bytes; }
static void glDeleteBuffers(int n, GLuint *name) { ++deletes; }
static unsigned long index_upload(const void *p, unsigned size)
{
    bound = device.index_buffer;
    streamed = p;
    ++uploads;
    return 16;
}
static void glDrawElements(int mode, GLsizei count, int type, const void *offset)
{
    static const unsigned corner[] = { 0, 1, 2, 0, 2, 3 };
    const WORD *indices = bound == 9 ? cached : streamed;
    unsigned i;
    if (mode != GL_TRIANGLES || type != GL_UNSIGNED_SHORT || count != (GLsizei)(expected / 4 * 6)) abort();
    if (bound == 9 ? offset != NULL : bound != device.index_buffer || offset != (const void *)16) abort();
    for (i = 0; i < (unsigned)count; ++i)
        if (indices[i] != (WORD)(i / 6 * 4 + corner[i % 6])) abort();
    ++draws;
}

#include "under_test.inc"

int main(void)
{
    static const unsigned sizes[] = { 3, 4, 7, 32, 65535, 65536, 65540 };
    unsigned i;
    device.index_buffer = 3;
#ifndef HALO_GLES
    failed = 1;
    CHECK(!create_sequential_quad_buffer() && deletes == 1 && bound == 3, "failed allocation restores stream");
    expected = 8;
    draw_sequential_quads(expected);
    CHECK(uploads == 1, "unavailable cache streams original indices");
    failed = uploads = draws = 0;
    device.sequential_quad_buffer = create_sequential_quad_buffer();
    CHECK(device.sequential_quad_buffer == 9 && bound == 3, "initialization restores stream binding");
#endif
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
    {
        bound = 123;
        expected = sizes[i];
        draw_sequential_quads(expected);
    }
    CHECK(draws == 7, "one draw for each list");
#ifdef HALO_GLES
    CHECK(uploads == 7, "GLES keeps streamed indices");
#else
    CHECK(uploads == 1, "oversized lists keep streamed wrapping semantics");
#endif
    return 0;
}
