#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV; // unused, but present in VBO layout
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorldNormal;

void main() {
    mat3 Nmat = mat3(transpose(inverse(uModel))); // safe even with non-uniform scale
    vWorldNormal = normalize(Nmat * aNormal);
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}

// output normal vectorx