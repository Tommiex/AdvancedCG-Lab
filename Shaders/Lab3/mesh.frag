#version 330 core
in vec3 vWorldNormal;
out vec4 FragColor;
uniform vec3 uColor;
uniform vec3 uLightDir; // world-space direction TO light (or from light)
void main() {
    vec3 N = normalize(vWorldNormal);
    float ndl = max(dot(N, normalize(uLightDir)), 0.0);
    vec3 ambient = 0.18 * uColor;
    vec3 diffuse = 0.82 * uColor * ndl;
    FragColor = vec4(ambient + diffuse, 1.0);
}