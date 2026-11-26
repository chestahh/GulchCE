"""Capture busy-combat shadow inputs with passive timing instrumentation.

Diagnostic executable only. Capture readbacks invalidate its performance
numbers; use the ordinary performance build for gameplay measurements.
"""
from pathlib import Path
import hashlib
import json
import argparse
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def change(text, before, after):
    if text.count(before)!=1:raise ValueError('Capture hook changed: '+before)
    return text.replace(before,after,1)


def main():
    paths=[ROOT/'port/linux/game/modern_shadows.c',ROOT/'port/linux/src/modern_shadows_gl.c']
    original={p:p.read_bytes() for p in paths}
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/shadow-comparison/busy-capture')
    out=parser.parse_args().output.resolve()
    if not out.is_relative_to((ROOT/'build').resolve()):parser.error('Output must be beneath build/')
    out.mkdir(parents=True,exist_ok=True)
    if (out/'halo-performance.exe').exists():raise ValueError('Refusing stale capture output')
    try:
        text=paths[0].read_text()
        text='void shadow_probe_camera(const float *,const float *);\n'+text
        text=change(text,'\tselect_lights();','\tshadow_probe_camera((const float *)&render.camera.position,(const float *)&render.camera.forward);\n\tselect_lights();')
        paths[0].write_text(text)
        text='#include <stdio.h>\n#include <stdlib.h>\n'+paths[1].read_text()
        text=change(text,'static int drawing;', '''static int drawing;
static float probe_camera[6];
static int probe_frame;
void shadow_probe_camera(const float *position,const float *forward)
{ ++probe_frame;memcpy(probe_camera,position,12);memcpy(probe_camera+3,forward,12); }
''')
        marker='\tsize = vertex_count * sizeof(*vertices) + index_count * sizeof(*indices);'
        text=change(text,marker,'''    { FILE *f=fopen("shadow-meshes.bin",mesh_count?"ab":"wb"); if(f) {
      fwrite(&mesh_count,4,1,f);fwrite(&vertex_count,4,1,f);fwrite(&index_count,4,1,f);
      fwrite(vertices,sizeof(*vertices),vertex_count,f);fwrite(indices,2,index_count,f);fclose(f); } }
'''+marker)
        marker='\t\tif (!alpha || target != GL_TEXTURE_2D) { failed = 1; return 0; }'
        text=change(text,marker,marker+'''
        { static GLuint seen[256];static int count;int i;
          for(i=0;i<count;i++)if(seen[i]==alpha)break;
          if(i==count && count<256 && description.width>0 && description.height>0 && description.width<=4096 && description.height<=4096) {
            int w=description.width,h=description.height;unsigned char *pixels=malloc(w*h*4);FILE *f;char name[80];
            if(pixels) { glBindTexture(GL_TEXTURE_2D,alpha);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
              snprintf(name,sizeof(name),"shadow-alpha-%u.bin",alpha);f=fopen(name,"wb");
              if(f){fwrite(&w,4,1,f);fwrite(&h,4,1,f);fwrite(pixels,4,w*h,f);fclose(f);}free(pixels);seen[count++]=alpha;
            }
          }
        }
''')
        text=change(text,'\tmap->view_map = 0;', '''    { int frame=probe_frame;
      if(frame==6000 || frame==12000 || frame==18000) {
        FILE *f;char name[80];int d;snprintf(name,sizeof(name),"shadow-draws-%d-%d.bin",frame,(int)(map-maps));f=fopen(name,"wb");
        if(f){fwrite(map->light,4,4,f);fwrite(&draw_count,4,1,f);
          for(d=0;d<draw_count;d++){fwrite(&draws[d].mesh,4,3,f);fwrite(draws[d].matrices,sizeof(draws[d].matrices),1,f);
            fwrite(draws[d].uv_transform,sizeof(draws[d].uv_transform),1,f);fwrite(&draws[d].alpha,4,1,f);}fclose(f);}
        snprintf(name,sizeof(name),"shadow-camera-%d-%d.bin",frame,(int)(map-maps));f=fopen(name,"wb");if(f){fwrite(probe_camera,4,6,f);fclose(f);}
      }
    }
\tmap->view_map = 0;''')
        paths[1].write_text(text)
        subprocess.run([sys.executable,'tools/lighting_performance_build.py','--output-dir',str(out)],check=True,cwd=ROOT)
        (out/'capture-inputs.json').write_text(json.dumps({str(p.relative_to(ROOT)):hashlib.sha256(b).hexdigest() for p,b in original.items()},indent=2))
    finally:
        for p,b in original.items():p.write_bytes(b)
        subprocess.run(['ninja','windows'],check=True,cwd=ROOT)


if __name__=='__main__':main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
