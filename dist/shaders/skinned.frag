#version 330 core

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;

uniform vec4 uBaseColor;
uniform vec3 uLightDir;
uniform int uUseTexture;
uniform sampler2D uDiffuseTex;
uniform int uUseToonTex;
uniform sampler2D uToonTex;
uniform vec3 uCameraPos;
uniform float uAlphaCutoff;


out vec4 FragColor;

void main() {
    vec4 albedo = uBaseColor;
    if (uUseTexture != 0) {
        albedo *= texture(uDiffuseTex, vUV);
    }
    if (uAlphaCutoff > 0.0 && albedo.a < uAlphaCutoff) {
        discard;
    }

    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // sRGB -> Linear
    vec3 albedoLinear = pow(albedo.rgb, vec3(2.2));

    float ndotl = max(dot(N, L), 0.0);

    // Toon shading: sample ramp directly by lighting.
    // Many MMD toon textures are already strip/ramp images. We apply only a small bias/scale
    // to tune the "threshold" (contrast) and support both vertical and horizontal ramps.
    vec3 diffuse = vec3(0.0);
    if (uUseToonTex != 0) {
        const float bias = 0.05;
        const float scale = 1.15;
        float t = clamp((ndotl + bias) * scale, 0.0, 1.0);
        float v = 1.0 - t;

        vec3 toonSRGB_V = texture(uToonTex, vec2(0.5, v)).rgb;
        vec3 toonSRGB_H = texture(uToonTex, vec2(v, 0.5)).rgb;
        vec3 toonSRGB = max(toonSRGB_V, toonSRGB_H);
        vec3 toonLinear = pow(toonSRGB, vec3(2.2));
        diffuse = albedoLinear * toonLinear;
    } else {
        // Fallback: softer anime-style diffuse (reduced harsh shadow contrast).
        float softDiffuse = mix(0.50, 1.0, smoothstep(0.0, 0.85, ndotl));
        diffuse = albedoLinear * softDiffuse;
    }

    // Keep only a tiny clean highlight to avoid plastic/waxy skin.
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 128.0);
    vec3 specular = vec3(0.02) * spec;

    vec3 ambient = albedoLinear * vec3(0.20);
    float hemi = 0.5 * N.y + 0.5;
    vec3 skyFill = albedoLinear * vec3(0.06, 0.07, 0.09) * hemi;

    vec3 colorLinear = ambient + diffuse + skyFill + specular;
    FragColor = vec4(max(colorLinear, vec3(0.0)), albedo.a);
}