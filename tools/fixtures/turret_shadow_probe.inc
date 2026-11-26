/* Test-only source disk and layered depth storage. Eight logical lights remain.
   No geometry is excluded; each source receives a complete depth comparison. */
static PFNGLFRAMEBUFFERTEXTURELAYERPROC probe_layer;
static float probe_axis[3], probe_up[3];
static int probe_turret;
static int mesh_faces(const struct shadow_mesh *,const float *,int,const float *);

static int probe_capacity(void)
{
    static int count;
    if (!count)
    {
        const char *value=getenv("HALO_TURRET_SOURCES");
        count=value?atoi(value):1;
        if(count!=2 && count!=4) count=1;
        probe_layer=(PFNGLFRAMEBUFFERTEXTURELAYERPROC)SDL_GL_GetProcAddress("glFramebufferTextureLayer");
        platform_log("TURRET PROBE sources %d disk radius %s ahead %s",count,
            getenv("HALO_TURRET_RADIUS")?getenv("HALO_TURRET_RADIUS"):"0.06",
            getenv("HALO_TURRET_AHEAD")?getenv("HALO_TURRET_AHEAD"):"0.06");
    }
    return count;
}

void turret_probe_source(const float *axis,const float *up)
{
    probe_turret=axis && up;
    if(probe_turret) { memcpy(probe_axis,axis,sizeof(probe_axis)); memcpy(probe_up,up,sizeof(probe_up)); }
}

static void probe_offsets(struct shadow_map *value)
{
    float right[3], radius=.06f, ahead=.06f;
    int i,j;
    value->samples=probe_turret?probe_capacity():1;
    probe_capacity();
    memset(value->offsets,0,sizeof(value->offsets));
    if(getenv("HALO_TURRET_RADIUS")) radius=(float)atof(getenv("HALO_TURRET_RADIUS"));
    if(getenv("HALO_TURRET_AHEAD")) ahead=(float)atof(getenv("HALO_TURRET_AHEAD"));
    if(!(radius>=0 && radius<=.2f)) radius=.06f;
    if(!(ahead>=0 && ahead<=.2f)) ahead=.06f;
    right[0]=probe_axis[1]*probe_up[2]-probe_axis[2]*probe_up[1];
    right[1]=probe_axis[2]*probe_up[0]-probe_axis[0]*probe_up[2];
    right[2]=probe_axis[0]*probe_up[1]-probe_axis[1]*probe_up[0];
    if(value->samples>1)
        for(i=0;i<value->samples;i++) for(j=0;j<3;j++)
            value->offsets[i][j]=probe_axis[j]*ahead+(i<2?probe_up[j]:right[j])*radius*(i%2?-1.f:1.f);
    value->offsets[0][3]=(float)value->samples;
}

static int probe_faces(const struct shadow_mesh *mesh,const float *matrices,int nodes)
{
    float source[4];
    int sample,axis,faces=0;
    for(sample=0;sample<map->samples;sample++)
    {
        memcpy(source,map->light,sizeof(source));
        for(axis=0;axis<3;axis++) source[axis]+=map->offsets[sample][axis];
        faces|=mesh_faces(mesh,matrices,nodes,source);
    }
    return faces;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
