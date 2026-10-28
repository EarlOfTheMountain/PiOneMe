// GK unlit fragment
precision mediump float;

uniform sampler2D u_tex;
uniform vec4 u_tint;

varying vec2 v_uv;

void main() {
    gl_FragColor = texture2D(u_tex, v_uv) * u_tint;
}
