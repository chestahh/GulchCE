/* Buffered whole-frame timings for the real vehicle fixture, no readbacks. */
#include <stdio.h>
#include "modern_shadows.h"
static void turret_probe_frame(void)
{
    static FILE *file;
    static double origin, previous, flushed;
    LARGE_INTEGER counter, frequency;
    double now;
    long index, shadowed=0, turrets=0;
    if(!game_in_progress() || game_time_menu_scene()) return;
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    now=(double)counter.QuadPart/(double)frequency.QuadPart;
    if(!file)
    {
        origin=previous=flushed=now;
        file=fopen("turret-frames.csv","w");
        if(!file) return;
        setvbuf(file,NULL,_IOFBF,65536);
        fprintf(file,"seconds,frame_ms,tick,shadowed,turrets\n");
    }
    for(index=0;index<rasterizer_lights.light_count;index++)
        if(modern_shadows_selected(index))
        {
            shadowed++;
            turrets+=turret_timing_selected(index);
        }
    fprintf(file,"%.6f,%.6f,%ld,%ld,%ld\n",now-origin,(now-previous)*1000,capture_ticks,shadowed,turrets);
    if(now-flushed>=1) { fflush(file); flushed=now; }
    previous=now;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
