#include "MathUtils.h"
#include <cmath>
#include <cstring>

Mat4 perspective(float fovy, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy * 0.5f);
    Mat4 mat = {};
    mat.m[0] = f / aspect;
    mat.m[5] = f;
    mat.m[10] = (zfar + znear) / (znear - zfar);
    mat.m[11] = -1.0f;
    mat.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return mat;
}

Mat4 lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
    float fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
    float rlf = 1.0f / sqrtf(fx*fx + fy*fy + fz*fz);
    fx *= rlf; fy *= rlf; fz *= rlf;
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float rls = 1.0f / sqrtf(sx*sx + sy*sy + sz*sz);
    sx *= rls; sy *= rls; sz *= rls;
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;
    Mat4 mat = {};
    mat.m[0] = sx; mat.m[4] = sy; mat.m[8] = sz;  mat.m[12] = 0.0f;
    mat.m[1] = ux; mat.m[5] = uy; mat.m[9] = uz;  mat.m[13] = 0.0f;
    mat.m[2] = -fx; mat.m[6] = -fy; mat.m[10] = -fz; mat.m[14] = 0.0f;
    mat.m[3] = mat.m[7] = mat.m[11] = 0.0f; mat.m[15] = 1.0f;
    // Translate
    mat.m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    mat.m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    mat.m[14] = fx*eyeX + fy*eyeY + fz*eyeZ;
    return mat;
}

Mat4 rotationX(float angle) {
    Mat4 m = {};
    m.m[0] = 1.0f;
    m.m[5] = cosf(angle); m.m[6] = -sinf(angle);
    m.m[9] = sinf(angle); m.m[10] = cosf(angle);
    m.m[15] = 1.0f;
    return m;
}

Mat4 rotationY(float angle) {
    Mat4 m = {};
    m.m[0] = cosf(angle); m.m[2] = sinf(angle);
    m.m[5] = 1.0f;
    m.m[8] = -sinf(angle); m.m[10] = cosf(angle);
    m.m[15] = 1.0f;
    return m;
}

Mat4 rotationZ(float angle) {
    Mat4 m = {};
    m.m[0] = cosf(angle); m.m[4] = -sinf(angle);
    m.m[1] = sinf(angle); m.m[5] = cosf(angle);
    m.m[10] = 1.0f;
    m.m[15] = 1.0f;
    return m;
}

Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                r.m[i + j*4] += a.m[i + k*4] * b.m[k + j*4];
    return r;
}
