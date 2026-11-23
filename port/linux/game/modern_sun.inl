/* One near-field orthographic map, independent of the feature-light selector.
   Reserve the full light-space depth of the BSP so distant cliffs still block
   sunlight. Families at the edge share a receiver-space fade with their old projection. */
extern int config_boolean(const char *name);
static long sun_families[MAXIMUM_OBJECTS_PER_MAP];
static int prepared_families;
int modern_shadows_sun_casters(void) { return sun_count; }

static int sun_owns(long object)
{
    for (int i = 0; i < sun_count; i++)
        if (sun_families[i] == object) return 1;
    return 0;
}

/* Clip the swept caster sphere against the viewing frustum. An offscreen
   caster can survive if a ray travelling away from the sun enters the view. */
static int sun_reaches_view(real_point3d const *center, real radius, real_vector3d const *toward)
{
    real first = 0.f, last = REAL_MAX;
    for (int i = 0; i < 6; i++)
    {
        real_plane3d const *plane = &render.frustum.world_planes[i];
        real slope = dot_product3d(&plane->n, toward);
        real distance = plane3d_distance_to_point(plane, center) - radius;
        if (slope > 0.f) first = MAX(first, distance / slope);
        else if (slope < 0.f) last = MIN(last, distance / slope);
        else if (distance > 0.f) return 0;
        if (first > last) return 0;
    }
    return 1;
}

/* A fixed grate stays in the cached visibility map. A change to its evaluated
   UV transform or texture invalidates it before any receiver can use it. */
static int sun_static_unchanged(void)
{
    int i;
    if (sun_cutout_count > SUN_CUTOUTS) return 0;
    for (i=0; i<sun_cutout_count; i++)
    {
        const struct structure_material *m = sun_cutouts[i].material;
        const void *alpha;
        float scale[2]={1,1}, uv[8];
        if (material(shader_definition_get(m->shader.index),m->permutation_index,&alpha,scale,NULL,uv)!=1 ||
            alpha!=sun_cutouts[i].texture || memcmp(uv,sun_cutouts[i].uv,sizeof(uv))) return 0;
    }
    return 1;
}

static short sun_cluster_sky(int cluster)
{
    if (!VALID_INDEX(cluster, cached_bsp->clusters.count)) return NONE;
    return TAG_BLOCK_GET_ELEMENT(&cached_bsp->clusters,cluster,struct structure_cluster)->sky_index;
}

/* References identify which cluster(s) own each BSP triangle. Ambiguous
   shared surfaces block the light but retain their original bake. */
static int sun_classify_surfaces(void)
{
    struct sun_reference { long surface, node; };
    const struct sun_reference *refs = cached_bsp->surface_references.address;
    const int count = cached_bsp->surfaces.count;
    int leaf;
    if (sun_surface_sky) return 1;
    if (count <= 0 || !refs || !(sun_surface_sky=malloc(count))) return 0;
    memset(sun_surface_sky,254,count);
    for (leaf=0; leaf<cached_bsp->leaves.count; leaf++)
    {
        struct structure_leaf const *l=TAG_BLOCK_GET_ELEMENT(&cached_bsp->leaves,leaf,struct structure_leaf);
        int first=l->first_surface_reference_index, n=l->surface_reference_count, j;
        int sky=sun_cluster_sky(l->cluster_index);
        unsigned char owner=sky>=0 && sky<254 ? (unsigned char)sky : 255;
        if (first<0 || n<0 || first>cached_bsp->surface_references.count-n) continue;
        for (j=0; j<n; j++)
        {
            long surface=refs[first+j].surface;
            if (!VALID_INDEX(surface,count)) continue;
            if (sun_surface_sky[surface]==254) sun_surface_sky[surface]=owner;
            else if (sun_surface_sky[surface]!=owner) sun_surface_sky[surface]=255;
        }
    }
    return 1;
}

/* Looking down in first person puts the old forward focus beyond the body,
   often below the floor. Fit towards the player's shadow-map footprint as
   the camera approaches the body and pitches down. Detached cameras keep
   their own near coverage. */
static float sun_detail_focus(real_point3d *focus, float radius)
{
    long unit;
    real_point3d center;
    float extent, pitch, proximity, weight;
    if (render.local_player_index == NONE || render.camera.forward.k >= -.1f) return radius;
    unit = player_control_get_unit_index(render.local_player_index);
    if (unit == NONE || !object_try_and_get(unit)) return radius;
    object_get_bounding_sphere(unit,&center,&extent);
    render_interpolation_object_point(unit,&center);
    if (!(extent > 0.f && extent < radius)) return radius;
    proximity = PIN(2.f-distance3d(&center,&render.camera.position),0.f,1.f);
    pitch = PIN((-render.camera.forward.k-.1f)/.5f,0.f,1.f);
    weight = pitch*proximity;
    weight *= weight*(3.f-2.f*weight);
    points_interpolate(focus,&center,weight,focus);
    return radius + weight*(MAX(.25f,extent*1.15f)-radius);
}

static void sun_prepare(void)
{
    const float radius = 12.f;
    float detail[2], detail_radius, region[8];
    real_matrix4x3 basis;
    real_vector3d toward;
    real_rgb_color tint;
    real_point3d focus, near_focus;
    struct object_iterator it;
    struct object_datum *obj;
    float frame[16], color[3], depth_extent = 1.f;
    int ok = 1, reuse, i;
    short sky;
    prepared_families = 0;
    prepared_sky = NONE;
    int mode = setting();
    if (!cached_bsp || !mode || !config_boolean("display.environment_sun") ||
        !game_engine_allow_dynamic_lighting() || !rasterizer_debug_options.draw_environment_shadows ||
        global_window_parameters.rasterizer_target != 0) return;
    sky = sun_cluster_sky(render.cluster_index);
    if (!render_sky_sun(sky,&toward,&tint)) return;
    basis.scale = 1.f;
    scale_vector3d(&toward,-1.f,&basis.up);
    perpendicular3d(&basis.up,&basis.forward);
    normalize3d(&basis.forward);
    cross_product3d(&basis.up,&basis.forward,&basis.left);
    point_from_line3d(&render.camera.position,&render.camera.forward,radius*.65f,&focus);
    point_from_line3d(&render.camera.position,&render.camera.forward,4.f,&near_focus);
    detail_radius = sun_detail_focus(&near_focus,3.f);
    basis.position = *global_origin3d;
    /* World-anchored tile steps are integral texels at all supported tiers.
       Quantize translation in the shadow plane only. Depth stays BSP-fixed. */
    for (i = 0; i < 2; i++)
    {
        real_vector3d const *axis = i ? &basis.left : &basis.forward;
        float along = dot_product3d(axis,(real_vector3d const *)&focus);
        float snapped = floorf(along / 1.5f + .5f) * 1.5f;
        float near_along = dot_product3d(axis,(real_vector3d const *)&near_focus);
        /* Express the finer tile in the wide tile's light plane. Both share
           the same BSP-fixed depth, including blockers far above the camera. */
        float detail_step = detail_radius/64.f;
        detail[i] = floorf(near_along/detail_step+.5f)*detail_step - snapped;
        /* Cache the BSP on a snapped grid, but move coverage continuously.
           Even the far end of the fade stays inside the snapped depth map. */
        for (int j=0;j<3;j++) region[4*i+j] = axis->n[j]/radius;
        region[4*i+3] = -along/radius;
        point_from_line3d(&basis.position,axis,snapped,&basis.position);
    }
    const real_rectangle3d *bounds = &cached_bsp->world_bounds;
    float mid = 0.f;
    for (i = 0; i < 3; i++)
    {
        mid += basis.up.n[i] * (bounds->n[2*i+1]+bounds->n[2*i])*.5f;
        depth_extent += fabsf(basis.up.n[i]) * (bounds->n[2*i+1]-bounds->n[2*i])*.5f;
    }
    point_from_line3d(&basis.position,&basis.up,mid,&basis.position);
    matrix_to_gl(&basis,frame);
    for (i = 0; i < 3; i++) frame[8+i] *= radius/depth_extent;
    if (!sun_classify_surfaces()) return;
    reuse = modern_environment_sun_begin(frame,radius,region,detail,detail_radius,sun_static_unchanged(),sky,mode == 2);
    if (!reuse) return;
    if (reuse == 1)
    {
        sun_cutout_count=0;
        ok = modern_environment_sun_static(world_casters_clipped(cached_bsp,&basis.position,radius,frame));
    }
    /* Intersect the caster with the tile. A large or offscreen mesh may
       shadow a visible receiver even when its origin lies outside the tile. */
    object_iterator_new(&it,_object_mask_all,0);
    while (ok && (obj = object_iterator_next(&it)) != NULL)
    {
        real_point3d center;
        real_vector3d delta;
        real extent;
        int remaining = 128;
        if (obj->object.parent_object_index != NONE) continue;
        if (obj->object.first_child_object_index == NONE &&
            (TEST_FLAG(obj->object.flags,_object_invisible_bit) ||
             object_definition_get(obj->definition_index)->object.model.index == NONE)) continue;
        object_get_render_bounding_sphere(it.index,&center,&extent);
        render_interpolation_object_point(it.index,&center);
        if (obj->object.first_child_object_index != NONE &&
            !environmental_family_bounds(it.index,&center,&extent,&remaining,1)) continue;
        vector_from_points3d(&basis.position,&center,&delta);
        float x = dot_product3d(&basis.forward,&delta);
        float y = dot_product3d(&basis.left,&delta);
        float z = fabsf(dot_product3d(&basis.up,&delta));
        if (fabsf(x) > radius+extent || fabsf(y) > radius+extent || z > depth_extent+extent ||
            !sun_reaches_view(&center,extent,&toward)) continue;
        remaining = 128;
        modern_environment_sun_detail_caster(
            fabsf(x-detail[0]) <= detail_radius+extent &&
            fabsf(y-detail[1]) <= detail_radius+extent);
        ok = environmental_casters(it.index,&remaining);
        if (z+extent < depth_extent &&
            sun_cluster_sky(obj->object.location.cluster_index) == sky)
            sun_families[prepared_families++] = it.index;
    }
    for (i=0;i<3;i++) color[i] = .65f*PIN(tint.n[i],0.f,1.f);
    if (modern_environment_sun_finish(ok,color)) prepared_sky = sky;
}

void modern_shadows_sun_project(void)
{
    float camera[16];
    if (prepared_sky == NONE) return;
    matrix_to_gl(&global_window_parameters.frustum.view_to_world,camera);
    if (modern_environment_sun_project(camera,(const float *)global_window_parameters.frustum.projection_matrix))
        sun_count = prepared_families;
}
