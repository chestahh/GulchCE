#include "harness.h"
#include <math.h>
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define REAL_MAX 3.402823466e+38F
#define _pi 3.14159265358979323846f
#define MAXIMUM_GEOMETRIES_PER_MODEL 256
#define MAXIMUM_PARTS_PER_MODEL_GEOMETRY 32
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&((t *)(b)->address)[i])
#define csmemset memset
#define D3DLOCK_READONLY 1
typedef unsigned short word;
typedef byte BYTE;
typedef struct { real scale; real_vector3d forward, left, up; real_point3d position; } real_matrix4x3;
struct tag_block { long count; void *address; void *definition; };
struct model_node { real_matrix4x3 runtime_default_inverse_matrix; };
struct model { struct tag_block nodes, geometries; };
struct vertex_buffer { short type; word pad; long count, offset; void *base_address, *hardware_format; };
struct triangle_buffer { short type; word pad; long count; void *base_address, *hardware_format; };
struct model_vertex_compressed {
	real_point3d position; unsigned long normal, binormal, tangent;
	short uv[2]; byte nodes[2]; short node_weight;
};
enum { _triangle_buffer_type_triangles, _triangle_buffer_type_precompiled_strip };
enum { _rasterizer_vertex_type_model_compressed = 5 };
static real_matrix4x3 identity = {1, {1,0,0}, {0,1,0}, {0,0,1}, {0,0,0}};
static real_point3d origin;
static void matrix4x3_transform_point(real_matrix4x3 const *m, real_point3d const *p, real_point3d *o)
{
	o->x = m->position.x + m->scale * (m->forward.i*p->x + m->left.i*p->y + m->up.i*p->z);
	o->y = m->position.y + m->scale * (m->forward.j*p->x + m->left.j*p->y + m->up.j*p->z);
	o->z = m->position.z + m->scale * (m->forward.k*p->x + m->left.k*p->y + m->up.k*p->z);
}
static int created, deleted, fail_create, fail_lock, locks;
static short ce_count, mcc_count;
static byte palette[4] = {2,0,1,3};
static struct vertex_buffer const *palette_key;
short custom_edition_part_palette(struct vertex_buffer const *v, byte const **p)
{ CHECK(v == palette_key, "original vertex descriptor required"); *p = ce_count ? palette : NULL; return ce_count; }
short mcc_cache_part_palette(void const *v, byte const **p)
{ CHECK(v == palette_key, "original MCC descriptor required"); *p = mcc_count ? palette : NULL; return mcc_count; }
void platform_log(char const *s, ...) { (void)s; }
static void IDirect3DVertexBuffer8_Lock(void *b, long offset, long size, BYTE **out, int flags)
{ (void)offset; (void)size; CHECK(flags == D3DLOCK_READONLY, "read-only source"); *out = fail_lock ? NULL : b; locks++; }
#define IDirect3DIndexBuffer8_Lock IDirect3DVertexBuffer8_Lock
static void IDirect3DVertexBuffer8_Unlock(void *b) { (void)b; locks--; }
#define IDirect3DIndexBuffer8_Unlock IDirect3DVertexBuffer8_Unlock
static boolean rasterizer_triangle_buffer_new(struct triangle_buffer *b, short type, long n, void const *p)
{
	if (fail_create) return FALSE;
	b->type = type; b->count = n; b->hardware_format = (void *)p; created++;
	return TRUE;
}
static void rasterizer_triangle_buffer_delete(struct triangle_buffer *b)
{ if (b->hardware_format) deleted++; memset(b, 0, sizeof(*b)); }
#include "under_test.inc"

static struct model model;
static struct model_node bones[4];
static real_matrix4x3 pose[4];
static struct first_person_legs_geometry geometry;
static struct first_person_legs_source_part part;
static struct model_vertex_compressed vertices[7], original[7];
static word indices[9] = {0,1,2, 2,3,4, 4,5,6};
/* 0 torso, 1 thigh, 2 head, 3 second thigh. The head triangle
shares a thigh edge, while the intact torso triangle has no cut edge. */
static boolean upper[4] = {FALSE,FALSE,TRUE,FALSE};
static boolean lower[4] = {FALSE,TRUE,FALSE,TRUE};
static void world(void)
{
	int i;
	for (i=0; i<4; i++) bones[i].runtime_default_inverse_matrix = pose[i] = identity;
	for (i=0; i<7; i++) {
		vertices[i].position.x = 0.2f + i * 0.04f;
		vertices[i].position.y = (i % 2) * 0.1f;
		vertices[i].position.z = 0.4f + i * 0.02f;
		vertices[i].nodes[0] = i < 2 ? 0 : i < 6 ? 3 : 6;
		vertices[i].nodes[1] = 255;
		vertices[i].node_weight = 32767;
	}
	memcpy(original, vertices, sizeof(vertices));
	model.nodes.count=4; model.nodes.address=bones;
	model.geometries.count=1; model.geometries.address=&geometry;
	geometry.parts.count=1; geometry.parts.address=&part;
	part.vertex_buffer.type=5; part.vertex_buffer.count=7; part.vertex_buffer.hardware_format=vertices;
	part.triangle_buffer.type=0; part.triangle_buffer.count=3; part.triangle_buffer.hardware_format=indices;
	palette_key=&part.vertex_buffer;
}
static struct triangle_buffer const *get(void)
{ return first_person_legs_geometry_get(10,&model,upper,lower,&part.vertex_buffer,&part.triangle_buffer); }
static int close_real(real a, real b) { return fabs(a-b) < 0.0001f; }

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	world();
	CASE("mesh") {
		struct triangle_buffer const *out=get();
		CHECK(out && out->count==2, "head triangles removed, torso retained");
		CHECK(memcmp(first_person_legs_parts[0].indices,indices,6*sizeof(word))==0,"intact triangles");
		CHECK(memcmp(vertices,original,sizeof(vertices))==0,"vertices never rewritten");
		CHECK(out!=&part.triangle_buffer && part.triangle_buffer.count==3,"private indices");
		CHECK(get()==out && created==1,"stable buffer for queued shield draws");
	}
	CASE("strip") {
		boolean keep[7]={1,1,1,1,1,1,1};
		word strip[8]={0,1,2,2,3,4,5,6}, output[18];
		word expected[12]={0,1,2, 3,2,4, 3,4,5, 5,4,6};
		CHECK(first_person_legs_select(strip,6,1,keep,7,output)==4,"degenerate strip");
		CHECK(!memcmp(output,expected,sizeof(expected)),"strip winding survives degenerates");
	}
	CASE("weights") {
		struct model_vertex_compressed v=vertices[2];
		CHECK(first_person_legs_vertex_kept(&v,4,upper,0,NULL),"unused invalid node ignored");
		v.nodes[1]=6; v.node_weight=32766;
		CHECK(!first_person_legs_vertex_kept(&v,4,upper,0,NULL),"even small head influence removed");
		v.node_weight=0; v.nodes[0]=255; v.nodes[1]=3;
		CHECK(first_person_legs_vertex_kept(&v,4,upper,0,NULL),"second influence alone");
		v.node_weight=-1;
		CHECK(!first_person_legs_vertex_kept(&v,4,upper,0,NULL),"invalid signed weight");
	}
	CASE("palette") {
		struct model_vertex_compressed v=vertices[2];
		v.nodes[0]=0;
		CHECK(!first_person_legs_vertex_kept(&v,4,upper,4,palette),"palette maps head");
		v.nodes[0]=3;
		CHECK(first_person_legs_vertex_kept(&v,4,upper,4,palette),"palette maps torso");
		v.nodes[0]=12;
		CHECK(!first_person_legs_vertex_kept(&v,4,upper,4,palette),"palette bounds");
		ce_count=4; CHECK(get()!=NULL,"CE palette supported");
		first_person_legs_geometry_reset(); ce_count=0; mcc_count=4;
		CHECK(get()!=NULL,"MCC palette supported");
	}
	CASE("malformed") {
		part.triangle_buffer.count=0;
		CHECK(!get() && !first_person_legs_part_count,"empty part does not poison cache");
		part.triangle_buffer.count=3;
		indices[8]=7;
		CHECK(!first_person_legs_geometry_prepare(10,&model,upper,lower),"invalid triangle hides whole local body");
		CHECK(!first_person_legs_bytes && !locks,"failed buffer cleanup");
		first_person_legs_geometry_reset(); indices[8]=6; part.vertex_buffer.type=4;
		CHECK(!first_person_legs_geometry_prepare(10,&model,upper,lower),"unsupported format");
		first_person_legs_geometry_reset(); part.vertex_buffer.type=5; model.nodes.count=65540;
		CHECK(!first_person_legs_geometry_prepare(10,&model,upper,lower),"node count cannot wrap");
	}
	CASE("failure") {
		fail_create=1;
		CHECK(!first_person_legs_geometry_prepare(10,&model,upper,lower),"allocation failure hides body");
		CHECK(!first_person_legs_bytes && !locks,"no retained allocation");
		first_person_legs_geometry_reset(); fail_create=0; fail_lock=1;
		CHECK(!first_person_legs_geometry_prepare(10,&model,upper,lower),"failed lock");
		first_person_legs_geometry_reset(); fail_lock=0; first_person_legs_bytes=FIRST_PERSON_LEGS_BYTES;
		CHECK(!get(),"budget bounded");
	}
	CASE("lifecycle") {
		CHECK(first_person_legs_geometry_prepare(10,&model,upper,lower),"prepare");
		CHECK(first_person_legs_geometry_prepare(10,&model,upper,lower) && created==1,"cache survives frames");
		first_person_legs_geometry_reset();
		CHECK(deleted==created && !first_person_legs_bytes && !first_person_legs_part_count,"reset releases");
		CHECK(first_person_legs_geometry_prepare(11,&model,upper,lower) && created==2,"new map");
	}
	CASE("boundary") {
		CHECK(get()!=NULL,"mesh");
		/* Torso vertices 0,1 plus leg vertex 4 at the head cut. Vertex
		5 is only in the removed triangle and must not inflate clearance. */
		CHECK(first_person_legs_parts[0].boundary_count==3,"all non-leg and cut-edge vertices");
	}
	CASE("clearance") {
		int f, movement;
		real fovs[]={20,70,90,110,120,150};
		real_vector3d view={0,1,0};
		real_point3d eye={0,0,1}, pivot={0,0,0};
		vertices[0].nodes[1]=9; vertices[0].node_weight=12000;
		bones[0].runtime_default_inverse_matrix.position.z=-0.2f;
		CHECK(get()!=NULL,"mesh");
		for(movement=0;movement<6;movement++) for(f=0;f<6;f++) {
			int i;
			real angle=fovs[f]*_pi/180.f;
			real offset, expected=0.16f;
			pose[0].position.z=movement*0.13f;
			pose[1].position.z=movement*-0.07f;
			pose[3].position.z=movement*-0.3f;
			eye.z=movement&1 ? 0.65f : 1.f;
			offset=first_person_legs_geometry_setback(10,pose,&pivot,0,1,&view,&eye,angle,0.16f);
			/* Independent surface reconstruction from original vertices,
			inverse bind and animated bones. Test both above/below eye. */
			for(i=0;i<5;i++) if(i<2 || i==4) {
				real_point3d a,b,p;
				real w=vertices[i].node_weight/32767.f, behind, needed;
				int n=vertices[i].nodes[0]/3;
				int n2=w==1.f ? 0 : vertices[i].nodes[1]/3;
				matrix4x3_transform_point(&bones[n].runtime_default_inverse_matrix,&vertices[i].position,&a);
				matrix4x3_transform_point(&pose[n],&a,&p); a=p;
				matrix4x3_transform_point(&bones[n2].runtime_default_inverse_matrix,&vertices[i].position,&b);
				matrix4x3_transform_point(&pose[n2],&b,&p); b=p;
				p.x=a.x*w+b.x*(1-w); p.z=a.z*w+b.z*(1-w);
				behind=eye.y-(pivot.y+(p.x-pivot.x)-offset);
				needed=fabs(eye.z-p.z)*tan(angle/2)+0.03f/cos(angle/2);
				CHECK(behind+0.00001f>=needed,"surface enters view: movement %d FOV %.0f vertex %d",movement,fovs[f],i);
				expected=MAX(expected,needed+p.x);
			}
			CHECK(close_real(offset,expected),"no unnecessary retreat beyond animated surface");
		}
	}
	CASE("invalid-view") {
		real_vector3d view={1,0,0};
		CHECK(get()!=NULL,"mesh");
		CHECK(first_person_legs_geometry_setback(10,pose,&origin,1,0,&view,&origin,0,0.16f)==REAL_MAX,"invalid FOV");
	}
	first_person_legs_geometry_reset();
	return 0;
}
