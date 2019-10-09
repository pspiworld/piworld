precision highp float;

uniform sampler2D sampler;
uniform bool is_sign;

varying vec2 fragment_uv;
varying vec4 fragment_color;

void main() {
    vec4 color = texture2D(sampler, fragment_uv);
    if (is_sign) {
        if (color == vec4(1.0)) {
            discard;
        }
        else {
            color = fragment_color;
        }
    }
    else {
        color.a = max(color.a, 0.4);
    }
    gl_FragColor = color;
}
