#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 vPos;
layout(set = 0, binding = 0) uniform MVP {
    mat4 mvp;
} ubo;
void main() {
    vPos = inPosition;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}