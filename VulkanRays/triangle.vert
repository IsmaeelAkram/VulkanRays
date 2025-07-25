#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 vPos;
layout(location = 1) out vec3 vColor;
layout(set = 0, binding = 0) uniform MVP {
    mat4 mvp;
} ubo;
void main() {
    vPos = inPosition;
    vColor = inColor;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}