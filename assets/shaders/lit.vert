// GK lit: directional diffuse lighting + depth fog + UV scroll
attribute vec3 a_pos;
attribute vec3 a_nrm;
attribute vec2 a_uv;

uniform mat4 u_vp;
uniform mat4 u_model;
uniform vec3 u_cam_pos;
uniform vec2 u_uv_scroll;

varying vec3 v_nrm;
varying vec2 v_uv;
varying float v_depth;

void main() {
    mat3 nm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz);
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_nrm = nm * a_nrm;
    v_uv  = a_uv + u_uv_scroll;
    gl_Position = u_vp * world;
    v_depth = length(world.xyz - u_cam_pos);
}
