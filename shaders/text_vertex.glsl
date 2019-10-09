precision highp float;

uniform mat4 matrix;

attribute vec4 position;
attribute vec2 uv;
attribute vec4 color;

varying vec2 fragment_uv;
varying vec4 fragment_color;

void main() {
    gl_Position = matrix * position;
    fragment_uv = uv;
    fragment_color = color;
}
