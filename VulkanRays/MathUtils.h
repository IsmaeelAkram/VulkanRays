#pragma once
#include <cmath>

struct Mat4 {
    float m[16];
};

Mat4 perspective(float fovy, float aspect, float znear, float zfar);
Mat4 lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);
Mat4 rotationX(float angle);
Mat4 rotationY(float angle);
Mat4 rotationZ(float angle);
Mat4 mat4_mul(const Mat4& a, const Mat4& b);
