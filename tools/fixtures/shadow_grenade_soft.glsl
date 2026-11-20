// M4A receiver experiment. The effective radius is an approximation in world
// units. Search/filter radii are angular; map resolution limits their footprint.
// Offset rays estimate visibility at the original receiver. A nearby ray can
// miss the receiver plane or intersect its extension far from that receiver.
// The plane identifies a sample of the surface itself. Other hits are compared
// with the actual receiver projected onto the sampled cubemap texel's ray.
vec2 grenade_offset_sample(float stored_distance, vec3 receiver, vec3 texel_ray,
                           vec3 normal, float plane_numerator)
{
    vec3 direction = normalize(texel_ray);
    float receiver_depth = dot(receiver, direction);
    float bias = max(0.0005, receiver_depth*0.00001);
    // Evaluate the plane residual along its normal. Dividing by incidence
    // magnifies depth roundoff into alternating stripes on grazing surfaces.
    float incidence = dot(direction, normal);
    float plane_error = abs(stored_distance*incidence-plane_numerator);
    float plane_tolerance = bias*abs(incidence) + 0.00001*stored_distance;
    bool same_surface = incidence*plane_numerator > 0.0 && plane_error <= plane_tolerance;
    float visibility = same_surface ? 1.0 : step(receiver_depth-bias, stored_distance);
    return vec2(visibility, stored_distance);
}

float grenade_soft_visibility(samplerCube depth, vec4 light, vec3 d, vec3 n)
{
    const float emitter_radius = 0.01;
    const vec2 ring[8] = vec2[8](
        vec2(0.34, 0.0), vec2(-0.38, 0.35), vec2(0.06, -0.63), vec2(0.45, 0.59),
        vec2(-0.82, -0.14), vec2(0.77, -0.49), vec2(-0.25, 0.93), vec2(-0.47, -0.80));
    float distance = length(d);
    vec3 axis = d / distance;
    // Orthonormal basis around the source ray, independent of cubemap face.
    float sign_z = axis.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (sign_z + axis.z);
    float b = axis.x * axis.y * a;
    vec3 u = vec3(1.0 + sign_z*axis.x*axis.x*a, sign_z*b, -sign_z*axis.x);
    vec3 v = vec3(b, sign_z + axis.y*axis.y*a, -axis.y);
    float texel = 2.0 * max(abs(axis.x), max(abs(axis.y), abs(axis.z))) / float(textureSize(depth, 0).x);
    // Near casing can block the emitter well outside the receiver footprint.
    // Search broadly, then bound the eight-tap filter to avoid separated bands.
    float footprint = min(8.0*emitter_radius / distance, 48.0 * texel);
    vec2 center = grenade_depth_sample(depth, light.w, axis, d, n, false);
    float count = 1.0 - center.x;
    float sum = count * center.y;
    const vec2 search[4] = vec2[4](vec2(1,0), vec2(-1,0), vec2(0,1), vec2(0,-1));
    for (int i=0; i<4; ++i) {
        vec3 ray = axis + footprint*(u*search[i].x + v*search[i].y);
        vec2 hit = grenade_depth_sample(depth, light.w, ray, d, n, true);
        float blocked = 1.0 - hit.x;
        sum += blocked * hit.y;
        count += blocked;
    }
    if (count == 0.0) return center.x;
    float blocker = max(sum/count, 0.00001);
    float penumbra = min(20.0*texel, emitter_radius*max(distance-blocker,0.0)/(distance*blocker));
    if (penumbra < 0.25*texel) return center.x;
    float visible = center.x;
    for (int i=0; i<8; ++i) {
        vec3 ray = axis + penumbra*(u*ring[i].x + v*ring[i].y);
        visible += grenade_depth_sample(depth, light.w, ray, d, n, true).x;
    }
    return mix(center.x, visible/9.0, smoothstep(0.25*texel, texel, penumbra));
}
