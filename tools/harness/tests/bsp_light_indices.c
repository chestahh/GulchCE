/* Check cached byte offsets in the real upload functions against a small GL ring. */
#include "harness.h"

#define HALO_WINDOWS 1
#define INDEX_BUFFER_SIZE 256
#define GL_ELEMENT_ARRAY_BUFFER 0
#define GL_STREAM_DRAW 1
typedef int BOOL;

static struct { unsigned long index_offset, index_buffer; } device;
static struct { unsigned long streamed_bytes; } stats;
static byte storage[INDEX_BUFFER_SIZE];
static unsigned long writes, uploaded, wraps;

static void state_element_array_buffer(unsigned long buffer)
{
    CHECK(buffer == device.index_buffer, "wrong index buffer");
}

static void glBufferData(int target, unsigned long size, const void *data, int usage)
{
    CHECK(size == sizeof(storage) && !data, "unexpected ring allocation");
    memset(storage, 0xF3, size);
    wraps++;
}

static void buffer_upload(int target, unsigned long offset, unsigned long size, const void *data)
{
    CHECK(offset + size <= sizeof(storage), "storage bounds");
    memcpy(storage + offset, data, size);
    writes++;
    uploaded += size;
}

#include "under_test.inc"

static void draw(const byte *source, unsigned long size)
{
    unsigned long offset;
    stats.streamed_bytes += size;
    offset = bsp_light_index_upload(source, size);
    CHECK(stats.streamed_bytes == uploaded, "stream statistics must count actual transfer bytes");
    CHECK(offset + size <= sizeof(storage) && !memcmp(storage + offset, source, size),
          "each material must read its original indices");
}

int main(int argc, char **argv)
{
    byte light[252], other[248];
    unsigned int i;
    CHECK(argc == 2, "expected one case");
    for (i = 0; i < sizeof(light); i++) light[i] = (byte)(i ^ 0x31);
    memset(other, 0x67, sizeof(other));
    bsp_light_index_batch_set(light, sizeof(light));
    if (!strcmp(argv[1], "sparse"))
    {
        draw(light + 16, 10);
        draw(light + 26, 8);
        draw(light + 80, 14);
        draw(light + 4, 6);
        CHECK(writes == 4 && uploaded == 38, "sparse material draws upload exact slices");
    }
    else
    {
        for (i = 0; i < 8; i++) draw(light + 8*i, 8);
        CHECK(writes < 8, "adjacent materials must share an upload");
        if (!strcmp(argv[1], "wrap"))
        {
            draw(other, sizeof(other));
            CHECK(wraps, "an unrelated upload must wrap this ring");
            draw(light + 56, 8);
        }
        else if (!strcmp(argv[1], "new-pass"))
        {
            bsp_light_index_batch_set(NULL, 0);
            memset(light, 0x17, sizeof(light));
            draw(light + 56, 8);
            bsp_light_index_batch_set(light, sizeof(light));
            draw(light + 56, 8);
        }
        else CHECK(!strcmp(argv[1], "dense"), "unknown case");
    }
    return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
