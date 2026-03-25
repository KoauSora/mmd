#version 330 core

in vec2 vUV;

uniform sampler2D uHdrColor;
uniform float uExposure;
uniform float uGamma;

out vec4 FragColor;

void main() {
    vec3 hdr = texture(uHdrColor, vUV).rgb;

    // Reinhard tone mapping.
    vec3 mapped = vec3(1.0) - exp(-hdr * max(uExposure, 0.0001));

    // Linear -> sRGB.
    vec3 ldr = pow(clamp(mapped, 0.0, 1.0), vec3(1.0 / max(uGamma, 0.0001)));
    FragColor = vec4(ldr, 1.0);
}
