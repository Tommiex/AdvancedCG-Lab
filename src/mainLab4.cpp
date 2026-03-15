#include <iostream>
#include <vector>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Libs/Window.h"
#include "Libs/Shader.h"

const GLint WIDTH = 1280, HEIGHT = 720;

struct MeshGL
{
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;
    void destroy()
    {
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }
};

static MeshGL buildMesh(const std::vector<float> &v, const std::vector<unsigned> &idx)
{
    MeshGL m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);
    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void *)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void *)24);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    m.indexCount = (GLsizei)idx.size();
    return m;
}

static MeshGL makeCube()
{
    const glm::vec3 N[] = {{0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}};
    const glm::vec3 U[] = {{1, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
    const glm::vec3 V[] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    std::vector<float> verts;
    std::vector<unsigned> idx;
    for (int f = 0; f < 6; f++)
    {
        glm::vec3 n = N[f], u = U[f], v = V[f];
        glm::vec3 c = n * 0.5f;
        glm::vec3 p[] = {c - u * 0.5f - v * 0.5f, c + u * 0.5f - v * 0.5f, c + u * 0.5f + v * 0.5f, c - u * 0.5f + v * 0.5f};
        for (int i = 0; i < 4; i++)
        {
            verts.insert(verts.end(), {p[i].x, p[i].y, p[i].z, n.x, n.y, n.z, (i == 1 || i == 2) ? 1.f : 0.f, (i >= 2) ? 1.f : 0.f});
        }
        unsigned b = f * 4;
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    return buildMesh(verts, idx);
}

struct LinesGL
{
    GLuint VAO = 0, VBO = 0;
    GLsizei vertCount = 0;
    void destroy()
    {
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }
};

static LinesGL makeAxes(float s = 0.7f)
{
    float d[] = {
        0, 0, 0, 1, 0, 0,
        s, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0,
        0, s, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 1,
        0, 0, s, 0, 0, 1};
    LinesGL l;
    l.vertCount = 6;
    glGenVertexArrays(1, &l.VAO);
    glBindVertexArray(l.VAO);
    glGenBuffers(1, &l.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, l.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(d), d, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void *)12);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return l;
}

struct Node
{
    int parent = -1;
    glm::mat4 localFrame{1.f}, worldFrame{1.f}, drawFromFrame{1.f};
    glm::vec3 color{1.f};
};

static void updateWorldFrames(std::vector<Node> &nodes)
{
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].parent < 0)
        {
            nodes[i].worldFrame = nodes[i].localFrame;
        }
        else
        {
            nodes[i].worldFrame = nodes[nodes[i].parent].worldFrame * nodes[i].localFrame;
        }
    }
}

static glm::vec3 projectOntoPlane(const glm::vec3& v, const glm::vec3& axis)
{
glm::vec3 ax = glm::normalize(axis);
return v - glm::dot(v, ax) * ax;
}
static float signedAngleAroundAxis(const glm::vec3& a, const glm::vec3& b, const glm::vec3& axis)
{
glm::vec3 aa = glm::normalize(a);
glm::vec3 bb = glm::normalize(b);
glm::vec3 ax = glm::normalize(axis);
float cosang = glm::clamp(glm::dot(aa, bb), -1.0f, 1.0f);
float ang = std::acos(cosang);
// sign via triple product
float s = glm::dot(ax, glm::cross(aa, bb));
if (s < 0.0f) ang = -ang;
return ang;
}
int main()
{
    Window window(WIDTH, HEIGHT, 3, 3);
    if (window.initialise() != 0)
    {
        return -1;
    }
    GLFWwindow *w = window.getWindow();
    glEnable(GL_DEPTH_TEST);

    Shader meshSh, lineSh;
    meshSh.CreateFromFiles("Shaders/Lab3/mesh.vert", "Shaders/Lab3/mesh.frag");
    lineSh.CreateFromFiles("Shaders/Lab3/line.vert", "Shaders/Lab3/line.frag");

    GLuint uModel_m = meshSh.GetUniformLocation("uModel");
    GLuint uView_m = meshSh.GetUniformLocation("uView");
    GLuint uProj_m = meshSh.GetUniformLocation("uProj");
    GLuint uColor_m = meshSh.GetUniformLocation("uColor");
    GLuint uLight = meshSh.GetUniformLocation("uLightDir");
    GLuint uModel_l = lineSh.GetUniformLocation("uModel");
    GLuint uView_l = lineSh.GetUniformLocation("uView");
    GLuint uProj_l = lineSh.GetUniformLocation("uProj");

    MeshGL cube = makeCube();
    LinesGL axes = makeAxes();
    // MeshGL targetMesh = makeCube();
    glm::vec3 targetPos = glm::vec3(1.0f, 1.0f, 1.0f);

    glm::mat4 view = glm::lookAt(glm::vec3(3, 3, 6), glm::vec3(0, 1.5f, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), float(WIDTH) / HEIGHT, 0.1f, 100.f);
    glm::vec3 lightDir = glm::normalize(glm::vec3(1, 2, 1.5f));
    glClearColor(0.08f, 0.08f, 0.10f, 1);

    std::vector<Node> nodes(5);
    nodes[0].parent = -1;
    nodes[1].parent = 0;
    nodes[2].parent = 1;
    nodes[3].parent = 2;
    nodes[4].parent = 3;
    nodes[0].color = {0.5f, 0.5f, 0.5f};
    nodes[1].color = {0.2f, 0.6f, 0.2f};
    nodes[2].color = {0.2f, 0.4f, 0.8f};
    nodes[3].color = {0.8f, 0.6f, 0.1f};
    nodes[4].color = {0.8f, 0.2f, 0.2f};

    float baseH = 0.24f, upperLen = 1.20f, foreLen = 0.80f, handLen = 0.45f, fingerLen = 0.30f;

    auto T = [](glm::vec3 v)
    {
        return glm::translate(glm::mat4(1.f), v);
    };
    auto S = [](glm::vec3 v)
    {
        return glm::scale(glm::mat4(1.f), v);
    };

    nodes[0].drawFromFrame = T({0, baseH * .5f, 0}) * S({0.8f, baseH, 0.8f});
    nodes[1].drawFromFrame = T({0, upperLen * .5f, 0}) * S({0.22f, upperLen, 0.22f});
    nodes[2].drawFromFrame = T({0, foreLen * .5f, 0}) * S({0.18f, foreLen, 0.18f});
    nodes[3].drawFromFrame = T({0, handLen * .5f, 0}) * S({0.25f, handLen, 0.12f});
    nodes[4].drawFromFrame = T({0, fingerLen * .5f, 0}) * S({0.08f, fingerLen, 0.08f});

    float shoulderYaw = 0;
    float shoulderPitch = 0;
    float elbowPitch = glm::radians(40.f);
    float wristTwist = 0;
    float fingerBend = glm::radians(20.f);

    struct Limits
    {
        float min, max;
    };
    Limits limSY = {glm::radians(-180.f), glm::radians(180.f)};
    Limits limSP = {glm::radians(-90.f), glm::radians(90.f)};
    Limits limEB = {glm::radians(-10.f), glm::radians(150.f)};
    Limits limWT = {glm::radians(-90.f), glm::radians(90.f)};
    Limits limFB = {glm::radians(-10.f), glm::radians(90.f)};

    auto R = [](float a, glm::vec3 ax)
    {
        return glm::rotate(glm::mat4(1.f), a, ax);
    };

    auto rebuildFK = [&]()
    {
        shoulderYaw = glm::clamp(shoulderYaw, limSY.min, limSY.max);
        shoulderPitch = glm::clamp(shoulderPitch, limSP.min, limSP.max);
        elbowPitch = glm::clamp(elbowPitch, limEB.min, limEB.max);
        wristTwist = glm::clamp(wristTwist, limWT.min, limWT.max);
        fingerBend = glm::clamp(fingerBend, limFB.min, limFB.max);

        nodes[0].localFrame = glm::mat4(1.f);
        nodes[1].localFrame = T({0, baseH, 0}) * R(shoulderYaw, {0, 1, 0}) * R(shoulderPitch, {1, 0, 0});
        nodes[2].localFrame = T({0, upperLen, 0}) * R(elbowPitch, {1, 0, 0});
        nodes[3].localFrame = T({0, foreLen, 0}) * R(wristTwist, {0, 1, 0});
        nodes[4].localFrame = T({0, handLen, 0}) * R(fingerBend, {1, 0, 0});
        updateWorldFrames(nodes);
    };
    rebuildFK();

    float lastTime = (float)glfwGetTime();
    const float speed = 1.5f;

    

    while (!glfwWindowShouldClose(w))
    {
        float now = (float)glfwGetTime(), dt = now - lastTime;
        lastTime = now;
        glfwPollEvents();
        

        float moveSpeed = 1.2f; // units/sec

        if (glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS)
            targetPos.x -= moveSpeed * dt;
        if (glfwGetKey(w, GLFW_KEY_L) == GLFW_PRESS)
            targetPos.x += moveSpeed * dt;
        if (glfwGetKey(w, GLFW_KEY_I) == GLFW_PRESS)
            targetPos.y += moveSpeed * dt;
        if (glfwGetKey(w, GLFW_KEY_K) == GLFW_PRESS)
            targetPos.y -= moveSpeed * dt;
        if (glfwGetKey(w, GLFW_KEY_U) == GLFW_PRESS)
            targetPos.z -= moveSpeed * dt;
        if (glfwGetKey(w, GLFW_KEY_O) == GLFW_PRESS)
            targetPos.z += moveSpeed * dt;
        // keep it visible
        targetPos.x = glm::clamp(targetPos.x, -2.0f, 2.0f);
        targetPos.y = glm::clamp(targetPos.y, 0.2f, 2.5f);
        targetPos.z = glm::clamp(targetPos.z, -2.0f, 2.0f);


        const float gain = 0.75f;
        const float maxDelta = glm::radians(4.0f);
        for (int iter = 0; iter < 10; iter++) { // Repeat for N iterations [cite: 220]
    rebuildFK(); // Update world frames to get current positions [cite: 194, 213]
    
    // Get end-effector position (Hand is nodes[4]) [cite: 71, 221]
    glm::vec3 endPos = glm::vec3(nodes[4].worldFrame[3]); 

    // Iterate joints from Hand back to Shoulder [cite: 222, 223]
    for (int j = 3; j >= 1; j--) {
        glm::vec3 jointPos = glm::vec3(nodes[j].worldFrame[3]); 
        
        glm::vec3 localAxis = (j == 1) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0); 
        
        glm::vec3 axisW = glm::normalize(glm::mat3(nodes[j].worldFrame) * localAxis);

        glm::vec3 vEnd = endPos - jointPos;
        glm::vec3 vTar = targetPos - jointPos;

        glm::vec3 pEnd = projectOntoPlane(vEnd, axisW);
        glm::vec3 pTar = projectOntoPlane(vTar, axisW);

        if (glm::length(pEnd) > 1e-5f && glm::length(pTar) > 1e-5f){
            float d = signedAngleAroundAxis(pEnd, pTar, axisW); 
            

            d = glm::clamp(d * gain, -maxDelta, maxDelta);
            if (j == 1) shoulderYaw += d;
            else if (j == 2) elbowPitch += d;

        }
    }
}
        rebuildFK();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        meshSh.UseShader();
        glm::mat4 targetM = glm::translate(glm::mat4(1.0f), targetPos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.18f));
        glUniformMatrix4fv(uModel_m, 1, GL_FALSE, glm::value_ptr(targetM));
        glUniform3f(uColor_m, 1.0f, 0.1f, 0.9f);
        glBindVertexArray(cube.VAO);    
        glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, 0);

        glUniformMatrix4fv(uView_m, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_m, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3f(uLight, lightDir.x, lightDir.y, lightDir.z);
        glBindVertexArray(cube.VAO);
        for (auto &n : nodes)
        {
            glm::mat4 model = n.worldFrame * n.drawFromFrame;
            glUniformMatrix4fv(uModel_m, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(uColor_m, n.color.x, n.color.y, n.color.z);
            glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, 0);
        }

        lineSh.UseShader();
        glUniformMatrix4fv(uView_l, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_l, 1, GL_FALSE, glm::value_ptr(proj));
        glBindVertexArray(axes.VAO);
        for (auto &n : nodes)
        {
            glUniformMatrix4fv(uModel_l, 1, GL_FALSE, glm::value_ptr(n.worldFrame));
            glDrawArrays(GL_LINES, 0, axes.vertCount);
        }
        glBindVertexArray(0);
        glfwSwapBuffers(w);
    }
    cube.destroy();
    axes.destroy();
    return 0;
}
