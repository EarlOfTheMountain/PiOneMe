// GK sprite fragment
precision mediump float;

uniform sampler2D u_tex;

varying vec2 v_uv;
varying vec4 v_col;

void main() {
    gl_FragColor = texture2D(u_tex, v_uv) * v_col;
}
