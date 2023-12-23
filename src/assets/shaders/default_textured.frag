#version 460 core

layout(location=0) out vec4 FRAG_COL;

layout(set=2, binding=0) uniform sampler2D sTexture;

layout(location=0) in vec2 vtc;

void main() {
    FRAG_COL = vec4(texture(sTexture, vtc).rgb, 1.0);
}
