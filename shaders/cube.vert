#version 450

layout(set = 0, binding = 0) uniform MatrixM
{
    mat4 M;
} _833;

layout(set = 0, binding = 1) uniform MatrixV
{
    mat4 V;
} _868;

layout(set = 0, binding = 2) uniform MatrixP
{
    mat4 P;
} _899;

layout(set = 0, binding = 4) uniform MatrixLightView
{
    mat4 u_LightView[1];
} _915;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 0) out vec3 v_vertexToEyeDirection;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec3 v_vertexToPointLightDirection[1];

mat4 transpose_inverse_mat4(mat4 m)
{
    float Coef00 = (m[2].z * m[3].w) - (m[3].z * m[2].w);
    float Coef02 = (m[1].z * m[3].w) - (m[3].z * m[1].w);
    float Coef03 = (m[1].z * m[2].w) - (m[2].z * m[1].w);
    float Coef04 = (m[2].y * m[3].w) - (m[3].y * m[2].w);
    float Coef06 = (m[1].y * m[3].w) - (m[3].y * m[1].w);
    float Coef07 = (m[1].y * m[2].w) - (m[2].y * m[1].w);
    float Coef08 = (m[2].y * m[3].z) - (m[3].y * m[2].z);
    float Coef10 = (m[1].y * m[3].z) - (m[3].y * m[1].z);
    float Coef11 = (m[1].y * m[2].z) - (m[2].y * m[1].z);
    float Coef12 = (m[2].x * m[3].w) - (m[3].x * m[2].w);
    float Coef14 = (m[1].x * m[3].w) - (m[3].x * m[1].w);
    float Coef15 = (m[1].x * m[2].w) - (m[2].x * m[1].w);
    float Coef16 = (m[2].x * m[3].z) - (m[3].x * m[2].z);
    float Coef18 = (m[1].x * m[3].z) - (m[3].x * m[1].z);
    float Coef19 = (m[1].x * m[2].z) - (m[2].x * m[1].z);
    float Coef20 = (m[2].x * m[3].y) - (m[3].x * m[2].y);
    float Coef22 = (m[1].x * m[3].y) - (m[3].x * m[1].y);
    float Coef23 = (m[1].x * m[2].y) - (m[2].x * m[1].y);
    vec4 Fac0 = vec4(Coef00, Coef00, Coef02, Coef03);
    vec4 Fac1 = vec4(Coef04, Coef04, Coef06, Coef07);
    vec4 Fac2 = vec4(Coef08, Coef08, Coef10, Coef11);
    vec4 Fac3 = vec4(Coef12, Coef12, Coef14, Coef15);
    vec4 Fac4 = vec4(Coef16, Coef16, Coef18, Coef19);
    vec4 Fac5 = vec4(Coef20, Coef20, Coef22, Coef23);
    vec4 Vec0 = vec4(m[1].x, m[0].x, m[0].x, m[0].x);
    vec4 Vec1 = vec4(m[1].y, m[0].y, m[0].y, m[0].y);
    vec4 Vec2 = vec4(m[1].z, m[0].z, m[0].z, m[0].z);
    vec4 Vec3 = vec4(m[1].w, m[0].w, m[0].w, m[0].w);
    vec4 Inv0 = vec4(1.0, -1.0, 1.0, -1.0) * (((Vec1 * Fac0) - (Vec2 * Fac1)) + (Vec3 * Fac2));
    vec4 Inv1 = vec4(-1.0, 1.0, -1.0, 1.0) * (((Vec0 * Fac0) - (Vec2 * Fac3)) + (Vec3 * Fac4));
    vec4 Inv2 = vec4(1.0, -1.0, 1.0, -1.0) * (((Vec0 * Fac1) - (Vec1 * Fac3)) + (Vec3 * Fac5));
    vec4 Inv3 = vec4(-1.0, 1.0, -1.0, 1.0) * (((Vec0 * Fac2) - (Vec1 * Fac4)) + (Vec2 * Fac5));
    mat4 Inverse = mat4(vec4(Inv0.x, Inv1.x, Inv2.x, Inv3.x), vec4(Inv0.y, Inv1.y, Inv2.y, Inv3.y), vec4(Inv0.z, Inv1.z, Inv2.z, Inv3.z), vec4(Inv0.w, Inv1.w, Inv2.w, Inv3.w));
    vec4 Row0 = vec4(Inverse[0].x, Inverse[0].y, Inverse[0].z, Inverse[0].w);
    float Determinant = dot(m[0], Row0);
    Inverse = Inverse * (1.0 / Determinant);
    return Inverse;
}

mat4 inverse_mat4(mat4 m)
{
    float Coef00 = (m[2].z * m[3].w) - (m[3].z * m[2].w);
    float Coef02 = (m[1].z * m[3].w) - (m[3].z * m[1].w);
    float Coef03 = (m[1].z * m[2].w) - (m[2].z * m[1].w);
    float Coef04 = (m[2].y * m[3].w) - (m[3].y * m[2].w);
    float Coef06 = (m[1].y * m[3].w) - (m[3].y * m[1].w);
    float Coef07 = (m[1].y * m[2].w) - (m[2].y * m[1].w);
    float Coef08 = (m[2].y * m[3].z) - (m[3].y * m[2].z);
    float Coef10 = (m[1].y * m[3].z) - (m[3].y * m[1].z);
    float Coef11 = (m[1].y * m[2].z) - (m[2].y * m[1].z);
    float Coef12 = (m[2].x * m[3].w) - (m[3].x * m[2].w);
    float Coef14 = (m[1].x * m[3].w) - (m[3].x * m[1].w);
    float Coef15 = (m[1].x * m[2].w) - (m[2].x * m[1].w);
    float Coef16 = (m[2].x * m[3].z) - (m[3].x * m[2].z);
    float Coef18 = (m[1].x * m[3].z) - (m[3].x * m[1].z);
    float Coef19 = (m[1].x * m[2].z) - (m[2].x * m[1].z);
    float Coef20 = (m[2].x * m[3].y) - (m[3].x * m[2].y);
    float Coef22 = (m[1].x * m[3].y) - (m[3].x * m[1].y);
    float Coef23 = (m[1].x * m[2].y) - (m[2].x * m[1].y);
    vec4 Fac0 = vec4(Coef00, Coef00, Coef02, Coef03);
    vec4 Fac1 = vec4(Coef04, Coef04, Coef06, Coef07);
    vec4 Fac2 = vec4(Coef08, Coef08, Coef10, Coef11);
    vec4 Fac3 = vec4(Coef12, Coef12, Coef14, Coef15);
    vec4 Fac4 = vec4(Coef16, Coef16, Coef18, Coef19);
    vec4 Fac5 = vec4(Coef20, Coef20, Coef22, Coef23);
    vec4 Vec0 = vec4(m[1].x, m[0].x, m[0].x, m[0].x);
    vec4 Vec1 = vec4(m[1].y, m[0].y, m[0].y, m[0].y);
    vec4 Vec2 = vec4(m[1].z, m[0].z, m[0].z, m[0].z);
    vec4 Vec3 = vec4(m[1].w, m[0].w, m[0].w, m[0].w);
    vec4 Inv0 = vec4(1.0, -1.0, 1.0, -1.0) * (((Vec1 * Fac0) - (Vec2 * Fac1)) + (Vec3 * Fac2));
    vec4 Inv1 = vec4(-1.0, 1.0, -1.0, 1.0) * (((Vec0 * Fac0) - (Vec2 * Fac3)) + (Vec3 * Fac4));
    vec4 Inv2 = vec4(1.0, -1.0, 1.0, -1.0) * (((Vec0 * Fac1) - (Vec1 * Fac3)) + (Vec3 * Fac5));
    vec4 Inv3 = vec4(-1.0, 1.0, -1.0, 1.0) * (((Vec0 * Fac2) - (Vec1 * Fac4)) + (Vec2 * Fac5));
    mat4 Inverse = mat4(vec4(Inv0), vec4(Inv1), vec4(Inv2), vec4(Inv3));
    vec4 Row0 = vec4(Inverse[0].x, Inverse[1].x, Inverse[2].x, Inverse[3].x);
    float Determinant = dot(m[0], Row0);
    Inverse = Inverse * (1.0 / Determinant);
    return Inverse;
}

void main()
{
    mat4 param = _833.M;
    mat4 N = transpose_inverse_mat4(param);
    vec4 ePosition = vec4(a_position, 1.0);
    vec4 normal = vec4(a_normal, 0.0);
    ePosition = ePosition;
    ePosition = _833.M * ePosition;
    normal = N * normalize(normal);
    mat4 param_1 = _868.V;
    v_vertexToEyeDirection = inverse_mat4(param_1)[3].xyz - ePosition.xyz;
    v_normal = normal.xyz;
    gl_Position = (_899.P * _868.V) * ePosition;
    mat4 param_2 = _915.u_LightView[0];
    v_vertexToPointLightDirection[0] = inverse_mat4(param_2)[3].xyz - ePosition.xyz;
}