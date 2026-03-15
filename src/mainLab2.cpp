#undef GLFW_DLL
#include <iostream>
#include <vector>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Libs/Window.h"
#include "Libs/Shader.h"
static const GLint WIDTH = 900, HEIGHT = 650;
struct MeshGL
{
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;

    void destroy()
    {
        if (EBO)
            glDeleteBuffers(1, &EBO);
        if (VBO)
            glDeleteBuffers(1, &VBO);
        if (VAO)
            glDeleteVertexArrays(1, &VAO);
        VAO = VBO = EBO = 0;
        indexCount = 0;
    }
};

static MeshGL buildIndexedMesh(const std::vector<float> &positions, const std::vector<unsigned int> &indices)
{
    MeshGL m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // aPos (vec3)
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    // glEnableVertexAttribArray(0);

    const GLsizei stride = 8 * sizeof(float);
    // location 0: aPos (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)(0));
    glEnableVertexAttribArray(0);
    // location 1: aNormal (vec3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // location 2: aUV (vec2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    m.indexCount = static_cast<GLsizei>(indices.size());
    return m;
}

static MeshGL makeSphere(int stacks = 32, int slices = 64)
{
    // Vertex layout: pos(3), normal(3), uv(2) = 8 floats
    std::vector<float> v;
    std::vector<unsigned int> idx;
    v.reserve((stacks + 1) * (slices + 1) * 8);
    const float PI = 3.14159265358979323846f;
    for (int i = 0; i <= stacks; ++i)
    {
        float t = float(i) / float(stacks); // [0,1]
        float phi = t * PI;                 // [0,PI]
        for (int j = 0; j <= slices; ++j)
        {
            float s = float(j) / float(slices); // [0,1]
            float theta = s * 2.0f * PI;        // [0,2PI]
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            // Position (unit sphere)
            glm::vec3 pos(x, y, z);
            // Normal (same as pos for unit sphere)
            glm::vec3 n = glm::normalize(pos);
            // UV
            glm::vec2 uv(s, 1.0f - t);
            // pack vertex
            v.push_back(pos.x);
            v.push_back(pos.y);
            v.push_back(pos.z);
            v.push_back(n.x);
            v.push_back(n.y);
            v.push_back(n.z);
            v.push_back(uv.x);
            v.push_back(uv.y);
        }
    }
    // Indices (two triangles per quad)
    const int ring = slices + 1;
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int a = i * ring + j;
            int b = (i + 1) * ring + j;
            int c = (i + 1) * ring + (j + 1);
            int d = i * ring + (j + 1);
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(c);
            idx.push_back(a);
            idx.push_back(c);
            idx.push_back(d);
        }
    }
    return buildIndexedMesh(v, idx);
}
int main()
{
    Window mainWindow(WIDTH, HEIGHT, 3, 3);
    if (mainWindow.initialise() != 0)
    {
        std::cerr << "Failed to initialize window.\n";
        return 1;
    }
    GLFWwindow *w = mainWindow.getWindow();
    glfwSetWindowTitle(w, "Lab 2 - Physically Based Rendering (PBR)");
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // PBR shader
    Shader pbr;
    pbr.CreateFromFiles("Shaders/Lab2/pbr.vert", "Shaders/Lab2/pbr.frag");
    GLuint uModel = pbr.GetUniformLocation("uModel");
    GLuint uView = pbr.GetUniformLocation("uView");
    GLuint uProj = pbr.GetUniformLocation("uProj");
    GLuint uCamPos = pbr.GetUniformLocation("uCamPos");
    GLuint uAlbedo = pbr.GetUniformLocation("uAlbedo");
    GLuint uMetallic = pbr.GetUniformLocation("uMetallic");
    GLuint uRoughness = pbr.GetUniformLocation("uRoughness");
    GLuint uAO = pbr.GetUniformLocation("uAO");
    auto srgbToLinear = [](glm::vec3 c)
    {
        return glm::vec3(std::pow(c.x, 2.2f), std::pow(c.y, 2.2f), std::pow(c.z, 2.2f));
    };
    GLuint uLightCount = pbr.GetUniformLocation("uLightCount");
    // arrays: "uLightPos[0]" etc
    GLuint uLightPos0 = pbr.GetUniformLocation("uLightPos[0]");
    GLuint uLightCol0 = pbr.GetUniformLocation("uLightColor[0]");
    // Build meshes
    MeshGL sphere = makeSphere(32, 64);
    glm::vec3 camPos(0.0f, 0.0f, 13.5f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    // metalic
    // float metallic = 0.0f;
    // float roughness = 0.35f;
    
    // copper
    // float metallic = 1.0f;
    // float roughness = 0.35f;
    // glm::vec3 albedo(0.955f, 0.637f, 0.538f);

    //bronze
    // float metallic = 1.0f;de

    //brass
    float metallic = 1.0f;
    float roughness = 0.05f;
    glm::vec3 albedo(0.829f, 0.565f, 0.224f);
    float ao = 1.0f;
    // Helper clamp
    auto clampf = [](float x, float lo, float hi)
    { return std::max(lo, std::min(hi, x)); };
    while (!glfwWindowShouldClose(w))
    {
        glfwPollEvents();
        // --- One-press-per-action input (avoids fast auto-repeat when key is held)
        static int prev[GLFW_KEY_LAST + 1] = {0};
        auto pressedOnce = [&](int key) -> bool
        {
            int cur = glfwGetKey(w, key);
            bool fired = (cur == GLFW_PRESS && prev[key] != GLFW_PRESS);
            prev[key] = cur;
            return fired;
        };
        if (pressedOnce(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(w, GLFW_TRUE);
        const float step = 0.05f;
        // cycle albedo presets to see material response better
        if (pressedOnce(GLFW_KEY_C))
        {
            static int preset = 0;
            preset = (preset + 1) % 4;
            if (preset == 0)
                albedo = glm::vec3(0.95f, 0.00f, 0.00f); // red
            if (preset == 1)
                albedo = glm::vec3(0.00f, 0.65f, 1.00f); // blue-ish
            if (preset == 2)
                albedo = glm::vec3(0.95f, 0.85f, 0.30f); // gold-ish base
            if (preset == 3)
                albedo = glm::vec3(0.90f, 0.90f, 0.90f); // neutral
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pbr.UseShader();
        glUniform1i(uLightCount, 4);
        glUniformMatrix4fv(uView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(uProj, 1, GL_FALSE, &proj[0][0]);
        glUniform3f(uCamPos, camPos.x, camPos.y, camPos.z);
        float t = static_cast<float>(glfwGetTime());
        // Circular motion around the grid
        glm::vec3 lightPos0(6.0f * std::cos(t * 0.7f), 4.0f, 6.0f * std::sin(t * 0.7f));
        glm::vec3 lightPos1(6.0f * std::cos(t * 0.7f + 3.14159f), 4.0f, 6.0f * std::sin(t * 0.7f + 3.14159f));
        glm::vec3 lightPos2(0.0f, 6.0f + 1.5f * std::sin(t * 1.3f), 0.0f);
        glm::vec3 lightPos3(8.0f * std::cos(t * 0.35f), -2.5f, 8.0f * std::sin(t * 0.35f));
        // Intensities (HDR-ish)
        glm::vec3 lightCol0(80.0f, 80.0f, 80.0f);
        glm::vec3 lightCol1(50.0f, 50.0f, 50.0f);
        glm::vec3 lightCol2(30.0f, 30.0f, 30.0f);
        glm::vec3 lightCol3(60.0f, 60.0f, 60.0f);
        glUniform3fv(pbr.GetUniformLocation("uLightPos[0]"), 1, &lightPos0[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightPos[1]"), 1, &lightPos1[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightPos[2]"), 1, &lightPos2[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightPos[3]"), 1, &lightPos3[0]);
        // Upload colors
        glUniform3fv(pbr.GetUniformLocation("uLightColor[0]"), 1, &lightCol0[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightColor[1]"), 1, &lightCol1[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightColor[2]"), 1, &lightCol2[0]);
        glUniform3fv(pbr.GetUniformLocation("uLightColor[3]"), 1, &lightCol3[0]);
        // ---- Material constants
        glm::vec3 albedoSRGB = albedo;
        glm::vec3 albedoLin = srgbToLinear(albedoSRGB);
        glUniform3f(uAlbedo, albedoLin.x, albedoLin.y, albedoLin.z);
        glUniform1f(uAO, ao);
        // ---- Grid parameters
        const int rows = 5;         // metallic
        const int cols = 5;         // roughness
        const float spacing = 2.2f; // distance between spheres
        const float startX = -0.5f * (cols - 1) * spacing;
        const float startY = -0.5f * (rows - 1) * spacing;
        // Draw spheres
        glBindVertexArray(sphere.VAO);
        for (int r = 0; r < rows; r++)
        {
            // metallic from 0 -> 1 across rows
            float m = float(r) / float(rows - 1); // 0..1
            glUniform1f(uMetallic, m);
            for (int c = 0; c < cols; c++)
            {
                // roughness from 0.05 -> 1 across columns
                float t = float(c) / float(cols - 1);     // 0..1
                float rough = 0.05f + t * (1.0f - 0.05f); // 0.05..1
                glUniform1f(uRoughness, rough);
                float x = startX + c * spacing;
                float y = startY + (rows - 1 - r) * spacing; // top row = r=0
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3(x, y, 0.0f));
                model = glm::scale(model, glm::vec3(1.0f)); // optional
                glUniformMatrix4fv(uModel, 1, GL_FALSE, &model[0][0]);
                glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        glBindVertexArray(0);   
        glUseProgram(0);
        glfwSwapBuffers(w);
    }
    sphere.destroy();
    return 0;
}