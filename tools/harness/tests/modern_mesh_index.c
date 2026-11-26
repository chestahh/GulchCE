#include "harness.h"
#include <stdint.h>

enum { SHADOW_PARTS = 2048, MESH_BUCKETS = 2 * SHADOW_PARTS };
struct vertex_buffer { int id; };
static struct { struct vertex_buffer *vertices; void *triangles; } parts[SHADOW_PARTS];
static unsigned short mesh_index[MESH_BUCKETS];
#include "under_test.inc"

int main(void)
{
    static struct vertex_buffer vertices[64];
    static int triangles[32];
    unsigned i;
    for (i = 0; i < SHADOW_PARTS; ++i)
    {
        struct vertex_buffer *v = vertices + i % 64;
        int *t = triangles + i / 64;
        unsigned b = mesh_bucket(v, t);
        CHECK(!mesh_index[b], "new pair must get a free bucket");
        parts[i].vertices = v; parts[i].triangles = t;
        mesh_index[b] = (unsigned short)(i + 1);
    }
    for (i = 0; i < SHADOW_PARTS; ++i)
        CHECK(mesh_index[mesh_bucket(parts[i].vertices, parts[i].triangles)] == i+1, "cache returns original mesh");
    memset(mesh_index,0,sizeof(mesh_index));
    CHECK(!mesh_index[mesh_bucket(vertices,triangles)], "reset releases previous map ownership");
    return 0;
}
