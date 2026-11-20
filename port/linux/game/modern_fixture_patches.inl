/* A radiosity patch has a source location only where connected triangles
   have one emitting plane. Curved glow cones do not qualify. Discover once
   per active BSP. The texture, map name and cache family are irrelevant. */
enum { PATCH_TRIANGLES=64, PATCH_LIMIT=256 };
static struct spot_source patches[PATCH_LIMIT];
static int patch_count;

static int patch_connected(const real_point3d a[3],const real_point3d b[3])
{
    int shared=0;
    for (int i=0;i<3;i++)
        for (int j=0;j<3;j++)
            if (distance_squared3d(&a[i],&b[j])<1.e-10f) { shared++; break; }
    return shared>=2;
}

static void material_patches(const struct structure_material *m)
{
    const struct shader_radiosity_properties *r;
    if (m->shader.index==NONE || m->surface_count<=0 || m->surface_count>PATCH_TRIANGLES ||
        !m->vertices.hardware_format || m->first_surface_index<0 ||
        m->first_surface_index>cached_bsp->surfaces.count-m->surface_count) return;
    r=&shader_definition_get(m->shader.index)->base.radiosity;
    if (!(r->power>0.f)) return;
    if (m->vertices.type!=_rasterizer_vertex_type_environment_compressed &&
        m->vertices.type!=_rasterizer_vertex_type_environment_uncompressed) return;
    int stride=rasterizer_geometry_get_vertex_size(m->vertices.type);
    if (stride<12 || m->vertices.count<=0 || m->vertices.count>65535) return;
    BYTE *vertices=NULL;
    real_point3d triangles[PATCH_TRIANGLES][3];
    int groups[PATCH_TRIANGLES];
    real_vector3d normals[PATCH_TRIANGLES];
    struct structure_surface const *surfaces=(const struct structure_surface *)cached_bsp->surfaces.address+m->first_surface_index;
    IDirect3DVertexBuffer8_Lock(m->vertices.hardware_format,0,m->vertices.count*stride,&vertices,D3DLOCK_READONLY);
    if (!vertices) return;
    int ok=1, i, j;
    for (i=0;i<m->surface_count && ok;i++)
    {
        groups[i]=i;
        normals[i]=*global_zero_vector3d;
        for (j=0;j<3;j++)
        {
            int v=surfaces[i].vertex_indices[j];
            if (v>=m->vertices.count) { ok=0; break; }
            BYTE const *vertex=vertices+v*stride;
            memcpy(&triangles[i][j],vertex,sizeof(real_point3d));
            real_vector3d normal;
            if (m->vertices.type==_rasterizer_vertex_type_environment_compressed)
            {
                unsigned long packed;
                memcpy(&packed,vertex+12,sizeof(packed));
                normal=uncompress_int32_to_real_vector3d(packed);
            }
            else memcpy(&normal,vertex+12,sizeof(normal));
            add_vectors3d(&normals[i],&normal,&normals[i]);
        }
    }
    IDirect3DVertexBuffer8_Unlock(m->vertices.hardware_format);
    if (!ok) return;
    for (i=0;i<m->surface_count;i++)
        for (j=0;j<i;j++)
            if (groups[i]!=groups[j] && patch_connected(triangles[i],triangles[j]))
            {
                int from=groups[i],to=groups[j];
                for (int k=0;k<=i;k++) if (groups[k]==from) groups[k]=to;
            }
    for (i=0;i<m->surface_count && patch_count<PATCH_LIMIT;i++)
    {
        if (groups[i]!=i) continue;
        struct spot_source s={0};
        float total=0;
        for (j=0;j<m->surface_count;j++)
        {
            real_vector3d a,b,n;
            if (groups[j]!=i) continue;
            vector_from_points3d(&triangles[j][0],&triangles[j][1],&a);
            vector_from_points3d(&triangles[j][0],&triangles[j][2],&b);
            cross_product3d(&a,&b,&n);
            if (dot_product3d(&n,&normals[j])<0) scale_vector3d(&n,-1.f,&n);
            float area=magnitude3d(&n);
            total+=area;
            for (int k=0;k<3;k++)
            {
                s.axis.n[k]+=n.n[k];
                s.origin.n[k]+=area*(triangles[j][0].n[k]+triangles[j][1].n[k]+triangles[j][2].n[k])/3.f;
            }
        }
        if (!(total>.00001f) || normalize3d(&s.axis)<.999f*total) continue;
        for (int k=0;k<3;k++)
        {
            s.origin.n[k]/=total;
            s.color[k]=.4f*PIN(r->color_of_emitted_light.n[k],0.f,1.f);
        }
        point_from_line3d(&s.origin,&s.axis,.01f,&s.origin);
        s.angle=1.20f;
        s.range=PIN(sqrtf(r->power*total*.5f),.25f,8.f);
        s.color[3]=s.range;
        patches[patch_count++]=s;
    }
}

static void admit_patches(struct spot_source chosen[2])
{
    if (!patches_ready)
    {
        patch_count=0;
        for (int lm=0;lm<cached_bsp->lightmaps.count;lm++)
        {
            const struct structure_lightmap *l=TAG_BLOCK_GET_ELEMENT(&cached_bsp->lightmaps,lm,struct structure_lightmap);
            for (int m=0;m<l->materials.count;m++)
                material_patches(TAG_BLOCK_GET_ELEMENT(&l->materials,m,struct structure_material));
        }
        patches_ready=1;
    }
    for (int i=0;i<patch_count;i++) admit_fixture(chosen,patches[i]);
}
