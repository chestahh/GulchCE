/* Environmental object shadows. Included beside the immutable Modern mesh
   cache so conversion/poses/materials have one owner. One 2D map is reused
   sequentially; no local-light cube or selection slot is borrowed. */
static struct
{
    GLuint depth, scene_depth, scene_fbo, caster, receiver;
    int size, scene_size[2], copied, count, unavailable, soft, marking;
    GLenum scene_format;
    GLint caster_frame, caster_radius, caster_alpha_map, caster_bones, caster_uv, caster_alpha;
    GLuint destination;
    int viewport[4];
    float frame[16], radius, pixel_center[2];
} environment;
static struct
{
    GLuint depth[2], framebuffer[2];
    int size, enabled, valid, count, soft, unavailable;
    float frames[32], radius;
} environment_model;

static GLuint environment_program(const char *vertex, const char *fragment)
{
    GLuint vs = xgpu_compile_shader(GL_VERTEX_SHADER, vertex, "environment shadow");
    GLuint fs = xgpu_compile_shader(GL_FRAGMENT_SHADER, fragment, "environment shadow");
    GLuint result = vs && fs ? xgpu_link_program(vs, fs, "environment shadow") : 0;
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return result;
}

void modern_environment_receivers(int enabled) { environment.marking = enabled; }
int modern_environment_marking(void) { return environment.marking; }

void modern_environment_window(void)
{
    environment.marking = environment.copied = environment.count = 0;
    environment_model.enabled = environment_model.valid = environment_model.count = 0;
}

void modern_environment_dispose(void)
{
    if (environment_model.framebuffer[0] || environment_model.framebuffer[1])
        glDeleteFramebuffers(2, environment_model.framebuffer);
    if (environment_drawing)
    {
        drawing = environment_drawing = 0;
        draw_count = 0;
        xgpu_gl_end_external_pass();
    }
    if (environment_model.depth[0] || environment_model.depth[1])
        glDeleteTextures(2, environment_model.depth);
    memset(&environment_model, 0, sizeof(environment_model));
    if (environment.depth) glDeleteTextures(1, &environment.depth);
    if (environment.scene_depth) glDeleteTextures(1, &environment.scene_depth);
    if (environment.scene_fbo) glDeleteFramebuffers(1, &environment.scene_fbo);
    if (environment.caster) glDeleteProgram(environment.caster);
    if (environment.receiver) glDeleteProgram(environment.receiver);
    memset(&environment, 0, sizeof(environment));
    environment_drawing = 0;
}

static int environment_resources(void)
{
    static const char caster_vertex[] =
        "#version 330 core\n"
        "layout(location=0) in vec3 position; layout(location=1) in vec2 nodes;\n"
        "layout(location=2) in float weight; layout(location=3) in vec2 uv;\n"
        "uniform mat4 bones[64], frame; uniform float radius; uniform vec4 uv_transform[2];\n"
        "out vec2 texcoord; void main(){\n"
        "vec3 p=mix(bones[int(nodes.y)]*vec4(position,1),bones[int(nodes.x)]*vec4(position,1),weight).xyz;\n"
        "vec3 q=transpose(mat3(frame))*(p-frame[3].xyz)/radius;\n"
        "gl_Position=vec4(q.xy,0.5-0.5*q.z,1);\n"
        "texcoord=vec2(dot(vec4(uv,0,1),uv_transform[0]),dot(vec4(uv,0,1),uv_transform[1]));}\n";
    static const char caster_fragment[] =
        "#version 330 core\n"
        "in vec2 texcoord; uniform sampler2D alpha_map; uniform int alpha_test;\n"
        "void main(){if(alpha_test!=0 && texture(alpha_map,texcoord).a<=127.0/255.0) discard;}\n";
    static const char receiver_vertex[] =
        "#version 330 core\n"
        "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); gl_Position=vec4(p*2-1,0,1);}\n";
    static const char receiver_fragment[] =
        "#version 330 core\n"
        "uniform sampler2D scene_depth, shadow_depth; uniform mat4 camera, projection, frame;\n"
        "uniform vec4 viewport; uniform vec4 color; uniform float radius; uniform int reversed_depth, soft;\n"
        "out vec4 result;\n"
        "float compare_at(vec2 uv,float z,vec2 gradient){\n"
        "vec2 size=vec2(textureSize(shadow_depth,0)); vec2 center=(floor(uv*size)+0.5)/size;\n"
        "if(any(lessThan(center,vec2(0)))||any(greaterThan(center,vec2(1)))) return 0;\n"
        "float stored=texture(shadow_depth,center).r;\n"
        "return stored>0 && stored>z+dot(center-uv,gradient)+0.0001?1.0:0.0;}\n"
        "void main(){\n"
        "float d=texelFetch(scene_depth,ivec2(gl_FragCoord.xy),0).r;\n"
        "if((reversed_depth!=0 && d<=0)||(reversed_depth==0 && d>=1)) discard;\n"
        "vec2 xy=(gl_FragCoord.xy-viewport.xy)/viewport.zw; xy=vec2(xy.x*2-1,1-xy.y*2);\n"
        "float z=-projection[3][2]/(reversed_depth!=0?(1+projection[2][2])-d:d+projection[2][2]);\n"
        "vec3 view=vec3((-xy.x-projection[2][0])*z/projection[0][0],(-xy.y-projection[2][1])*z/projection[1][1],z);\n"
        "vec3 world=(camera*vec4(view,1)).xyz; vec3 q=transpose(mat3(frame))*(world-frame[3].xyz)/radius;\n"
        "vec2 uv=q.xy*0.5+0.5; float depth=0.5-0.5*q.z;\n"
        "vec3 dx=dFdx(vec3(uv,depth)),dy=dFdy(vec3(uv,depth)); float det=dx.x*dy.y-dx.y*dy.x;\n"
        "vec2 gradient=abs(det)>1.e-12?vec2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/det:vec2(0);\n"
        "vec3 n=cross(dFdx(world),dFdy(world)); float nl=length(n);\n"
        "if(any(greaterThan(abs(q.xy),vec2(1)))||q.z< -1||q.z>4||nl<1.e-12) discard;\n"
        "float shadow=compare_at(uv,depth,gradient);\n"
        "if(soft!=0){vec2 size=vec2(textureSize(shadow_depth,0)), f=fract(uv*size-0.5), base=uv-f/size;\n"
        "shadow=mix(mix(compare_at(base,depth+dot(base-uv,gradient),gradient),compare_at(base+vec2(1,0)/size,depth+dot(base+vec2(1,0)/size-uv,gradient),gradient),f.x),\n"
        "mix(compare_at(base+vec2(0,1)/size,depth+dot(base+vec2(0,1)/size-uv,gradient),gradient),compare_at(base+1/size,depth+dot(base+1/size-uv,gradient),gradient),f.x),f.y);}\n"
        "float fade=clamp(1-q.z/4,0,1)*clamp(1+q.z*2,0,1);\n"
        "float facing=abs(dot(n/nl,frame[2].xyz));\n"
        "result=vec4(mix(vec3(1),color.xyz,shadow*fade*facing),1);}\n";
    int size = 128 * (int)halo_shadow_resolution_scale();
    if (environment.size != size)
    {
        if (environment.depth) glDeleteTextures(1, &environment.depth);
        environment.depth = 0;
        environment.size = size;
        environment.unavailable = 0;
    }
    if (environment.unavailable || !create_resources()) return 0;
    if (!environment.caster)
    {
        environment.caster = environment_program(caster_vertex, caster_fragment);
        if (environment.caster)
        {
            environment.caster_frame = glGetUniformLocation(environment.caster, "frame");
            environment.caster_radius = glGetUniformLocation(environment.caster, "radius");
            environment.caster_alpha_map = glGetUniformLocation(environment.caster, "alpha_map");
            environment.caster_bones = glGetUniformLocation(environment.caster, "bones");
            environment.caster_uv = glGetUniformLocation(environment.caster, "uv_transform");
            environment.caster_alpha = glGetUniformLocation(environment.caster, "alpha_test");
        }
    }
    if (!environment.receiver) environment.receiver = environment_program(receiver_vertex, receiver_fragment);
    if (!environment.caster || !environment.receiver) return 0;
    if (!environment.depth)
    {
        glGenTextures(1, &environment.depth);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, environment.depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, environment.depth, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR;
}

static int environment_snapshot(void)
{
    int size[2];
    GLenum format;
    if (!xgpu_gl_scene_target(&environment.destination, size, &format, environment.viewport, environment.pixel_center)) return 0;
    if (size[0] != environment.scene_size[0] || size[1] != environment.scene_size[1] || format != environment.scene_format)
    {
        if (environment.scene_depth) glDeleteTextures(1, &environment.scene_depth);
        environment.scene_depth = 0;
        environment.copied = 0;
        memcpy(environment.scene_size, size, sizeof(size));
        environment.scene_format = format;
    }
    if (environment.copied) return 1;
    if (!environment.scene_fbo) glGenFramebuffers(1, &environment.scene_fbo);
    if (!environment.scene_depth)
    {
        glGenTextures(1, &environment.scene_depth);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, environment.scene_depth);
        glTexImage2D(GL_TEXTURE_2D, 0, format, size[0], size[1], 0, GL_DEPTH_STENCIL,
            format == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, environment.scene_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, environment.scene_depth, 0);
    { GLenum none = GL_NONE; glDrawBuffers(1, &none); }
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, environment.destination);
    glBlitFramebuffer(0,0,size[0],size[1],0,0,size[0],size[1],GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    environment.copied = glGetError() == GL_NO_ERROR;
    return environment.copied;
}

int modern_environment_begin(const float frame[16], float radius, int soft)
{
    /* A frame has a separate bounded environmental workload. Exhaustion keeps
       the stock projection; neither selection nor resources consume local slots. */
    if (drawing || !xgpu_gl_ready() || environment.count >= 32) return 0;
    environment.count++;
    if (!environment_snapshot() || !environment_resources())
    {
        environment.unavailable = 1;
        xgpu_gl_end_external_pass();
        return 0;
    }
    memcpy(environment.frame, frame, sizeof(environment.frame));
    environment.radius = radius;
    environment.soft = soft;
    drawing = environment_drawing = 1;
    draw_count = failed = 0;
    return 1;
}

/* Conservative projection of the bounded receiver volume. If it crosses
   the eye plane, keep the complete viewport; a smaller rectangle is unsafe. */
static void environment_scissor(const float camera[16], const float projection[16], int rectangle[4])
{
    float low[2] = {1,1}, high[2] = {0,0};
    int corner, axis, j;
    memcpy(rectangle, environment.viewport, sizeof(environment.viewport));
    for (corner = 0; corner < 8; corner++)
    {
        float q[3] = {corner & 1 ? 1.f : -1.f, corner & 2 ? 1.f : -1.f, corner & 4 ? 4.f : -1.f};
        float world[3], view[3] = {0,0,0}, screen[2];
        for (axis = 0; axis < 3; axis++)
        {
            world[axis] = environment.frame[12 + axis] - camera[12 + axis];
            for (j = 0; j < 3; j++) world[axis] += environment.frame[j * 4 + axis] * q[j] * environment.radius;
        }
        for (axis = 0; axis < 3; axis++)
            for (j = 0; j < 3; j++) view[axis] += camera[axis * 4 + j] * world[j];
        if (!(view[2] < -0.001f)) return;
        screen[0] = ((projection[0] * view[0] + projection[8] * view[2]) / -view[2] + 1.f) * .5f;
        screen[1] = (1.f - (projection[5] * view[1] + projection[9] * view[2]) / -view[2]) * .5f;
        for (axis = 0; axis < 2; axis++)
        {
            if (screen[axis] < low[axis]) low[axis] = screen[axis];
            if (screen[axis] > high[axis]) high[axis] = screen[axis];
        }
    }
    for (axis = 0; axis < 2; axis++)
    {
        int extent = environment.viewport[axis + 2];
        float margin = 2.f + environment.pixel_center[axis];
        float a = low[axis] * extent - margin, b = high[axis] * extent + margin;
        int first = a <= 0 ? 0 : a >= extent ? extent : (int)a;
        int last = b <= 0 ? 0 : b >= extent ? extent : (int)b + 1;
        rectangle[axis] += first;
        rectangle[axis + 2] = last > first ? last - first : 0;
    }
}

static int environment_draw_depth(GLuint target)
{
    GLuint p = environment.caster;
    int draw, success = !failed && draw_count > 0;
    if (success)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glViewport(0,0,environment.size,environment.size);
        glClipControl(GL_LOWER_LEFT,GL_ZERO_TO_ONE);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE); glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_GREATER); glDepthMask(GL_TRUE);
        glDepthRange(0,1); glClearDepth(0); glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(p); glBindVertexArray(vao);
        glUniformMatrix4fv(environment.caster_frame,1,GL_FALSE,environment.frame);
        glUniform1f(environment.caster_radius,environment.radius);
        glUniform1i(environment.caster_alpha_map,9);
        glActiveTexture(GL_TEXTURE9); glBindSampler(9,alpha_sampler);
        glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2); glEnableVertexAttribArray(3);
        for (draw=0;draw<draw_count;draw++)
        {
            int mesh=draws[draw].mesh;
            glBindBuffer(GL_ARRAY_BUFFER,meshes[mesh].vertices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,meshes[mesh].indices);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(struct modern_shadow_vertex),(void *)offsetof(struct modern_shadow_vertex,position));
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(struct modern_shadow_vertex),(void *)offsetof(struct modern_shadow_vertex,nodes));
            glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(struct modern_shadow_vertex),(void *)offsetof(struct modern_shadow_vertex,weight));
            glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(struct modern_shadow_vertex),(void *)offsetof(struct modern_shadow_vertex,uv));
            glUniformMatrix4fv(environment.caster_bones,draws[draw].nodes,GL_FALSE,draws[draw].matrices);
            glUniform4fv(environment.caster_uv,2,draws[draw].uv_transform);
            glUniform1i(environment.caster_alpha,draws[draw].alpha!=0);
            glBindTexture(GL_TEXTURE_2D,draws[draw].alpha);
            glDrawElements(GL_TRIANGLES,meshes[mesh].count,GL_UNSIGNED_SHORT,NULL);
        }
        /* Storage and programs are checked when created. Like the local-light
           depth pass, keep draws asynchronous; debug.gl_debug reports command
           errors through the desktop callback without a query after each map. */
    }
    return success;
}

int modern_environment_end(int complete, const float camera[16], const float projection[16], const float color[3])
{
    GLuint p;
    float viewport[4], rgba[4] = {color[0],color[1],color[2],1};
    int scissor[4];
    int draw, success = complete && !failed && draw_count > 0;
    if (!environment_drawing) return 0;
    if (success) success = environment_draw_depth(framebuffer);
    if (success)
    {
        p=environment.receiver;
        glBindFramebuffer(GL_FRAMEBUFFER,environment.destination);
        glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
        glViewport(environment.viewport[0],environment.viewport[1],environment.viewport[2],environment.viewport[3]);
        environment_scissor(camera, projection, scissor);
        glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
        glEnable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        /* Preserve the stock environmental receiver boundary. Model base passes
           already contain local illumination; multiplying them here would
           shadow unrelated light contributions. */
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_EQUAL,128,129);
        glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glStencilMask(0);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ZERO,GL_SRC_COLOR);
        glUseProgram(p);
        glBindTextures(8,1,&environment.depth); glBindSampler(8,0);
        glBindTextures(10,1,&environment.scene_depth); glBindSampler(10,0);
        glUniform1i(glGetUniformLocation(p,"shadow_depth"),8); glUniform1i(glGetUniformLocation(p,"scene_depth"),10);
        glUniform1i(glGetUniformLocation(p,"soft"),environment.soft);
        glUniform1i(glGetUniformLocation(p,"reversed_depth"),xgpu_reversed_depth);
        for (draw=0;draw<4;draw++) viewport[draw]=(float)environment.viewport[draw];
        viewport[0] += environment.pixel_center[0]; viewport[1] += environment.pixel_center[1];
        glUniform4fv(glGetUniformLocation(p,"viewport"),1,viewport);
        glUniformMatrix4fv(glGetUniformLocation(p,"frame"),1,GL_FALSE,environment.frame);
        glUniformMatrix4fv(glGetUniformLocation(p,"camera"),1,GL_FALSE,camera);
        glUniformMatrix4fv(glGetUniformLocation(p,"projection"),1,GL_FALSE,projection);
        glUniform1f(glGetUniformLocation(p,"radius"),environment.radius);
        glUniform4fv(glGetUniformLocation(p,"color"),1,rgba);
        success = glGetError() == GL_NO_ERROR;
        if (success)
        {
            glDrawArrays(GL_TRIANGLES,0,3);
            success = glGetError() == GL_NO_ERROR;
        }
    }
    glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
    xgpu_gl_end_external_pass();
    drawing=environment_drawing=0;
    return success;
}

/* Reuse two maps for the current object family. These do not consume any
   point-light slot. The caller retains them across consecutive child draws. */
void modern_environment_model_clear(void) { environment_model.enabled = 0; }
int modern_environment_model_active(void) { return environment_model.enabled != 0; }
int modern_environment_model_reuse(void) { environment_model.enabled = environment_model.valid; return environment_model.enabled != 0; }

int modern_environment_model_begin(const float frames[32], float radius, int soft)
{
    int i;
    environment_model.enabled = environment_model.valid = 0;
    if (drawing || !xgpu_gl_ready() || environment_model.count >= 32) return 0;
    environment_model.count++;
    if ((!environment.caster || !program || environment.size != 128 * (int)halo_shadow_resolution_scale()) &&
        !environment_resources()) { xgpu_gl_end_external_pass(); return 0; }
    if (environment_model.size != environment.size)
    {
        glDeleteTextures(2, environment_model.depth);
        glDeleteFramebuffers(2, environment_model.framebuffer);
        memset(environment_model.depth, 0, sizeof(environment_model.depth));
        memset(environment_model.framebuffer, 0, sizeof(environment_model.framebuffer));
        environment_model.size = environment.size;
        environment_model.unavailable = 0;
    }
    if (environment_model.unavailable) return 0;
    for (i = 0; i < 2; i++) if (!environment_model.depth[i])
    {
        glGenTextures(1, &environment_model.depth[i]);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, environment_model.depth[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, environment.size, environment.size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &environment_model.framebuffer[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, environment_model.framebuffer[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, environment_model.depth[i], 0);
        { GLenum none = GL_NONE; glDrawBuffers(1, &none); }
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
        {
            glDeleteTextures(2, environment_model.depth);
            glDeleteFramebuffers(2, environment_model.framebuffer);
            memset(environment_model.depth, 0, sizeof(environment_model.depth));
            memset(environment_model.framebuffer, 0, sizeof(environment_model.framebuffer));
            environment_model.unavailable = 1;
            xgpu_gl_end_external_pass();
            return 0;
        }
    }
    memcpy(environment_model.frames, frames, sizeof(environment_model.frames));
    environment_model.radius = radius;
    environment_model.soft = soft;
    drawing = environment_drawing = 1;
    draw_count = failed = 0;
    return 1;
}

void modern_environment_model_end(int complete, int mask)
{
    int i, success = complete && !failed && draw_count > 0;
    if (!environment_drawing) return;
    for (i = 0; success && i < 2; i++) if (mask & (1 << i))
    {
        memcpy(environment.frame, environment_model.frames + i * 16, sizeof(environment.frame));
        environment.radius = environment_model.radius;
        success = environment_draw_depth(environment_model.framebuffer[i]);
    }
    environment_model.enabled = environment_model.valid = success ? mask : 0;
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    xgpu_gl_end_external_pass();
    drawing = environment_drawing = 0;
}
