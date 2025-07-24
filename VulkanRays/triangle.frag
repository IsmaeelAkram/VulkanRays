#version 450
layout(location = 0) in vec3 vPos;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4((vPos + vec3(1.0)) * 0.5, 1.0); // Map [-1,1] to [0,1]
}