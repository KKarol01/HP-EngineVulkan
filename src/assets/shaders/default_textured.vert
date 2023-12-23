#version 460 core

layout(location=0) in vec3 ipos;
layout(location=1) in vec3 inorm;
layout(location=2) in vec2 itc;

layout(location=0) out vec2 vtc;

void main() {
    vtc = itc;
    gl_Position = vec4(ipos.xy, 0.0, 1.0);
}
