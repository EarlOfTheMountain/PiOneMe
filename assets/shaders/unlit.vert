// GK unlit: plain textured mesh (particles, skydomes, UI meshes)
attribute vec3 a_pos;
attribute vec3 a_nrm;
attribute vec2 a_uv;

uniform mat4 u_vp;
uniform mat4 u_model;
uniform vec2 u_uv_scroll;

varying vec2 v_uv;

void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_uv = a_uv + u_uv_scroll;
    gl_Position = u_vp * world;
}
