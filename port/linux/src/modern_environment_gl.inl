/* Environmental object shadows. Included beside the immutable Modern mesh
   cache so conversion/poses/materials have one owner. One 2D map is reused
   sequentially; no local-light cube or selection slot is borrowed. */
struct environment_receiver_uniforms
{
    GLint frame, camera, projection, radius, reversed_depth, viewport, soft, color;
    GLint clip_offset, sun_frame, sun_color_radius, sun_soft;
};

static struct
{
    GLuint depth, depth_fbo, scene_depth, scene_fbo, caster, receiver;
    int size, scene_size[2], copied, count, failed_size, soft, marking;
    GLenum scene_format;
    GLint caster_frame, caster_radius, caster_alpha_map, caster_bones, caster_uv, caster_alpha;
    struct environment_receiver_uniforms uniforms;
    GLuint destination, verified_copy_source;
    int viewport[4];
    float frame[16], radius, pixel_center[2];
} environment;
static struct
{
    GLuint depth[2], framebuffer[2];
    int size, enabled, valid, count, soft, unavailable;
    float frames[32], radius;
} environment_model;

/* Both receiver passes sample the same BSP and posed geometry. Keep these
   maps alive across model draws that reuse the environmental scratch depth. */
static struct
{
    GLuint static_depth, object_depth, object_fbo, receiver, static_caster, checked_receiver_target;
    const unsigned char *surface_sky[SHADOW_DRAWS];
    struct environment_receiver_uniforms uniforms;
    float tint[3];
    int ready, model, soft, partial;
    int writing, sky;
    int size, valid, projecting, object_checked;
    float frame[16], radius;
} sun;

enum { FIXTURE_LIMIT = 2, FLASH_SLOT = 2, SPOT_SOURCES = 3, SPOT_LAYERS = 5 };
static struct
{
    GLuint depth, fbo, receiver;
    int size, layers, layer, rendering, valid_static, checked_layers, mask, soft;
    float origins[SPOT_SOURCES][4], frames[SPOT_SOURCES][16], colors[SPOT_SOURCES][4], static_frames[FIXTURE_LIMIT][16];
    float camera[16], projection[16];
} spots;
static unsigned long long spot_window;

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
    sun.ready = sun.model = 0;
    spots.mask = 0;
    spot_window++;
    environment.marking = environment.copied = environment.count = 0;
    environment_model.enabled = environment_model.valid = environment_model.count = 0;
}

void modern_environment_dispose(void)
{
    if (spots.depth) glDeleteTextures(1, &spots.depth);
    if (spots.fbo) glDeleteFramebuffers(1, &spots.fbo);
    if (spots.receiver) glDeleteProgram(spots.receiver);
    memset(&spots, 0, sizeof(spots));
    if (sun.object_fbo) glDeleteFramebuffers(1,&sun.object_fbo);
    if (sun.object_depth) glDeleteTextures(1,&sun.object_depth);
    if (sun.static_depth) glDeleteTextures(1, &sun.static_depth);
    if (sun.receiver) glDeleteProgram(sun.receiver);
    if (sun.static_caster) glDeleteProgram(sun.static_caster);
    memset(&sun, 0, sizeof(sun));
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
    if (environment.depth_fbo) glDeleteFramebuffers(1, &environment.depth_fbo);
    if (environment.depth) glDeleteTextures(1, &environment.depth);
    if (environment.scene_depth) glDeleteTextures(1, &environment.scene_depth);
    if (environment.scene_fbo) glDeleteFramebuffers(1, &environment.scene_fbo);
    if (environment.caster) glDeleteProgram(environment.caster);
    if (environment.receiver) glDeleteProgram(environment.receiver);
    memset(&environment, 0, sizeof(environment));
    environment_drawing = 0;
}

/* Validate the replacement attachment before releasing the usable texture. */
static int environment_resize_texture(GLuint *current, GLuint *target, int size, int static_visibility)
{
    GLuint next = 0;
    GLenum attachment = static_visibility ? GL_COLOR_ATTACHMENT0 : GL_DEPTH_ATTACHMENT;
    GLenum output = static_visibility ? GL_COLOR_ATTACHMENT0 : GL_NONE;
    glGenTextures(1, &next);
    glActiveTexture(static_visibility ? GL_TEXTURE9 : GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, next);
    glTexImage2D(GL_TEXTURE_2D, 0, static_visibility ? GL_R32F : GL_DEPTH_COMPONENT32F,
        size, size, 0, static_visibility ? GL_RED : GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (!*target) glGenFramebuffers(1,target);
    glBindFramebuffer(GL_FRAMEBUFFER,*target);
    glReadBuffer(GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, next, 0);
    glDrawBuffers(1, &output);
    int complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    int error = glGetError() != GL_NO_ERROR;
    if (static_visibility || !complete || error)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, static_visibility ? 0 : *current, 0);
        output = GL_NONE;
        glDrawBuffers(1, &output);
    }
    if (!complete || error)
    {
        glDeleteTextures(1, &next);
        return 0;
    }
    if (*current) glDeleteTextures(1, current);
    *current = next;
    return 1;
}

#include "modern_sun_shader.inl"

/* Uniform locations belong to the linked program. Set constant samplers and
   the fixed unit-tile bounds once for its lifetime. */
static void environment_receiver_locations(GLuint program, struct environment_receiver_uniforms *u, int sunlight)
{
    glUseProgram(program);
    u->frame = glGetUniformLocation(program, "frame");
    u->camera = glGetUniformLocation(program, "camera");
    u->projection = glGetUniformLocation(program, "projection");
    u->radius = glGetUniformLocation(program, "radius");
    u->reversed_depth = glGetUniformLocation(program, "reversed_depth");
    u->viewport = glGetUniformLocation(program, "viewport");
    glUniform1i(glGetUniformLocation(program, "scene_depth"), 10);
    if (sunlight)
    {
        glUniform1i(glGetUniformLocation(program, "sun_objects"), 8);
        glUniform1i(glGetUniformLocation(program, "sun_bsp"), 9);
        glUniform4fv(glGetUniformLocation(program, "reach_min"), 1, (const float[4]){-1,-1,-1,0});
        glUniform4fv(glGetUniformLocation(program, "reach_max"), 1, (const float[4]){1,1,1,0});
        u->clip_offset = glGetUniformLocation(program, "clip_offset");
        u->sun_frame = glGetUniformLocation(program, "sun_frame");
        u->sun_color_radius = glGetUniformLocation(program, "sun_color_radius");
        u->sun_soft = glGetUniformLocation(program, "sun_soft");
    }
    else
    {
        glUniform1i(glGetUniformLocation(program, "shadow_depth"), 8);
        u->soft = glGetUniformLocation(program, "soft");
        u->color = glGetUniformLocation(program, "color");
    }
}

static int environment_resources_for(int projector)
{
    static const char caster_vertex[] =
        "#version 330 core\n"
        "layout(location=0) in vec3 position; layout(location=1) in vec2 nodes;\n"
        "layout(location=2) in float weight; layout(location=3) in vec2 uv;\n"
        "uniform mat4 bones[64], frame; uniform float radius; uniform vec4 uv_transform[2];\n"
        "out vec2 texcoord; void main(){\n"
        "vec3 p=mix(bones[int(nodes.y)]*vec4(position,1),bones[int(nodes.x)]*vec4(position,1),weight).xyz;\n"
        "vec3 q=transpose(mat3(frame))*(p-frame[3].xyz)/radius;\n"
        "gl_Position=radius<0.0?frame*vec4(p,1):vec4(q.xy,0.5-0.5*q.z,1);\n"
        "texcoord=vec2(dot(vec4(uv,0,1),uv_transform[0]),dot(vec4(uv,0,1),uv_transform[1]));}\n";
    static const char caster_fragment[] =
        "#version 330 core\n"
        "in vec2 texcoord; uniform sampler2D alpha_map; uniform int alpha_test;\n"
        "void main(){if(alpha_test!=0 && texture(alpha_map,texcoord).a<=127.0/255.0) discard;}\n";
    static const char receiver_vertex[] =
        "#version 330 core\n"
        "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); gl_Position=vec4(p*2-1,0,1);}\n";
    /* Rasterize the exit faces of the swept caster bounds. A visible receiver
       must lie between the camera and an exit face. This also holds when the
       camera is inside the volume, and each pixel is covered at most once. */
    static const char sun_vertex[] =
        "#version 330 core\n"
        "uniform mat4 frame,camera,projection; uniform vec4 reach_min,reach_max;\n"
        "uniform vec2 clip_offset; uniform float radius; uniform int reversed_depth;\n"
        "const int corners[6]=int[6](0,1,2,2,1,3);\n"
        "void main(){int face=gl_VertexID/6, axis=face/2, upper=face%2;\n"
        "vec3 eye=transpose(mat3(frame))*(camera[3].xyz-frame[3].xyz)/radius;\n"
        "if((upper==0 && eye[axis]<reach_min[axis]) || (upper==1 && eye[axis]>reach_max[axis]))"
        "{gl_Position=vec4(2,2,0,1);return;}\n"
        "int corner=corners[gl_VertexID%6]; vec3 t=vec3(0);t[axis]=float(upper);\n"
        "t[(axis+1)%3]=float(corner&1);t[(axis+2)%3]=float(corner>>1);\n"
        "vec3 q=mix(reach_min.xyz,reach_max.xyz,t);q.z/=dot(frame[2].xyz,frame[2].xyz);\n"
        "vec3 world=frame[3].xyz+mat3(frame)*q*radius;\n"
        "gl_Position=projection*vec4(transpose(mat3(camera))*(world-camera[3].xyz),1);\n"
        "gl_Position.xy+=clip_offset*gl_Position.w;\n"
        "if(reversed_depth!=0)gl_Position.z=gl_Position.w-gl_Position.z;}\n";
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
        "#ifdef SUN\n"
        "if(dot(n,camera[3].xyz-world)<0.) n=-n;\n"
        "result=vec4(sun_light(world,n/nl,true),0);\n"
        "#else\n"
        "float shadow=compare_at(uv,depth,gradient);\n"
        "if(soft!=0){vec2 size=vec2(textureSize(shadow_depth,0)), f=fract(uv*size-0.5), base=uv-f/size;\n"
        "shadow=mix(mix(compare_at(base,depth+dot(base-uv,gradient),gradient),compare_at(base+vec2(1,0)/size,depth+dot(base+vec2(1,0)/size-uv,gradient),gradient),f.x),\n"
        "mix(compare_at(base+vec2(0,1)/size,depth+dot(base+vec2(0,1)/size-uv,gradient),gradient),compare_at(base+1/size,depth+dot(base+1/size-uv,gradient),gradient),f.x),f.y);}\n"
        "float fade=clamp(1-q.z/4,0,1)*clamp(1+q.z*2,0,1);\n"
        "float facing=abs(dot(n/nl,frame[2].xyz));\n"
        "result=vec4(mix(vec3(1),color.xyz,shadow*fade*facing),1);\n"
        "#endif\n}\n";
    int size = 128 * (int)halo_shadow_resolution_scale();
    if (environment.failed_size != size) environment.failed_size = 0;
    if ((!projector && environment.failed_size) || !create_resources()) return 0;
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
    /* A fixture or flashlight writes its own depth array. It needs the caster
       program, but never the legacy environmental depth attachment. */
    if (projector) return environment.caster != 0;
    if (!environment.receiver)
    {
        environment.receiver = environment_program(receiver_vertex, receiver_fragment);
        if (environment.receiver) environment_receiver_locations(environment.receiver, &environment.uniforms, 0);
    }
    if (sun.projecting && !sun.static_caster)
    {
        /* The sign records sky ownership. Both signs block model receivers. */
        static const char sun_fragment[] =
            "#version 330 core\n"
            "in vec2 texcoord; uniform sampler2D alpha_map; uniform int alpha_test, eligible; out float visibility;\n"
            "void main(){if(alpha_test!=0 && texture(alpha_map,texcoord).a<=127.0/255.0) discard;"
            "visibility=eligible!=0?gl_FragCoord.z:-gl_FragCoord.z;}\n";
        sun.static_caster = environment_program(caster_vertex,sun_fragment);
        if (!sun.static_caster) return 0;
    }
    if (sun.projecting && !sun.receiver)
    {
        static const char sun_extensions[] =
            "#ifdef GL_ARB_texture_gather\n#extension GL_ARB_texture_gather : enable\n#define SUN_GATHER\n#endif\n#define SUN\n";
        char source[sizeof(receiver_fragment)+sizeof(shadow_filter)+sizeof(sun_lighting)+sizeof(sun_extensions)];
        const char version[] = "#version 330 core\n";
        strcpy(source, version);
        strcat(source,sun_extensions);
        strcat(source,shadow_filter);
        strcat(source,sun_lighting);
        strcat(source, receiver_fragment + sizeof(version) - 1);
        sun.receiver = environment_program(sun_vertex, source);
        if (!sun.receiver) return 0;
        environment_receiver_locations(sun.receiver, &sun.uniforms, 1);
    }
    if (!environment.caster || !environment.receiver) return 0;
    if (!environment.depth || environment.size != size)
    {
        if (!environment_resize_texture(&environment.depth, &environment.depth_fbo, size, 0)) return 0;
        environment.size = size;
    }
    return 1;
}

static int environment_resources(void)
{
    return environment_resources_for(0);
}

static int environment_snapshot(void)
{
    int size[2];
    GLenum format;
    GLuint source;
    if (!xgpu_gl_scene_target(&environment.destination, size, &format, &source, environment.viewport, environment.pixel_center)) return 0;
    if (size[0] != environment.scene_size[0] || size[1] != environment.scene_size[1] || format != environment.scene_format)
    {
        if (environment.scene_depth) glDeleteTextures(1, &environment.scene_depth);
        environment.scene_depth = 0;
        sun.checked_receiver_target = 0;
        environment.verified_copy_source = 0;
        environment.copied = 0;
        memcpy(environment.scene_size, size, sizeof(size));
        environment.scene_format = format;
    }
    if (environment.copied) return 1;
    if (!environment.scene_fbo) glGenFramebuffers(1, &environment.scene_fbo);
    if (!environment.scene_depth)
    {
        glGenTextures(1, &environment.scene_depth);
        if (!environment.scene_depth) return 0;
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, environment.scene_depth);
        glTexImage2D(GL_TEXTURE_2D, 0, format, size[0], size[1], 0, GL_DEPTH_STENCIL,
            format == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    /* Single-sample depth/stencil images have identical formats. Copy both
       planes together without a framebuffer depth resolve. The receiver
       always samples its private image, including on the multisample path. */
    if (sun.projecting && source && glCopyImageSubData)
    {
        if (environment.verified_copy_source != source) sun.checked_receiver_target = 0;
        glCopyImageSubData(source,GL_TEXTURE_2D,0,0,0,0,environment.scene_depth,
            GL_TEXTURE_2D,0,0,0,0,size[0],size[1],1);
        /* Source/target format, dimensions and sample count are fixed for
           this image pair. Validate a new pair before trusting it. A rejected
           copy still takes the depth-blit path on the same frame. */
        if (environment.verified_copy_source == source || glGetError() == GL_NO_ERROR)
        {
            environment.verified_copy_source = source;
            environment.copied = 1;
            return 1;
        }
        environment.verified_copy_source = 0;
    }
    sun.checked_receiver_target = 0;
    environment.verified_copy_source = 0;
    if (!environment.scene_fbo) return 0;
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
        environment.failed_size = 128 * (int)halo_shadow_resolution_scale();
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
    GLuint p = sun.writing ? sun.static_caster : environment.caster;
    GLint frame=environment.caster_frame, radius=environment.caster_radius, alpha_map=environment.caster_alpha_map;
    GLint bones=environment.caster_bones, uv=environment.caster_uv, alpha=environment.caster_alpha, eligible=-1;
    if (sun.writing)
    {
        frame=glGetUniformLocation(p,"frame"); radius=glGetUniformLocation(p,"radius");
        alpha_map=glGetUniformLocation(p,"alpha_map"); bones=glGetUniformLocation(p,"bones");
        uv=glGetUniformLocation(p,"uv_transform"); alpha=glGetUniformLocation(p,"alpha_test");
        eligible=glGetUniformLocation(p,"eligible");
    }
    int previous = -1;
    int draw, success = !failed && (draw_count > 0 || spots.rendering || sun.projecting);
    if (success)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        int depth_size = spots.rendering ? spots.size : environment.size;
        glViewport(0,0,depth_size,depth_size);
        glClipControl(GL_LOWER_LEFT,GL_ZERO_TO_ONE);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE); glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_GREATER); glDepthMask(GL_TRUE);
        glDepthRange(0,1); glClearDepth(0); glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        if (!sun.partial) glClear(GL_DEPTH_BUFFER_BIT);
        if (sun.writing)
        {
            glColorMask(GL_TRUE,GL_FALSE,GL_FALSE,GL_FALSE);
            if (!sun.partial) { glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); }
        }
        glUseProgram(p); glBindVertexArray(vao);
        glUniformMatrix4fv(frame,1,GL_FALSE,environment.frame);
        glUniform1f(radius,environment.radius);
        glUniform1i(alpha_map,9);
        glActiveTexture(GL_TEXTURE9); glBindSampler(9,alpha_sampler);
        for (draw=0;draw<draw_count;draw++)
        {
            int mesh=draws[draw].mesh;
            bind_caster_draw(draw, &previous, bones, uv, alpha);
            if (sun.writing)
            {
                const unsigned char *owners=sun.surface_sky[draw];
                int first=0, count=meshes[mesh].count/3;
                while (first<count)
                {
                    int last=first+1, lit=owners[first]==sun.sky;
                    while (last<count && (owners[last]==sun.sky)==lit) last++;
                    glUniform1i(eligible,lit);
                    glDrawElements(GL_TRIANGLES,(last-first)*3,GL_UNSIGNED_SHORT,
                        (void *)(size_t)(first*3*sizeof(unsigned short)));
                    first=last;
                }
            }
            else glDrawElements(GL_TRIANGLES,meshes[mesh].count,GL_UNSIGNED_SHORT,NULL);
        }
        /* Storage and programs are checked when created. Like the local-light
           depth pass, keep draws asynchronous; debug.gl_debug reports command
           errors through the desktop callback without a query after each map. */
    }
    return success;
}

/* The sun projects the same 36 vertices with the same program and images
   until a target changes. Validate a new combination, then let the driver
   queue its draws. The per-object and resolved-depth paths still check each
   draw so a failed replacement can return to the stock object shadow. */
static int environment_draw_receiver(void)
{
    GLuint target = sun.projecting && environment.verified_copy_source ? environment.destination : 0;
    int validated = target && target == sun.checked_receiver_target;
    if (!validated && glGetError() != GL_NO_ERROR) return 0;
    glDrawArrays(GL_TRIANGLES, 0, sun.projecting ? 36 : 3);
    if (validated) return 1;
    if (glGetError() != GL_NO_ERROR) return 0;
    if (target) sun.checked_receiver_target = target;
    return 1;
}

int modern_environment_end(int complete, const float camera[16], const float projection[16], const float color[3])
{
    const struct environment_receiver_uniforms *u = sun.projecting ? &sun.uniforms : &environment.uniforms;
    GLuint p;
    float viewport[4], rgba[4] = {color[0],color[1],color[2],1};
    int scissor[4];
    int draw, success = complete && !failed && (sun.projecting ? sun.ready : draw_count > 0);
    if (!environment_drawing) return 0;
    if (success && !sun.projecting) success = environment_draw_depth(environment.depth_fbo);
    if (success)
    {
        p=sun.projecting ? sun.receiver : environment.receiver;
        glBindFramebuffer(GL_FRAMEBUFFER,environment.destination);
        glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
        glViewport(environment.viewport[0],environment.viewport[1],environment.viewport[2],environment.viewport[3]);
        if (sun.projecting) memcpy(scissor,environment.viewport,sizeof(scissor));
        else environment_scissor(camera, projection, scissor);
        glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
        glEnable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        /* Preserve the stock environmental receiver boundary. Model base passes
           already contain local illumination; multiplying them here would
           shadow unrelated light contributions. */
        glEnable(GL_STENCIL_TEST); glStencilFunc(GL_EQUAL,128,129);
        glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP); glStencilMask(0);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(sun.projecting ? GL_DST_COLOR : GL_ZERO,sun.projecting ? GL_ONE : GL_SRC_COLOR);
        glUseProgram(p);
        glBindTextures(8,1,sun.projecting ? &sun.object_depth : &environment.depth); glBindSampler(8,0);
        glBindTextures(10,1,&environment.scene_depth); glBindSampler(10,0);
        if (sun.projecting)
        {
            /* Model and BSP draws run between depth generation and projection. */
            glBindVertexArray(vao);
            glDisable(GL_CULL_FACE);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glDepthRange(0,1);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_DEPTH_CLAMP);
            glDepthFunc(xgpu_reversed_depth ? GL_LEQUAL : GL_GEQUAL);
            glUniform2f(u->clip_offset,2.f*environment.pixel_center[0]/environment.viewport[2],
                -2.f*environment.pixel_center[1]/environment.viewport[3]);
            glBindTextures(9,1,&sun.static_depth); glBindSampler(9,0);
            glUniformMatrix4fv(u->sun_frame,1,GL_FALSE,sun.frame);
            glUniform4fv(u->sun_color_radius,1,(const float[4]){color[0],color[1],color[2],sun.radius});
            glUniform1i(u->sun_soft,sun.soft);
        }
        else
        {
            glUniform1i(u->soft,environment.soft);
            glUniform4fv(u->color,1,rgba);
        }
        glUniform1i(u->reversed_depth,xgpu_reversed_depth);
        for (draw=0;draw<4;draw++) viewport[draw]=(float)environment.viewport[draw];
        viewport[0] += environment.pixel_center[0]; viewport[1] += environment.pixel_center[1];
        glUniform4fv(u->viewport,1,viewport);
        glUniformMatrix4fv(u->frame,1,GL_FALSE,environment.frame);
        glUniformMatrix4fv(u->camera,1,GL_FALSE,camera);
        glUniformMatrix4fv(u->projection,1,GL_FALSE,projection);
        glUniform1f(u->radius,environment.radius);
        success = environment_draw_receiver();
    }
    if (sun.projecting) glDisable(GL_DEPTH_CLAMP);
    glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
    xgpu_gl_end_external_pass();
    drawing=environment_drawing=0;
    return success;
}

static int sun_depth_resources(void)
{
    if (sun.size != environment.size || !sun.static_depth || !sun.object_depth)
    {
        if (!environment_resize_texture(&sun.static_depth, &environment.depth_fbo, environment.size, 1)) return 0;
        sun.valid = sun.object_checked = 0;
        sun.checked_receiver_target = 0;
        if (!environment_resize_texture(&sun.object_depth,&sun.object_fbo,environment.size,0)) return 0;
        sun.size = environment.size;
    }
    return 1;
}

/* Returns 2 on a static cache hit, 1 when the caller must submit BSP depth. */
int modern_environment_sun_begin(const float frame[16], float radius, int cache_static, int sky, int soft)
{
    if (drawing || !xgpu_gl_ready()) return 0;
    sun.projecting = 1;
    if (!environment_resources()) goto fail;
    if (!sun_depth_resources()) goto fail;
    memcpy(environment.frame,frame,sizeof(environment.frame));
    environment.radius = radius;
    environment.soft = soft;
    drawing = environment_drawing = 1;
    draw_count = failed = 0;
    sun.partial = 0;
    sun.soft = soft;
    if (cache_static && sun.valid && sky==sun.sky && radius==sun.radius && !memcmp(sun.frame,frame,sizeof(sun.frame))) return 2;
    sun.writing=1;
    sun.sky=sky;
    memcpy(sun.frame,frame,sizeof(sun.frame));
    sun.radius = radius;
    sun.valid = 0;
    return 1;
fail:
    sun.projecting = 0;
    xgpu_gl_end_external_pass();
    return 0;
}

void modern_environment_sun_surfaces(const unsigned char *owners)
{
    if (draw_count>0) sun.surface_sky[draw_count-1]=owners;
}

/* Flush in bounded batches so dense BSPs and large scenery collections share
   the same map without dropping later casters. Clear each attachment once. */
static int sun_draw_batch(void)
{
    if (!sun.projecting || !environment_drawing) return 0;
    if (sun.writing)
    {
        glBindFramebuffer(GL_FRAMEBUFFER,environment.depth_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,sun.static_depth,0);
        GLenum target=GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1,&target);
    }
    int ok = environment_draw_depth(sun.writing ? environment.depth_fbo : sun.object_fbo);
    sun.partial = 1;
    draw_count = 0;
    return ok;
}

int modern_environment_sun_static(int complete)
{
    sun.valid = complete && sun_draw_batch() && glGetError()==GL_NO_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER,environment.depth_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);
    GLenum none=GL_NONE;
    glDrawBuffers(1,&none);
    sun.partial = sun.writing = draw_count = 0;
    return sun.valid;
}

int modern_environment_sun_finish(int complete, const float color[3])
{
    sun.ready = complete && sun.valid && sun_draw_batch();
    /* Validate the first posed draw on each new target. Later draws use the
       same checked framebuffer and program. Polling the GL worker for an
       error every frame serializes unrelated lighting work ahead of this pass. */
    if (sun.ready && !sun.object_checked)
        sun.object_checked = sun.ready = glGetError() == GL_NO_ERROR;
    memcpy(sun.tint,color,sizeof(sun.tint));
    sun.partial = sun.projecting = drawing = environment_drawing = draw_count = 0;
    glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
    xgpu_gl_end_external_pass();
    /* Units 14 and 15 belong exclusively to sunlight through this view. */
    if (sun.ready)
    {
        glBindTextures(14,1,&sun.static_depth); glBindSampler(14,0);
        glBindTextures(15,1,&sun.object_depth); glBindSampler(15,0);
    }
    return sun.ready;
}

int modern_environment_sun_project(const float camera[16], const float projection[16])
{
    if (drawing || !sun.ready) return 0;
    sun.projecting = drawing = environment_drawing = 1;
    failed = draw_count = 0;
    memcpy(environment.frame,sun.frame,sizeof(sun.frame));
    environment.radius = sun.radius;
    int ok = modern_environment_end(environment_snapshot(),camera,projection,sun.tint);
    sun.projecting = 0;
    return ok;
}

void modern_environment_sun_model(int eligible) { sun.model = sun.ready && eligible; }
void modern_environment_model_clear(void) { environment_model.enabled = sun.model = 0; }
int modern_environment_model_active(void) { return environment_model.enabled || sun.model; }
int modern_environment_model_reuse(void) { environment_model.enabled = environment_model.valid; return environment_model.enabled != 0; }

static int environment_model_resources(void)
{
    int i;
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
    return 1;
}

int modern_environment_model_begin(const float frames[32], float radius, int soft)
{
    environment_model.enabled = environment_model.valid = 0;
    if (drawing || !xgpu_gl_ready() || environment_model.count >= 32) return 0;
    environment_model.count++;
    if ((!environment.caster || !program || environment.size != 128 * (int)halo_shadow_resolution_scale()) &&
        !environment_resources()) { xgpu_gl_end_external_pass(); return 0; }
    if (!environment_model_resources()) return 0;
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
