precision highp float;

uniform sampler2D sampler;
uniform bool is_sign;
uniform sampler2D sky_sampler;
uniform float timer;
uniform vec4 hud_text_background;
uniform vec4 hud_text_color;

varying vec2 fragment_uv;
varying vec4 fragment_color;
varying float fog_factor;
varying float fog_height;

void main() {
    vec4 color = texture2D(sampler, fragment_uv);
    if (is_sign) {
        if (color == vec4(1.0)) {
            discard;
        }
        else {
            color = fragment_color;
            vec4 sky_color = texture2D(sky_sampler, vec2(timer, fog_height));
            color = mix(color, sky_color, fog_factor);
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
