/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

/* Localized baked emitters and the submitted flashlight have separate
   ownership from both the directional sun and the eight dynamic lights. */
#include "devices/devices.h"
#include "devices/device_light_fixtures.h"

#include "interface/first_person_weapons.h"
#include "physics/collisions.h"

/* Stock pulls the first-person projector back half a unit along the sight
   line. A shadow cast from behind the eye disappears behind its caster.
   Use the weapon's rendered mount and keep the authored beam direction. */
void modern_flashlight_mount(long wearer, struct rasterizer_light_submit_parameters *light)
{
    struct object_marker mount;
    struct collision_result hit;
    real_vector3d offset;
    real_point3d origin;
    long weapon;
    if (!setting() || render.local_player_index == NONE ||
        player_control_get_unit_index(render.local_player_index) != wearer ||
        !config_boolean("display.flashlight_shadows") || !game_engine_allow_dynamic_lighting()) return;
    point_from_line3d(&light->position,&light->forward,.5f,&origin);
    if (distance_squared3d(&origin,&render.camera.position) < .0025f)
    {
        short slot = unit_get(wearer)->unit.current_weapon_index;
        if (slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT)
        {
            weapon = unit_inventory_get_weapon(wearer,slot);
            if (weapon != NONE && first_person_weapon_get_marker_by_name_render(weapon,"primary trigger",&mount,1)>0)
                origin = mount.matrix.position;
        }
    }
    vector_from_points3d(&render.camera.position,&origin,&offset);
    if (collision_test_vector(FLAG(_collision_test_structure_bit)|FLAG(_collision_test_front_facing_surfaces_bit),
        &render.camera.position,&offset,wearer,&hit))
        point_from_line3d(&render.camera.position,&offset,MAX(0.f,hit.t-.01f),&origin);
    light->position=origin;
}

struct spot_source
{
    real_point3d origin;
    real_vector3d axis, up;
    float range, angle, color[4], distance;
    int placed, priority;
};
struct spot_definition
{
    long flags;
    real radius, modifier[2], falloff, cutoff, flare_radius, cos_inner, cos_outer, specular_scale, sin_outer;
    byte reserved[8];
    long interpolation;
    real_argb_color low, high;
    byte reserved58[0x7c];
    real baked_intensity;
    real_rgb_color baked_color;
};
typedef char spot_color_offset[offsetof(struct spot_definition,low)==0x38 ? 1 : -1];
typedef char spot_intensity_offset[offsetof(struct spot_definition,baked_intensity)==0xd4 ? 1 : -1];

enum { spot_dynamic = 1 << 0, spot_first_person_flashlight = 1 << 4 };

/* The camera beam is positioned after cluster visibility collects light markers.
   Include its live attachment even when its third-person marker was culled. */
short modern_flashlight_include(short view, long *lights, short count, short capacity)
{
    if (!setting() || view == NONE || !config_boolean("display.flashlight_shadows") || capacity <= 0)
        return count;
    long wearer = player_control_get_unit_index(view);
    struct unit_datum *unit = wearer == NONE ? NULL : unit_get(wearer);
    if (!unit || unit->unit.integrated_light_power <= 0.f) return count;
    const struct tag_block *attachments = &object_definition_get(unit->definition_index)->object.attachments;
    for (int i = 0; i < MIN(attachments->count, MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT); i++)
    {
        if (unit->object.attachment_types[i] != _object_attachment_type_light) continue;
        long handle = unit->object.attachment_indices[i];
        if (handle == NONE || light_port_emitter_object(handle) != wearer) continue;
        const struct object_attachment_definition *a = TAG_BLOCK_GET_ELEMENT(attachments,i,struct object_attachment_definition);
        if (a->type.index == NONE || a->type.group_tag != LIGHT_DEFINITION_TAG) continue;
        const struct spot_definition *d = tag_get(LIGHT_DEFINITION_TAG,a->type.index);
        if (!(d->flags & spot_first_person_flashlight)) continue;
        int pos = 0;
        while (pos < count && lights[pos] != handle) pos++;
        if (pos == count && count < capacity) count++;
        if (pos == capacity) pos--;
        memmove(lights+1, lights, pos*sizeof(*lights));
        lights[0] = handle;
        break;
    }
    return count;
}

static void spot_frame(const struct spot_source *s,float frame[16])
{
    real_vector3d x,y;
    float near=.005f, far=s->range, scale=1.f/tanf(s->angle);
    /* The submitted flashlight supplies a continuous roll reference. */
    cross_product3d(&s->axis,&s->up,&x);
    if (normalize3d(&x)==0.f)
    {
        perpendicular3d(&s->axis,&x);
        normalize3d(&x);
    }
    cross_product3d(&s->axis,&x,&y);
    memset(frame+12,0,4*sizeof(float));
    frame[14]=near*far/(far-near);
    for (int j=0;j<3;j++)
    {
        frame[4*j]=scale*x.n[j];
        frame[4*j+1]=scale*y.n[j];
        frame[4*j+2]=-s->axis.n[j]*near/(far-near);
        frame[4*j+3]=s->axis.n[j];
        for (int i=0;i<4;i++) frame[12+i]-=s->origin.n[j]*frame[4*j+i];
    }
}

static void admit_fixture(struct spot_source chosen[2], struct spot_source s)
{
    real_point3d end;
    float base=s.range*tanf(s.angle);
    point_from_line3d(&s.origin,&s.axis,s.range,&end);
    for (int plane=0;plane<6;plane++)
    {
        const real_plane3d *p=&render.frustum.world_planes[plane];
        float axial=dot_product3d(&p->n,&s.axis);
        float width=base*sqrtf(MAX(0.f,1.f-axial*axial));
        if (plane3d_distance_to_point(p,&s.origin)>0 && plane3d_distance_to_point(p,&end)>width) return;
    }
    s.distance=distance_squared3d(&render.camera.position,&s.origin);
    /* An authored fixture near this view takes precedence over inferred
       radiosity patches. A distant fixture cannot monopolize a room's slots. */
    s.priority=s.placed && s.distance<=4.f*s.range*s.range;
    for (int i=0;i<2;i++)
    {
        if (chosen[i].range && distance_squared3d(&chosen[i].origin,&s.origin)<.015625f) return;
        if (chosen[i].range && (chosen[i].priority>s.priority ||
            (chosen[i].priority==s.priority && chosen[i].distance<=s.distance))) continue;
        if (!i) chosen[1]=chosen[0];
        chosen[i]=s;
        return;
    }
}

#include "modern_fixture_patches.inl"

static void find_fixtures(struct spot_source chosen[2])
{
    struct object_iterator it;
    struct object_datum *object;
    /* Fixed map geometry can contribute to the BSP bake. Held items and
       transient projectiles also carry radiosity values in retail tags. */
    object_iterator_new(&it,_object_mask_device | _object_mask_scenery |
        _object_mask_sound_scenery | _object_mask_placeholder,0);
    while ((object=object_iterator_next(&it))!=NULL)
    {
        struct tag_block const *attachments=&object_definition_get(object->definition_index)->object.attachments;
        for (int j=0;j<MIN(attachments->count,MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT);j++)
        {
            struct object_attachment_definition const *a=TAG_BLOCK_GET_ELEMENT(attachments,j,struct object_attachment_definition);
            if (a->type.index==NONE || a->type.group_tag!=LIGHT_DEFINITION_TAG) continue;
            const struct spot_definition *d=tag_get(LIGHT_DEFINITION_TAG,a->type.index);
            struct object_marker marker;
            const struct light_fixture_datum *fixture = object->object.type == _object_type_light_fixture ?
                (const struct light_fixture_datum *)object : NULL;
            if (d->flags & (spot_dynamic | spot_first_person_flashlight)) continue;
            if (!fixture && !(d->baked_intensity > 0.f)) continue;
            struct spot_source s={0};
            s.placed=object->object.location.cluster_index!=NONE && render_location_visible(&object->object.location);
            if (a->marker_name[0])
            {
                if (object_definition_get(object->definition_index)->object.model.index==NONE ||
                    object_get_marker_by_name(it.index,a->marker_name,&marker,1)<=0) continue;
            }
            else object_get_world_matrix(it.index,&marker.matrix);
            s.origin=marker.matrix.position; s.axis=marker.matrix.forward;
            if (normalize3d(&s.axis)<=0.f) continue;
            /* Legacy omni fixture tags describe a short runtime gel radius.
               Bound the baked footprint around the authored attachment axis. */
            s.range=PIN(d->radius*4.f,.25f,12.f);
            s.angle=d->cutoff>0 && d->cutoff<1.5f?d->cutoff:1.05f;
            const float *rgb=fixture ? &d->high.red : d->baked_color.n;
            float strength=fixture ? 1.f : d->baked_intensity;
            if (fixture && fixture->light_fixture.intensity>0.f)
            {
                strength=fixture->light_fixture.intensity;
                rgb=fixture->light_fixture.color.n;
                if (fixture->light_fixture.cutoff_angle>.02f && fixture->light_fixture.cutoff_angle<1.5f)
                    s.angle=fixture->light_fixture.cutoff_angle;
            }
            for (int k=0;k<3;k++) s.color[k]=.45f*PIN(rgb[k]*strength,0.f,1.f);
            if (!(MAX(s.color[0],MAX(s.color[1],s.color[2]))>0.f)) continue;
            s.color[3]=s.range;
            admit_fixture(chosen,s);
        }
    }
}

static int fixture_casters(const struct spot_source *s,long families[MAXIMUM_OBJECTS_PER_MAP])
{
    struct object_iterator it;
    struct object_datum *object;
    int count=0;
    float cone_cos=cosf(s->angle),cone_sin=sinf(s->angle);
    object_iterator_new(&it,_object_mask_all,0);
    while ((object=object_iterator_next(&it))!=NULL)
    {
        int remaining=128;
        real_point3d center;
        real radius;
        if (object->object.parent_object_index!=NONE || TEST_FLAG(object->object.flags,_object_invisible_bit)) continue;
        if (object->object.first_child_object_index==NONE &&
            object_definition_get(object->definition_index)->object.model.index==NONE) continue;
        object_get_render_bounding_sphere(it.index,&center,&radius);
        render_interpolation_object_point(it.index,&center);
        if (object->object.first_child_object_index!=NONE &&
            !environmental_family_bounds(it.index,&center,&radius,&remaining,1)) return 0;
        if (distance_squared3d(&center,&s->origin)>(s->range+radius)*(s->range+radius)) continue;
        real_vector3d delta;
        vector_from_points3d(&s->origin,&center,&delta);
        float along=dot_product3d(&s->axis,&delta);
        float lateral=sqrtf(MAX(0.f,dot_product3d(&delta,&delta)-along*along));
        if (along < -radius || along > s->range+radius || lateral*cone_cos-along*cone_sin>radius) continue;
        families[count++]=it.index;
    }
    return count;
}

/* Resolve the submitted beam's owner per view. Other lights still see its wearer. */
static int flashlight_object_casters(real_point3d const *origin, real radius, long emitter)
{
    const long owners[2] = {NONE, NONE};
    struct object_datum *object = object_try_and_get(emitter);
    long wearer = emitter, held_emitter = NONE;
    if (object && object->object.type == _object_type_weapon)
    {
        wearer = object->object.parent_object_index;
        object = object_try_and_get(wearer);
        if (object && object->object.type == _object_type_biped) held_emitter = emitter;
    }
    if (!object || object->object.type != _object_type_biped) wearer = NONE;
    else if (held_emitter == NONE)
    {
        short slot = unit_get(wearer)->unit.current_weapon_index;
        if (slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT)
        {
            long weapon = unit_inventory_get_weapon(wearer, slot);
            object = object_try_and_get(weapon);
            if (object && object->object.parent_object_index == wearer)
                held_emitter = weapon;
        }
    }
    return object_casters(origin, radius, owners, NONE, wearer, held_emitter);
}

static void prepare_spots(int mode)
{
    int fixtures=config_boolean("display.environment_fixtures");
    int beam=config_boolean("display.flashlight_shadows");
    float frame[16];
    if ((!fixtures && !beam) || !game_engine_allow_dynamic_lighting() ||
        !rasterizer_debug_options.draw_environment_shadows) return;
    if (fixtures)
    {
        struct spot_source chosen[2]={0};
        find_fixtures(chosen);
        admit_patches(chosen);
        for (int i=0;i<2;i++)
        {
            struct spot_source *s=&chosen[i];
            if (!s->range) continue;
            long families[MAXIMUM_OBJECTS_PER_MAP];
            int count=fixture_casters(s,families);
            if (!count) continue;
            spot_frame(s,frame);
            int begin=modern_spot_begin(i+2,frame,1);
            if (!begin || (begin==1 && !modern_spot_end(world_casters(cached_bsp,&s->origin,s->range)))) continue;
            if (!modern_spot_begin(i,frame,0)) continue;
            int ok=1;
            for (int j=0;ok && j<count;j++)
            {
                int remaining=128;
                ok=environmental_casters(families[j],&remaining);
            }
            if (modern_spot_end(ok)) modern_spot_publish(i,frame,s->origin.n,s->color,mode==2);
        }
    }
    float nearest=REAL_MAX;
    long best=NONE;
    for (long i=0;beam && i<rasterizer_lights.light_count;i++)
    {
        const struct rasterizer_light_submit_parameters *light=&rasterizer_lights.lights[i];
        const struct spot_definition *d=(const void *)light->definition;
        if (!d || !(d->flags & spot_first_person_flashlight) || !(light->radius>.005f) ||
            MAX(light->color.red,MAX(light->color.green,light->color.blue))<=0.f) continue;
        float dist=distance_squared3d(&render.camera.position,&light->position);
        if (dist<nearest) { best=i; nearest=dist; }
    }
    if (best!=NONE)
    {
        const struct rasterizer_light_submit_parameters *light=&rasterizer_lights.lights[best];
        const struct spot_definition *d=(const void *)light->definition;
        struct spot_source s={0};
        s.origin=light->position; s.axis=light->forward; s.up=light->up; s.range=light->radius*MAX(1.f,d->specular_scale);
        s.angle=PIN(d->cutoff,.02f,1.55f); s.color[3]=s.range;
        if (normalize3d(&s.axis)>0.f)
        {
            spot_frame(&s,frame);
            long emitter = best < SHADOW_LIGHTS ? light_port_emitter_object(identities[best]) : NONE;
            if (modern_spot_begin(4,frame,0))
            {
                spot_clip_planes(frame);
                clipping_spot=1;
                int complete=world_casters(cached_bsp,&s.origin,s.range) &&
                    flashlight_object_casters(&s.origin,s.range,emitter);
                clipping_spot=0;
                if (modern_spot_end(complete))
                {
                    flashlight_index=best;
                    modern_spot_publish(2,frame,s.origin.n,s.color,mode==2);
                }
            }
        }
    }
    if (modern_spot_mask() & 3)
    {
        float camera[16];
        matrix_to_gl(&global_window_parameters.frustum.view_to_world,camera);
        modern_spot_camera(camera,(const float *)global_window_parameters.frustum.projection_matrix);
    }
}
int modern_spot_is_flashlight(long index) { return (modern_spot_mask() & 4) && index == flashlight_index; }
int modern_spot_flash_slot(int slot) { return modern_spot_is_flashlight(slots[slot]); }
int modern_spot_model_active(void) { return (modern_spot_mask()&3) || modern_spot_flash_slot(0) || modern_spot_flash_slot(1); }
void modern_spot_world_light(long index) { flash_world=modern_spot_is_flashlight(index); }
void modern_spots_project(void) { modern_spot_bsp(); }
