/* Three independently projected sources. Static and moving fixture depths are
   kept separate so existing occlusion in the bake is never applied twice. */
static const char spot_shader[] =
"uniform sampler2DArray spot_maps; uniform mat4 spot_frames[3],spot_camera,spot_projection;\n"
"uniform vec4 spot_colors[3],spot_origins[3],spot_viewport; uniform int spot_mask,spot_soft,spot_flash_slots,spot_reversed;\n"
"vec3 spot_world(float d){ vec2 xy=(gl_FragCoord.xy-spot_viewport.xy)/spot_viewport.zw*2.-1.;xy.y=-xy.y;\n"
"float z=-spot_projection[3][2]/(spot_reversed!=0?1.+spot_projection[2][2]-d:d+spot_projection[2][2]);\n"
"return (spot_camera*vec4((-xy-spot_projection[2].xy)*z/vec2(spot_projection[0][0],spot_projection[1][1]),z,1)).xyz;}\n"
"float spot_blocked(int i,vec3 p,out float weight){vec4 clip=spot_frames[i]*vec4(p,1);\n"
"vec3 q=clip.xyz/max(clip.w,.00001);vec2 uv=q.xy*.5+.5;vec3 dx=dFdx(vec3(uv,q.z)),dy=dFdy(vec3(uv,q.z));\n"
"float det=dx.x*dy.y-dy.x*dx.y; vec2 grad=abs(det)>1.e-14?vec2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/det:vec2(0);\n"
"weight=0.;if(clip.w<=0.||q.z<=0.||q.z>=1.||dot(q.xy,q.xy)>=1.)return 0.;\n"
"weight=(1.-smoothstep(.3,1.,length(q.xy)))*(1.-smoothstep(.25,1.,distance(p,spot_origins[i].xyz)/spot_colors[i].w));\n"
"ivec3 size=textureSize(spot_maps,0);vec2 pos=uv*vec2(size.xy);ivec2 base=ivec2(floor(pos-float(spot_soft)*.5));vec2 f=fract(pos-.5);float result=0.;\n"
"for(int t=0;t<=3*spot_soft;t++){ivec2 off=ivec2(t&1,t>>1),cell=clamp(base+off,ivec2(0),size.xy-1);\n"
"float z=q.z+dot((vec2(cell)+.5)/vec2(size.xy)-uv,grad)+max(.000001,.0005*q.z);\n"
"int layer=i==2?size.z-1:i;float blocker=texelFetch(spot_maps,ivec3(cell,layer),0).r;\n"
"float v=blocker>z?1.:0.;if(i<2 && texelFetch(spot_maps,ivec3(cell,i+2),0).r>z)v=0.;\n"
"vec2 a=mix(1.-f,f,vec2(off));result+=v*(spot_soft!=0?a.x*a.y:1.); }return result;}\n"
"vec3 fixture_visibility(vec3 p,vec3 n){vec3 removed=vec3(0);for(int i=0;i<2;i++){if((spot_mask&(1<<i))==0)continue;\n"
"float weight;float blocked=spot_blocked(i,p,weight);vec3 l=spot_origins[i].xyz-p;float facing=max(0.,dot(n,l)*inversesqrt(max(dot(l,l),1.e-12)));removed+=spot_colors[i].rgb*(weight*blocked*facing);}\n"
"return sqrt(max(vec3(.30),1.-removed));}\n"
"float beam_visibility(vec3 p){float w;return 1.-spot_blocked(2,p,w);}\n";

void modern_spot_emit(struct xgpu_text *text) { xgpu_text_append(text,"%s",spot_shader); }
int modern_spot_mask(void) { return spots.mask; }

static void spot_depth_resources(int size, int layers)
{
    if (!spots.depth || spots.size != size || spots.layers != layers)
    {
        if (spots.depth) glDeleteTextures(1,&spots.depth);
        spots.size=size; spots.layers=layers; spots.valid_static=spots.checked_layers=0;
        glGenTextures(1,&spots.depth);
        glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D_ARRAY,spots.depth);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,size,size,layers,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,NULL);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    if (!spots.fbo) glGenFramebuffers(1,&spots.fbo);
}

int modern_spot_begin(int layer, const float frame[16], int cacheable)
{
    int size = modern_shadow_pixels();
    int fixture = layer - 2;
    /* A flashlight-only scene needs one layer. Grow on first fixture use. */
    int layers = layer == 4 && spots.layers <= 1 ? 1 : 5;
    if (drawing || !xgpu_gl_ready()) return 0;
    if (cacheable && spots.size == size && fixture >= 0 && fixture < 2 &&
        (spots.valid_static & (1 << fixture)) && !memcmp(frame,spots.static_frames[fixture],64)) return 2;
    if (!environment_resources_for(1)) goto failed_begin;
    spot_depth_resources(size, layers);
    glBindFramebuffer(GL_FRAMEBUFFER,spots.fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,spots.depth,0,layers==1?0:layer);
    if (!(spots.checked_layers & (1<<layer)))
    {
        GLenum none=GL_NONE;
        glDrawBuffers(1,&none); glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR)
        {
            adaptive_reject_size();
            goto failed_begin;
        }
        spots.checked_layers |= 1<<layer;
    }
    memcpy(environment.frame,frame,sizeof(environment.frame));
    environment.radius=-1.f;
    spots.layer=layer;
    drawing=environment_drawing=spots.rendering=1;
    draw_count=failed=0;
    return 1;
failed_begin:
    xgpu_gl_end_external_pass();
    return 0;
}

int modern_spot_end(int complete)
{
    int ok=complete && environment_draw_depth(spots.fbo);
    if (spots.layer==2 || spots.layer==3)
    {
        int slot=spots.layer-2, opaque=ok;
        for (int i=0;opaque && i<draw_count;i++) if (draws[i].alpha) opaque=0;
        spots.valid_static &= ~(1<<slot);
        if (opaque)
        {
            spots.valid_static |= 1<<slot;
            memcpy(spots.static_frames[slot],environment.frame,sizeof(environment.frame));
        }
    }
    drawing=environment_drawing=spots.rendering=0;
    draw_count=0;
    glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
    xgpu_gl_end_external_pass();
    return ok;
}

void modern_spot_publish(int slot, const float frame[16], const float origin[3], const float color[4], int soft)
{
    memcpy(spots.frames[slot],frame,sizeof(spots.frames[slot]));
    memcpy(spots.origins[slot],origin,3*sizeof(float));
    memcpy(spots.colors[slot],color,sizeof(spots.colors[slot]));
    spots.mask |= 1<<slot;
    spots.soft=soft;
}

void modern_spot_camera(const float camera[16],const float projection[16])
{
    int size[2]; GLuint fbo,depth; GLenum format;
    if (!xgpu_gl_scene_target(&fbo,size,&format,&depth,environment.viewport,environment.pixel_center))
        spots.mask=0;
    memcpy(spots.camera,camera,sizeof(spots.camera));
    memcpy(spots.projection,projection,sizeof(spots.projection));
    xgpu_gl_end_external_pass();
}

static void spot_locations(GLuint program, struct modern_shadow_uniforms *u)
{
    u->spot_window=0;
    u->last_flash_slots=-1;
    u->spot_mask=glGetUniformLocation(program,"spot_mask");
    u->spot_frames=glGetUniformLocation(program,"spot_frames");
    u->spot_origins=glGetUniformLocation(program,"spot_origins");
    u->spot_colors=glGetUniformLocation(program,"spot_colors");
    u->spot_camera=glGetUniformLocation(program,"spot_camera");
    u->spot_projection=glGetUniformLocation(program,"spot_projection");
    u->spot_viewport=glGetUniformLocation(program,"spot_viewport");
    u->spot_soft=glGetUniformLocation(program,"spot_soft");
    u->spot_reversed=glGetUniformLocation(program,"spot_reversed");
    u->spot_flash_slots=glGetUniformLocation(program,"spot_flash_slots");
    glUniform1i(glGetUniformLocation(program,"spot_maps"),13);
}

static void spot_bind(struct modern_shadow_uniforms *u)
{
    if (u->spot_frames < 0) return;
    /* The two stock point-light bindings can change between models.
       Projector transforms, viewport and maps belong to this render window. */
    if (u->spot_flash_slots>=0)
    {
        int mask=modern_spot_flash_slot(0) | (modern_spot_flash_slot(1)<<1);
        if (u->last_flash_slots!=mask) glUniform1i(u->spot_flash_slots,mask);
        u->last_flash_slots=mask;
    }
    glBindTextures(13,1,&spots.depth); glBindSampler(13,0);
    if (u->spot_window==spot_window) return;
    u->spot_window=spot_window;
    float v[4];
    for (int i=0;i<4;i++) v[i]=(float)environment.viewport[i];
    v[0]+=environment.pixel_center[0]; v[1]+=environment.pixel_center[1];
    glUniformMatrix4fv(u->spot_frames,3,GL_FALSE,&spots.frames[0][0]);
    glUniform4fv(u->spot_origins,3,&spots.origins[0][0]);
    glUniform4fv(u->spot_colors,3,&spots.colors[0][0]);
    glUniformMatrix4fv(u->spot_camera,1,GL_FALSE,spots.camera);
    glUniformMatrix4fv(u->spot_projection,1,GL_FALSE,spots.projection);
    glUniform4fv(u->spot_viewport,1,v);
    glUniform1i(u->spot_mask,spots.mask);
    glUniform1i(u->spot_reversed,xgpu_reversed_depth);
    glUniform1i(u->spot_soft,spots.soft);
}

static struct modern_shadow_uniforms spot_receiver_uniforms;
static int spot_receiver_prepare(void)
{
    if (!spots.receiver)
    {
        char frag[sizeof(spot_shader)+512];
        strcpy(frag,"#version 330 core\n"); strcat(frag,spot_shader);
        strcat(frag,"uniform sampler2D scene;out vec4 color;void main(){float d=texelFetch(scene,ivec2(gl_FragCoord.xy),0).r;"
            "vec3 p=spot_world(d),n=cross(dFdx(p),dFdy(p));n*=inversesqrt(max(dot(n,n),1.e-24));if(dot(n,spot_camera[3].xyz-p)<0.)n=-n;color=vec4(fixture_visibility(p,n),1);}");
        spots.receiver=environment_program("#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}",frag);
        if (!spots.receiver) return 0;
        glUseProgram(spots.receiver); spot_locations(spots.receiver,&spot_receiver_uniforms);
        glUniform1i(glGetUniformLocation(spots.receiver,"scene"),10);
    }
    return 1;
}

void modern_spot_bsp(void)
{
    if (!(spots.mask&3) || !spot_receiver_prepare()) return;
    if (environment_snapshot())
    {
        glBindFramebuffer(GL_FRAMEBUFFER,environment.destination); glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
        glViewport(environment.viewport[0],environment.viewport[1],environment.viewport[2],environment.viewport[3]);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDepthMask(GL_FALSE);
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_EQUAL,128,129); glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ZERO,GL_SRC_COLOR);
        glUseProgram(spots.receiver); glBindTextures(10,1,&environment.scene_depth); glBindSampler(10,0);
        spot_bind(&spot_receiver_uniforms); glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,3);
    }
    xgpu_gl_end_external_pass();
}
