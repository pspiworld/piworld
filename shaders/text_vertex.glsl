precision highp float;

uniform mat4 matrix;
uniform vec3 camera;
uniform float fog_distance;
uniform int ortho;

attribute vec4 position;
attribute vec2 uv;
attribute vec4 color;

varying vec2 fragment_uv;
varying vec4 fragment_color;
varying float fog_factor;
varying float fog_height;

const float pi = 3.14159265;

void main() {
    gl_Position = matrix * position;
    fragment_uv = uv;
    fragment_color = color;

    // Fog
    if (bool(ortho)) {
        fog_factor = 0.0;
        fog_height = 0.0;
    }
    else {
        float camera_distance = distance(camera, vec3(position));
        fog_factor = pow(clamp(camera_distance / fog_distance, 0.0, 1.0), 4.0);
        float dy = gl_Position.y - camera.y;
        float dx = distance(gl_Position.xz, camera.xz);
        fog_height = (atan(dy, dx) + pi / 2.0) / pi;
    }
}
