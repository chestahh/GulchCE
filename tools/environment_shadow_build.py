"""Build environmental-shadow capture controls, absent from ordinary builds."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    out=args.output.resolve()
    if not out.is_relative_to((ROOT/'build').resolve()) or out.exists():
        p.error('Use a fresh build/ output directory')
    paths=[ROOT/'port/linux/src/modern_environment_gl.inl',ROOT/'port/linux/game/modern_shadows.c', ROOT/'source/render/render_objects.c']
    original={f:f.read_bytes() for f in paths}
    try:
        f=paths[0]; s=f.read_text()
        s=s.replace('environment.count++;','environment.count++;\n    if (getenv("SHADOW_ENV_FALLBACK")) return 0;',1)
        s=s.replace('environment.unavailable = 1;', 'environment.unavailable = 1;\n        platform_log("ENV RESOURCE FAIL error %u",glGetError());',1)
        s=s.replace('return success;\n}', 'if(environment.count==1) { static int samples; if(!(samples++%30)) platform_log("ENV GPU complete %d draws %d size %d scene %dx%d viewport %d %d %d %d radius %.5f",success,draw_count,environment.size,environment.scene_size[0],environment.scene_size[1],environment.viewport[0],environment.viewport[1],environment.viewport[2],environment.viewport[3],environment.radius); }\n    return success;\n}',1)
        s='#include <stdio.h>\n'+s
        dump=r"""    if (success && getenv("SHADOW_ENV_DUMP") && color[0]<.7f)
    {
        static int seen, dumped;
        if (!(seen++ % 100) && dumped<3)
        {
            char name[64]; FILE *f; float *pixels;
            int header[7]={environment.size,environment.scene_size[0],environment.scene_size[1],environment.viewport[0],environment.viewport[1],environment.viewport[2],environment.viewport[3]};
            snprintf(name,sizeof(name),"environment-dump-%d.bin",dumped++); f=fopen(name,"wb");
            if(f){
                fwrite(header,sizeof(header),1,f);fwrite(environment.frame,sizeof(float),16,f);fwrite(camera,sizeof(float),16,f);fwrite(projection,sizeof(float),16,f);
                fwrite(&environment.radius,sizeof(float),1,f);fwrite(color,sizeof(float),3,f);
                fwrite(environment.pixel_center,sizeof(float),2,f);
                pixels=malloc(environment.scene_size[0]*environment.scene_size[1]);
                glBindFramebuffer(GL_FRAMEBUFFER,environment.destination);
                glReadPixels(0,0,environment.scene_size[0],environment.scene_size[1],GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,pixels);
                {FILE *st=fopen("environment-stencil.bin","wb"); if(st){fwrite(pixels,1,environment.scene_size[0]*environment.scene_size[1],st);fclose(st);}}free(pixels);
                pixels=malloc(environment.size*environment.size*sizeof(float));
                glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_2D,environment.depth);
                glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,pixels);fwrite(pixels,sizeof(float),environment.size*environment.size,f);free(pixels);
                pixels=malloc(environment.scene_size[0]*environment.scene_size[1]*sizeof(float));
                glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D,environment.scene_depth);
                glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,pixels);fwrite(pixels,sizeof(float),environment.scene_size[0]*environment.scene_size[1],f);free(pixels);fclose(f);
            }
        }
    }
"""
        anchor='    if (success)\n    {\n        p=sun.projecting ? sun.receiver : environment.receiver;'
        assert s.count(anchor)==1
        s=s.replace(anchor,dump+anchor,1)
        f.write_text(s)
        f=paths[2]; s=f.read_text()
        s=s.replace('\tif (render_shadows)\n', '\t{ static int calls; if(!(calls++%60)) platform_log("ENV PASS enabled %d count %d",render_shadows,render_object_globals.rendered_object_count); }\n\tif (render_shadows)\n',1)
        s=s.replace('\t\t\tshadow_darkness = 1.f - real_rgb_color_brightness(&data->lighting->shadow_color);','\t\t\tshadow_darkness = 1.f - real_rgb_color_brightness(&data->lighting->shadow_color);\n\t\t\t{ static int calls; if(!(calls++%60)) platform_log("ENV ELIGIBLE object %ld pixels %.3f darkness %.3f",data->object_index,level_of_detail_pixels,shadow_darkness); }',1)
        f.write_text(s)
        f=paths[1]; s=f.read_text()
        s=s.replace('    if (!mode || !cached_bsp || global_window_parameters', '    { static int calls; if(!(calls++%60)) platform_log("ENV GUARD mode %d bsp %p target %d debug %d radius %.4f", mode,(void *)cached_bsp,global_window_parameters.rasterizer_target,rasterizer_debug_options.draw_environment_shadows,radius); }\n    if (!mode || !cached_bsp || global_window_parameters',1)
        a='return modern_environment_end(complete, camera, (float const *)global_window_parameters.frustum.projection_matrix, color, handoff);'
        assert a in s
        s=s.replace(a,'{ int result=modern_environment_end(complete, camera, (float const *)global_window_parameters.frustum.projection_matrix, color, handoff);\n      static int samples; if(!(samples++%60)) platform_log("ENV OWNER object %ld modern %d color %.4f %.4f %.4f",object,result,color[0],color[1],color[2]);\n      return result; }',1)
        f.write_text(s)
        subprocess.run([sys.executable,'tools/plasma_visual_build.py','--output-dir',str(out)],cwd=ROOT,check=True)
        mf=out/'build-manifest.json';manifest=json.loads(mf.read_text());manifest['environment_probe']=True
        manifest['environment_source_hashes']={str(f.relative_to(ROOT)):hashlib.sha256(data).hexdigest() for f,data in original.items()}
        mf.write_text(json.dumps(manifest,indent=2))
    finally:
        for f,data in original.items(): f.write_bytes(data)
        subprocess.run(['ninja','windows'],cwd=ROOT,check=True)

if __name__=='__main__': main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
