#version 450
layout(push_constant) uniform Draw { float depth; vec4 color; } draw_state;
layout(location = 0) out vec4 color;
void main()
{
    const vec2 positions[3] = vec2[3](vec2(-0.75, -0.75), vec2(0.75, -0.75), vec2(0.0, 0.75));
    gl_Position = vec4(positions[gl_VertexIndex], draw_state.depth, 1.0);
    color = draw_state.color;
}
