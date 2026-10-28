// GK flat: solid-color geometry (debug lines, overlays)
attribute vec3 a_pos;

uniform mat4 u_vp;
uniform mat4 u_model;

void main() {
    gl_Position = u_vp * u_model * vec4(a_pos, 1.0);
}
