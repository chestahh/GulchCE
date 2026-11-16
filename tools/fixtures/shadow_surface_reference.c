/* Offline shadow-source visibility oracle. Independent ray/triangle tests,
   never linked into the game. Inputs are exported posed triangles, not maps. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

typedef struct { float p[3][3]; int id; } Triangle;
typedef struct { float low[3], high[3]; int start, count, left, right; } Node;
static Triangle *triangles;
static Node *nodes;
static int node_count, sort_axis;

static int compare_triangle(const void *left, const void *right)
{
    const Triangle *a = left, *b = right;
    float d = a->p[0][sort_axis] + a->p[1][sort_axis] + a->p[2][sort_axis] -
              b->p[0][sort_axis] - b->p[1][sort_axis] - b->p[2][sort_axis];
    return (d > 0) - (d < 0);
}

static int build_tree(int start, int count)
{
    int index = node_count++, i, j, k;
    Node *node = nodes + index;
    node->start = start; node->count = count;
    for (k = 0; k < 3; k++) { node->low[k] = INFINITY; node->high[k] = -INFINITY; }
    for (i = start; i < start + count; i++) for (j = 0; j < 3; j++) for (k = 0; k < 3; k++) {
        float value = triangles[i].p[j][k];
        node->low[k] = fminf(node->low[k], value);
        node->high[k] = fmaxf(node->high[k], value);
    }
    if (count > 8) {
        sort_axis = 0;
        for (k = 1; k < 3; k++)
            if (node->high[k] - node->low[k] > node->high[sort_axis] - node->low[sort_axis]) sort_axis = k;
        qsort(triangles + start, count, sizeof(Triangle), compare_triangle);
        node->left = build_tree(start, count / 2);
        node->right = build_tree(start + count / 2, count - count / 2);
    }
    return index;
}

static float dot(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1]; out[1] = a[2]*b[0] - a[0]*b[2]; out[2] = a[0]*b[1] - a[1]*b[0];
}

static int trace(int index, const float origin[3], const float direction[3],
                 float minimum, float *closest, int first)
{
    Node *node = nodes + index;
    float low = minimum, high = *closest;
    int i, k, found = -1;
    for (k = 0; k < 3; k++) {
        if (fabsf(direction[k]) < 1.e-12f) {
            if (origin[k] < node->low[k] || origin[k] > node->high[k]) return -1;
        } else {
            float a = (node->low[k] - origin[k]) / direction[k];
            float b = (node->high[k] - origin[k]) / direction[k];
            low = fmaxf(low, fminf(a,b)); high = fminf(high, fmaxf(a,b));
            if (low > high) return -1;
        }
    }
    if (node->count > 8) {
        int other;
        found = trace(node->left, origin, direction, minimum, closest, first);
        if (first && found >= 0) return found;
        other = trace(node->right, origin, direction, minimum, closest, first);
        return other >= 0 ? other : found;
    }
    for (i = node->start; i < node->start + node->count; i++) {
        float e1[3], e2[3], offset[3], h[3], q[3], det, u, v, distance;
        for (k = 0; k < 3; k++) {
            e1[k] = triangles[i].p[1][k] - triangles[i].p[0][k];
            e2[k] = triangles[i].p[2][k] - triangles[i].p[0][k];
            offset[k] = origin[k] - triangles[i].p[0][k];
        }
        cross(direction,e2,h); det = dot(e1,h);
        if (fabsf(det) < 1.e-12f) continue;
        u = dot(offset,h) / det;
        if (u < 0 || u > 1) continue;
        cross(offset,e1,q); v = dot(direction,q) / det;
        if (v < 0 || u + v > 1) continue;
        distance = dot(e2,q) / det;
        if (distance > minimum && distance < *closest) {
            *closest = distance; found = triangles[i].id;
            if (first) return found;
        }
    }
    return found;
}

static int read_floats(FILE *file, float *values, size_t count)
{
    size_t i;
    if (fread(values,sizeof(float),count,file) != count) return 0;
    for (i = 0; i < count; i++) if (!isfinite(values[i])) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    FILE *input = NULL, *output = NULL;
    int32_t header[3];
    float *receivers = NULL, *sources = NULL;
    int i, j, k, nearest, result = 1;
    if (argc != 4 || (strcmp(argv[1],"nearest") && strcmp(argv[1],"visibility"))) return 2;
    nearest = !strcmp(argv[1],"nearest");
    input = fopen(argv[2],"rb");
    if (!input || fread(header,4,3,input) != 3) goto done;
    if (header[0] < 1 || header[0] > 1000000 || header[1] < 1 || header[1] > 7000000 ||
        header[2] < 1 || header[2] > 8192 || (nearest && header[2] != 1)) goto done;
    triangles = malloc((size_t)header[0] * sizeof(Triangle));
    nodes = calloc((size_t)header[0] * 2, sizeof(Node));
    receivers = malloc((size_t)header[1] * 3 * sizeof(float));
    sources = malloc((size_t)header[2] * 3 * sizeof(float));
    if (!triangles || !nodes || !receivers || !sources) goto done;
    for (i = 0; i < header[0]; i++) {
        if (!read_floats(input,triangles[i].p[0],9)) goto done;
        triangles[i].id = i;
    }
    if (!read_floats(input,receivers,(size_t)header[1]*3) || !read_floats(input,sources,(size_t)header[2]*3) || fgetc(input) != EOF) goto done;
    fclose(input); input = NULL;
    build_tree(0,header[0]);
    output = fopen(argv[3],"wb");
    if (!output) goto done;
    for (i = 0; i < header[1]; i++) {
        float visible = 0;
        for (j = 0; j < header[2]; j++) {
            float direction[3], length, minimum, closest;
            int32_t id;
            for (k = 0; k < 3; k++) direction[k] = receivers[3*i+k] - sources[3*j+k];
            /* A fixed world-space tolerance must not grow with cube-ray length. */
            length = sqrtf(dot(direction,direction));
            minimum = length > 0 ? 1.e-6f / length : 1;
            closest = 1 - minimum;
            id = minimum < closest ? trace(0,sources+3*j,direction,minimum,&closest,!nearest) : -1;
            if (nearest) {
                if (fwrite(&id,4,1,output) != 1) goto done;
            } else visible += id < 0;
        }
        if (!nearest) {
            visible /= header[2];
            if (fwrite(&visible,4,1,output) != 1) goto done;
        }
    }
    result = 0;
done:
    if (input) fclose(input);
    if (output && fclose(output)) result = 1;
    free(triangles); free(nodes); free(receivers); free(sources);
    if (result) fprintf(stderr,"Invalid reference input or failed I/O\n");
    return result;
}
