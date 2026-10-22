"""Draw-local honeycomb uniforms must preserve colours and clear between passes."""
import os
import ast
from pathlib import Path
import re
import shutil
import subprocess

import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]


def test_hex_links_are_continuous_across_cells(tmp_path):
    """Execute the shipped pattern with fixed pixel derivatives, not a copy.

    A per-cell pulse makes opposite sides of the same link disagree. Sample
    all six shared edges over time, including translated negative UVs.
    """
    cc = shutil.which("clang++")
    if not cc:
        pytest.skip("clang++ required")
    header = (ROOT / "port/linux/src/shield_honeycomb_gl.h").read_text()
    glsl = "".join(ast.literal_eval(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', header))
    pattern = glsl[glsl.index("float shield_hex("):glsl.index("vec4 shield_honeycomb_surface(")]
    source = r"""
#include <cassert>
#include <cmath>
#include <algorithm>
using std::min; using std::max;
struct vec2 {
    double x, y;
    vec2(double a, double b): x(a), y(b) {}
};
vec2 operator-(vec2 a, vec2 b) { return vec2(a.x-b.x,a.y-b.y); }
vec2 operator*(vec2 a, double b) { return vec2(a.x*b,a.y*b); }
vec2 mod(vec2 a, vec2 b) { return vec2(a.x-floor(a.x/b.x)*b.x,a.y-floor(a.y/b.y)*b.y); }
vec2 abs(vec2 a) { return vec2(fabs(a.x),fabs(a.y)); }
double dot(vec2 a, vec2 b) { return a.x*b.x+a.y*b.y; }
double length(vec2 a) { return sqrt(dot(a,a)); }
double footprint = 0.02;
vec2 dFdx(vec2) { return vec2(footprint,0); }
vec2 dFdy(vec2) { return vec2(0,footprint); }
double clamp(double x, double a, double b) { return min(max(x,a),b); }
double smoothstep(double a, double b, double x) {
    double t = clamp((x-a)/(b-a),0.,1.); return t*t*(3.-2.*t);
}
double mix(double a, double b, double t) { return a+(b-a)*t; }
""" + pattern.replace("float", "double") + r"""
int main() {
    const double pi = 3.141592653589793;
    for (int tile=-3; tile<=3; ++tile) {
        for (int edge=0; edge<6; ++edge) {
            double angle=edge*pi/3.;
            vec2 normal(cos(angle),sin(angle));
            double lowest=10., highest=0.;
            for (int time=0; time<200; ++time) {
                vec2 inside(normal.x*(0.8660254-0.00001)+tile*1.7320508,
                            normal.y*(0.8660254-0.00001));
                vec2 outside(normal.x*(0.8660254+0.00001)+tile*1.7320508,
                             normal.y*(0.8660254+0.00001));
                double a=shield_hex(inside,time*0.07), b=shield_hex(outside,time*0.07);
                assert(std::isfinite(a) && std::isfinite(b));
                assert(fabs(a-b)<0.001);
                assert(a>0.2); // Links dim without disappearing between pulses.
                lowest=min(lowest,a); highest=max(highest,a);
            }
            assert(highest>lowest*2.); // The pulse remains visibly distinct.
        }
    }
    footprint=2.; // Unresolved cells converge instead of sparkling as UVs move.
    double average=shield_hex(vec2(0.,0.),0.);
    for (int i=0; i<100; ++i)
        assert(fabs(shield_hex(vec2(i*0.137,-i*0.193),i*0.31)-average)<0.00001);
}
"""
    path = tmp_path / "links.cpp"
    path.write_text(source)
    exe = tmp_path / ("links.exe" if os.name == "nt" else "links")
    subprocess.run([cc, "-std=c++17", str(path), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_draw_uniform_lifetime(tmp_path):
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("clang required")
    renderer = (ROOT / "port/linux/src/d3d8_gl.c").read_text()
    source = """
#include <assert.h>
#include <string.h>
static float shield_honeycomb_value[3][4];
""" + function(renderer, "d3d8_gl_set_shield_honeycomb") + """
int main(void) {
    float difference[4] = {.1f, -.2f, .3f, 0.f};
    float parallel[4] = {.5f, .6f, .7f, .47f};
    float saved[4];
    memcpy(saved, parallel, sizeof(saved));
    assert(shield_honeycomb_value[0][3] == 0.f);
    d3d8_gl_set_shield_honeycomb(18.f, 12.f, difference, parallel);
    assert(shield_honeycomb_value[0][3] == 18.f);
    assert(!memcmp(shield_honeycomb_value[1], difference, 3 * sizeof(float)));
    assert(shield_honeycomb_value[1][3] == 12.f);
    assert(!memcmp(shield_honeycomb_value[2], saved, sizeof(saved)));
    assert(!memcmp(parallel, saved, sizeof(saved)));
    d3d8_gl_set_shield_honeycomb(0.f, 0.f, NULL, NULL);
    assert(shield_honeycomb_value[0][3] == 0.f);
    parallel[0] = 0.f; parallel[1] = 1.f; parallel[2] = 0.f;
    d3d8_gl_set_shield_honeycomb(45.f, 13.f, difference, parallel);
    assert(!memcmp(shield_honeycomb_value[2], parallel, sizeof(parallel)));
    assert(shield_honeycomb_value[0][3] == 45.f);
    d3d8_gl_set_shield_honeycomb(0.f, 0.f, NULL, NULL);
    assert(shield_honeycomb_value[0][3] == 0.f);
    return 0;
}
"""
    path = tmp_path / "uniforms.c"
    path.write_text(source)
    exe = tmp_path / ("uniforms.exe" if os.name == "nt" else "uniforms")
    subprocess.run([cc, "-std=gnu89", "-Wall", "-Werror", str(path), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_only_owned_plasma_draws_activate_surface():
    source = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_plasma_energy.c").read_text()
    draw = function(source, "rasterizer_plasma_energy_draw")
    start = draw.index("honeycomb_scale = shield_look_honeycomb_scale(group->shader);")
    submit = draw.index("rasterizer_transparent_geometry_group_draw__internal(", start)
    clear = draw.index("d3d8_gl_set_shield_honeycomb(0.f", submit)
    assert draw.index("overshield_effect_color(") < start < submit < clear
    assert "color_constants[1], color_constants[2]" in draw[start:submit]
    assert "D3DCULL_CCW" in draw[start:submit]
    assert "D3DCULL_NONE" in draw[submit:clear]
    glsl = (ROOT / "port/linux/src/shield_honeycomb_gl.h").read_text()
    assert "if (shield_honeycomb[0].w == 0.0) return original;" in glsl
