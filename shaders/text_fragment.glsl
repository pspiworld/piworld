precision highp float;

uniform sampler2D sampler;
uniform bool is_sign;

varying vec2 fragment_uv;
varying vec4 fragment_color;

const vec4 hud_text_background = vec4(0.4, 0.4, 0.4, 0.4);
const vec4 hud_text_color = vec4(0.85, 0.85, 0.85, 1.0);

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
    else {  // HUD text
        if (color == vec4(1.0)) {
            color = hud_text_background;
        }
        else {
            color = hud_text_color;
        }
    }
    gl_FragColor = color;
}
