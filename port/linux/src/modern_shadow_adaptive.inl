/* GPU timestamps bracket primary-view rendering through the final blit. Read
   completed samples only. Presentation waits and the frame limiter fall
   outside the GPU interval; delivered FPS is measured separately. */
#include "port_config.h"
#include <SDL3/SDL_timer.h>

static struct
{
    unsigned long revision;
    int configured, resolution, count, target, selected_ceiling, ceiling, scale, slots;
    int frames, samples, slow, spare, opened, pending, primary, promoted;
    GLuint stamps[2];
    Uint64 start, previous;
    double gpu_ms;
} adaptive;

static void adaptive_reset(void)
{
    if (adaptive.stamps[0]) glDeleteQueries(2, adaptive.stamps);
    memset(&adaptive, 0, sizeof(adaptive));
}

static int adaptive_target(long fps)
{
    switch (fps)
    {
    case 30: case 60: case 120: case 144: case 165: case 180: case 240: return (int)fps;
    default: return 240;
    }
}

static void adaptive_configure(void)
{
    unsigned long revision = config_changes();
    int ceiling, resolution, count, target;
    ceiling = (int)halo_shadow_resolution_scale();
    if (adaptive.configured && adaptive.revision == revision && adaptive.selected_ceiling == ceiling) return;
    resolution = config_boolean("display.adaptive_shadow_resolution");
    count = config_boolean("display.adaptive_shadow_budget");
    target = adaptive_target(config_integer("display.shadow_frame_budget"));
    if (!adaptive.configured || resolution != adaptive.resolution || count != adaptive.count ||
        target != adaptive.target || ceiling != adaptive.selected_ceiling)
    {
        adaptive_reset();
        adaptive.resolution = resolution;
        adaptive.count = count;
        adaptive.target = target;
        adaptive.selected_ceiling = adaptive.ceiling = ceiling;
        adaptive.scale = resolution && ceiling > 8 ? 8 : ceiling;
        adaptive.slots = MODERN_SHADOW_LIGHTS;
        adaptive.configured = 1;
    }
    adaptive.revision = revision;
}

/* Finish the current view at a consistent size. The next primary view
   retries one tier lower and excludes the failed tier for this map. */
static void adaptive_reject_size(void)
{
    if (adaptive.resolution && adaptive.scale > 1) adaptive.ceiling = adaptive.scale / 2;
}

static int modern_shadow_pixels(void)
{
    return 128 * (adaptive.resolution ? adaptive.scale : (int)halo_shadow_resolution_scale());
}

static void adaptive_window(double fps, double render_ms)
{
    double budget = 1000.0 / adaptive.target;
    int scale = adaptive.scale, slots = adaptive.slots;
    adaptive.slow = fps < adaptive.target * .98 && render_ms > budget * .90 ? adaptive.slow + 1 : 0;
    adaptive.spare = render_ms < budget * .72 ? adaptive.spare + 1 : 0;
    if (adaptive.slow >= 2)
    {
        if (adaptive.resolution && scale > 1) scale /= 2;
        else if (adaptive.count && slots > 1) slots--;
        adaptive.slow = 0;
    }
    if (adaptive.spare >= 6)
    {
        if (adaptive.count && slots < MODERN_SHADOW_LIGHTS) slots++;
        else if (adaptive.resolution && scale < adaptive.ceiling) scale *= 2;
        adaptive.spare = 0;
    }
    if (scale != adaptive.scale || slots != adaptive.slots)
    {
        adaptive.scale = scale;
        adaptive.slots = slots;
        platform_log("Adaptive shadows: %d px, %d local slots, target %d FPS", 128*scale, slots, adaptive.target);
    }
}

int modern_adaptive_begin(void)
{
    Uint64 now;
    GLuint ready;
    adaptive_configure();
    if ((!adaptive.resolution && !adaptive.count) || !xgpu_gl_ready()) return MODERN_SHADOW_LIGHTS;
    if (adaptive.primary) return adaptive.slots;
    adaptive.primary = 1;
    adaptive.promoted = 0;
    if (adaptive.scale > adaptive.ceiling)
    {
        adaptive.scale = adaptive.ceiling;
        adaptive.start = 0;
    }
    now = SDL_GetTicksNS();
    if (!adaptive.start || now - adaptive.previous > 250000000)
    {
        adaptive.start = now;
        adaptive.frames = adaptive.samples = adaptive.slow = adaptive.spare = 0;
        adaptive.gpu_ms = 0.;
    }
    else adaptive.frames++;
    adaptive.previous = now;
    if (adaptive.pending)
    {
        glGetQueryObjectuiv(adaptive.stamps[1], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready)
        {
            GLuint64 start, end;
            glGetQueryObjectui64v(adaptive.stamps[0], GL_QUERY_RESULT, &start);
            glGetQueryObjectui64v(adaptive.stamps[1], GL_QUERY_RESULT, &end);
            if (end >= start && end - start < 250000000)
            {
                adaptive.gpu_ms += (end - start) / 1e6;
                adaptive.samples++;
            }
            adaptive.pending = 0;
        }
    }
    if (now - adaptive.start >= 500000000)
    {
        if (adaptive.samples >= 2)
            adaptive_window(adaptive.frames * 1e9 / (now-adaptive.start), adaptive.gpu_ms / adaptive.samples);
        else adaptive.slow = adaptive.spare = 0;
        adaptive.frames = adaptive.samples = 0;
        adaptive.gpu_ms = 0.;
        adaptive.start = now;
    }
    if (!(adaptive.frames % 4) && !adaptive.pending)
    {
        if (!adaptive.stamps[0]) glGenQueries(2, adaptive.stamps);
        glQueryCounter(adaptive.stamps[0], GL_TIMESTAMP);
        adaptive.opened = 1;
    }
    return adaptive.slots;
}

void modern_adaptive_present(void)
{
    if (adaptive.opened)
    {
        glQueryCounter(adaptive.stamps[1], GL_TIMESTAMP);
        adaptive.pending = 1;
        adaptive.opened = 0;
    }
    adaptive.primary = 0;
}
