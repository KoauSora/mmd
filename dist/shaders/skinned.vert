#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in ivec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform samplerBuffer uBonesTex;
uniform int uBoneCount;
uniform vec3 uCameraPos;


out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vUV;

mat4 fetchBoneMat(int boneIndex) {
    int idx = clamp(boneIndex, 0, max(uBoneCount - 1, 0));
    int base = idx * 4;
    vec4 r0 = texelFetch(uBonesTex, base + 0);
    vec4 r1 = texelFetch(uBonesTex, base + 1);
    vec4 r2 = texelFetch(uBonesTex, base + 2);
    vec4 r3 = texelFetch(uBonesTex, base + 3);
    return mat4(r0, r1, r2, r3);
}

void main() {
    mat4 b0 = fetchBoneMat(aBoneIds.x);
    mat4 b1 = fetchBoneMat(aBoneIds.y);
    mat4 b2 = fetchBoneMat(aBoneIds.z);
    mat4 b3 = fetchBoneMat(aBoneIds.w);

    mat4 skinMat =
          aBoneWeights.x * b0
        + aBoneWeights.y * b1
        + aBoneWeights.z * b2
        + aBoneWeights.w * b3;

    vec4 skinnedLocalPos = skinMat * vec4(aPos, 1.0);

    mat3 skinNormalMat =
          aBoneWeights.x * mat3(b0)
        + aBoneWeights.y * mat3(b1)
        + aBoneWeights.z * mat3(b2)
        + aBoneWeights.w * mat3(b3);
    vec3 skinnedLocalNormal = normalize(skinNormalMat * aNormal);

    vec4 worldPos = uModel * skinnedLocalPos;
    vWorldPos = worldPos.xyz;

    mat3 modelNormalMat = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(modelNormalMat * skinnedLocalNormal);
    vUV = aUV;

    gl_Position = uProj * uView * worldPos;
}