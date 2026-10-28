// GK lit fragment: texture * tint * (ambient + diffuse), fogged by depth
precision mediump float;

uniform sampler2D u_tex;
uniform vec4  u_tint;
uniform vec3  u_light_dir;   // direction the light TRAVELS
uniform vec3  u_light_col;
uniform float u_ambient;
uniform vec3  u_fog_col;
uniform float u_fog_near;
uniform float u_fog_far;

varying vec3  v_nrm;
varying vec2  v_uv;
varying float v_depth;

void main() {
    vec4 tex = texture2D(u_tex, v_uv);
    vec3 n = normalize(v_nrm);
    float diff = max(dot(n, -u_light_dir), 0.0);
    vec3 col = tex.rgb * u_tint.rgb * (u_light_col * diff + vec3(u_ambient));
    float fog = clamp((v_depth - u_fog_near) / (u_fog_far - u_fog_near), 0.0, 1.0);
    gl_FragColor = vec4(mix(col, u_fog_col, fog), tex.a * u_tint.a);
}
