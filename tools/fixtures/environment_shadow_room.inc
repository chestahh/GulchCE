
/* Copied into the instrumented desktop binary only. Keep the lamp camera
   independent of player input and prove each source using the same caster. */
#include "render/render_cameras.h"
int bench_spot_sources=7;
static int room_mode;
static real_point3d beam_origin;
static real_vector3d beam_axis;
void bench_spot_log(int slot,const float *p,const float *frame)
{
    static int seen;
    if (!room_mode || lighting_performance_seconds()<1.) return;
    if (slot==2 && room_mode==4)
    {
        memcpy(&beam_origin,p,sizeof(beam_origin));
        beam_axis=(real_vector3d){frame[3],frame[7],frame[11]};
    }
    if (seen&(1<<slot)) return;
    seen|=1<<slot;
    fprintf(stderr,"E3 slot %d at %.4f %.4f %.4f axis %.4f %.4f %.4f\n",slot,p[0],p[1],p[2],frame[3],frame[7],frame[11]);
}
void bench_fixture_room(struct render_window *window)
{
    static int initialized, compare, last=-1, staged_beam;
    static long vehicle=NONE;
    static long subject=NONE;
    static real_point3d pose={8.82f,-11.1f,-2.68f};
    real_vector3d forward={0,-1,0},up={0,0,1};
    if (!initialized)
    {
        const char *mode=getenv("HALO_BENCH_FIXTURE_ROOM");
        if (mode) room_mode=!strcmp(mode,"timing") ? 1 : !strcmp(mode,"panel") ? 3 : !strcmp(mode,"beam") ? 4 : 2;
        compare=room_mode>1;
        if (room_mode==3) pose=(real_point3d){-7.94f,-12.48f,-2.6f};
        initialized=1;
    }
    if (!room_mode || !origin) return;
    double elapsed=clock_seconds()-origin;
    int stage=MIN(6,(int)(elapsed/1.75));
    static const int masks[]={0,1,2,3,4,7,0};
    bench_spot_sources=compare?masks[stage]:7;
    if (room_mode==4) bench_spot_sources=(stage&1)?4:0;
    if (subject==NONE)
    {
        real_point3d from={pose.x,pose.y,-1.f};
        real_vector3d down={0,0,-5};
        struct collision_result floor;
        if (collision_test_vector(FLAG(_collision_test_structure_bit)|FLAG(_collision_test_front_facing_surfaces_bit),
            &from,&down,NONE,&floor)) pose.z=floor.point.z+.01f;
        long tag=tag_loaded('bipd',"characters\\cyborg_mp\\cyborg_mp");
        struct object_placement_data placement;
        if (tag!=NONE)
        {
            object_placement_data_new(&placement,tag,NONE);
            placement.position=pose; placement.forward=forward; placement.up=up;
            subject=object_new(&placement);
            if (room_mode==4 && subject!=NONE)
            {
                long rifle=tag_loaded('weap',"weapons\\assault rifle\\assault rifle");
                long hog=tag_loaded('vehi',"vehicles\\warthog\\warthog");
                if (rifle!=NONE)
                {
                    object_placement_data_new(&placement,rifle,NONE); placement.position=pose;
                    rifle=object_new(&placement);
                    if (rifle!=NONE) unit_add_weapon_to_inventory(subject,rifle,TRUE);
                }
                if (hog!=NONE)
                {
                    object_placement_data_new(&placement,hog,NONE);
                    placement.position=pose; placement.position.x+=1.4f;
                    vehicle=object_new(&placement);
                }
                fprintf(stderr,"E3 receiver vehicle %ld rifle %ld\n",vehicle,rifle);
            }
        }
        fprintf(stderr,"E3 two-lamp biped %ld at %.3f %.3f %.3f\n",subject,pose.x,pose.y,pose.z);
    }
    struct render_camera *c=&window->render_camera,*r=&window->rasterizer_camera;
    c->position=(real_point3d){8.1f,-15.05f,-1.90f};
    if (room_mode==3) c->position=(real_point3d){-6.65f,-13.45f,-1.82f};
    real_point3d target=pose; target.z+=.47f;
    vector_from_points3d(&c->position,&target,&c->forward); normalize3d(&c->forward);
    observer_up_from_forward(&c->forward,&c->up);
    r->position=c->position; r->forward=c->forward; r->up=c->up;
    if (compare && subject!=NONE && clock_seconds()-origin<1.)
    {
        long player=player_control_get_unit_index(0);
        if (player!=NONE)
        {
            real_point3d stand=pose; stand.y-=1.6f;
            real_vector3d aim={0,1,0};
            object_set_position(player,&stand,&aim,&up);
            unit_get(player)->unit.desired_aiming_vector=aim;
            unit_set_desired_flashlight_state(player,TRUE);
        }
    }
    if (room_mode==4 && subject!=NONE && vehicle!=NONE && beam_axis.j!=0 && elapsed>1.05)
    {
        real_point3d actor,hull;
        point_from_line3d(&beam_origin,&beam_axis,1.4f,&actor);
        point_from_line3d(&beam_origin,&beam_axis,2.9f,&hull);
        actor.z-=.38f; hull.z-=.23f;
        if (stage>=2 && stage<=3) actor.x+=.22f;
        if (stage>=4 && stage<=5) actor.x+=1.5f;
        if (staged_beam!=stage+1)
        {
            object_set_position(subject,&actor,&forward,&up);
            real_vector3d sideways={1,0,0};
            object_set_position(vehicle,&hull,&sideways,&up); staged_beam=stage+1;
        }
    }
    if (compare && subject!=NONE && elapsed>1.15 && elapsed<12.) game_time_set_speed(0.f);
    if (compare && elapsed>=12.) game_time_set_speed(1.f);
    if (compare && stage!=last) { fprintf(stderr,"E3 comparison %d mask %d\n",stage,bench_spot_sources); last=stage; }
}
