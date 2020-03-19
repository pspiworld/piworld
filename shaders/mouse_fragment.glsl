precision highp float;

uniform sampler2D sampler;

varying vec2 fragment_uv;

void main() {
    vec3 color = vec3(texture2D(sampler, fragment_uv));
    if (color == vec3(1.0, 0.0, 1.0)) {
        discard;
    }
    gl_FragColor = vec4(color, 1.0);
}
