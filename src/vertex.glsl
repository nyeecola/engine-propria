#version 330

uniform mat4 MVP;

attribute vec3 vCol;
attribute vec2 vPos;

smooth out vec3 color;

void main() {
    vec2 vPoss = vPos * 100.0f;
    gl_Position = MVP * vec4(vPoss, 0.0, 1.0);
    color = vCol;
};
