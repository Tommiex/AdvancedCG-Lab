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

/*
Lab 1 (Advanced Computer Graphics) — Back to Basics: Polygons
Goals:
  1) Understand the minimal OpenGL draw pipeline (VAO/VBO/EBO).
  2) Draw: Triangle -> Quad -> N-gon (triangle fan indices).
  3) Practice shader loading + uniform usage (uMVP, uColor).

Controls:
  - ESC: close window
  - 1: draw triangle
  - 2: draw quad
  - 3: draw regular N-gon (default N=8)
  - UP/DOWN: increase/decrease N (clamped 3..64) when in N-gon mode
*/

static const GLint WIDTH = 900, HEIGHT = 650;

enum class Mode { TRIANGLE=1, QUAD=2, NGON=3 };

struct MeshGL
{
    GLuint VAO=0, VBO=0, EBO=0;
    GLsizei indexCount=0;

    void destroy()
    {
        if(EBO) glDeleteBuffers(1, &EBO);
        if(VBO) glDeleteBuffers(1, &VBO);
        if(VAO) glDeleteVertexArrays(1, &VAO);
        VAO=VBO=EBO=0;
        indexCount=0;
    }
};

static MeshGL buildIndexedMesh(const std::vector<float>& positions, const std::vector<unsigned int>& indices)
{
    MeshGL m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size()*sizeof(float), positions.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // aPos (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    m.indexCount = static_cast<GLsizei>(indices.size());
    return m;
}

static MeshGL makeTriangle()
{
    // CCW triangle
    std::vector<float> pos = {
        -0.6f, -0.5f, 0.0f,
         0.6f, -0.5f, 0.0f,
         0.0f,  0.6f, 0.0f
    };
    std::vector<unsigned int> idx = { 0, 1, 2 };
    return buildIndexedMesh(pos, idx);
}

static MeshGL makeQuad()
{
    // Two triangles: (0,1,2) + (0,2,3)
    std::vector<float> pos = {
        -0.6f, -0.5f, 0.0f,  // 0
         0.6f, -0.5f, 0.0f,  // 1
         0.6f,  0.5f, 0.0f,  // 2
        -0.6f,  0.5f, 0.0f   // 3
    };
    std::vector<unsigned int> idx = { 0, 1, 2,  0, 2, 3 };
    return buildIndexedMesh(pos, idx);
}

static MeshGL makeNGon(int N)
{
    // Regular N-gon centered at origin, radius r.
    // We triangulate using a triangle fan from the center vertex.
    const float r = 0.65f;
    const float PI = 3.14159265358979323846f;

    // positions: [center] + N ring vertices
    std::vector<float> pos;
    pos.reserve((N+1)*3);

    // center
    pos.push_back(0.0f); pos.push_back(0.0f); pos.push_back(0.0f);

    for(int i=0;i<N;i++)
    {
        float a = (2.0f*PI*i)/N;
        pos.push_back(r*std::cos(a));
        pos.push_back(r*std::sin(a));
        pos.push_back(0.0f);
    }

    // indices for fan: (0, i, i+1) for i=1..N-1 and wrap
    std::vector<unsigned int> idx;
    idx.reserve(N*3);
    for(int i=1;i<=N;i++)
    {
        unsigned int a = 0;
        unsigned int b = i;
        unsigned int c = (i % N) + 1; // wrap to 1
        idx.push_back(a); idx.push_back(b); idx.push_back(c);
    }

    return buildIndexedMesh(pos, idx);
}

static void setWindowTitle(GLFWwindow* w, Mode mode, int N)
{
    std::string title = "Lab 1 - Polygons | Mode: ";
    if(mode==Mode::TRIANGLE) title += "Triangle";
    else if(mode==Mode::QUAD) title += "Quad";
    else title += "N-gon (N=" + std::to_string(N) + ")";
    glfwSetWindowTitle(w, title.c_str());
}

int main()
{
    Window mainWindow(WIDTH, HEIGHT, 3, 3);
    if (mainWindow.initialise() != 0)
    {
        std::cerr << "Failed to initialize window.\n";
        return 1;
    }

    // Basic shader (solid color + MVP)
    Shader shader;
    shader.CreateFromFiles("Shaders/Lab1/basic.vert", "Shaders/Lab1/basic.frag");

    GLuint uMVP = shader.GetUniformLocation("uMVP");
    GLuint uColor = shader.GetUniformLocation("uColor");

    // Build initial meshes
    MeshGL tri = makeTriangle();
    MeshGL quad = makeQuad();
    
    int N = 8;
    MeshGL ngon = makeNGon(N);

    Mode mode = Mode::TRIANGLE;
    GLFWwindow* w = mainWindow.getWindow();
    setWindowTitle(w, mode, N);

    // Simple camera-like transform (just MVP for 2D-ish viewing)
    glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);

    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

    while (!glfwWindowShouldClose(w))
    {
        glfwPollEvents();

        // --- One-press-per-action input (avoids fast auto-repeat when key is held)
        static int prev[GLFW_KEY_LAST + 1] = {0};
        auto pressedOnce = [&](int key) -> bool {
            int cur = glfwGetKey(w, key);
            bool fired = (cur == GLFW_PRESS && prev[key] != GLFW_PRESS);
            prev[key] = cur;
            return fired;
        };

        if (pressedOnce(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(w, GLFW_TRUE);

        // Mode switches (one press)
        if (pressedOnce(GLFW_KEY_1)) { mode = Mode::TRIANGLE; setWindowTitle(w, mode, N); }
        if (pressedOnce(GLFW_KEY_2)) { mode = Mode::QUAD;     setWindowTitle(w, mode, N); }
        if (pressedOnce(GLFW_KEY_3)) { mode = Mode::NGON;     setWindowTitle(w, mode, N); }


        // Adjust N only in NGON mode (simple key polling; good enough for lab)
        if (mode == Mode::NGON)
        {
            bool rebuild = false;
            if (pressedOnce(GLFW_KEY_UP))   { N = std::min(64, N+1); rebuild = true; }
            if (pressedOnce(GLFW_KEY_DOWN)) { N = std::max(3,  N-1); rebuild = true; }
            if (rebuild)
            {
                ngon.destroy();
                ngon = makeNGon(N);
                setWindowTitle(w, mode, N);
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.UseShader();

        // A tiny rotation just to show MVP is working
        float t = static_cast<float>(glfwGetTime());
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), 0.15f*t, glm::vec3(0,0,1));
        glm::mat4 mvp = proj * view * model;
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, &mvp[0][0]);

        // Choose color by mode
        glm::vec3 color(0.2f, 0.9f, 0.7f);
        if (mode == Mode::TRIANGLE) color = glm::vec3(0.95f, 0.55f, 0.20f);
        if (mode == Mode::QUAD)     color = glm::vec3(0.35f, 0.70f, 1.00f);
        if (mode == Mode::NGON)     color = glm::vec3(0.75f, 0.85f, 0.30f);
        glUniform3f(uColor, color.x, color.y, color.z);

        // Draw
        const MeshGL* drawMesh = &tri;
        if (mode == Mode::QUAD) drawMesh = &quad;
        if (mode == Mode::NGON) drawMesh = &ngon;

        glBindVertexArray(drawMesh->VAO);
        glDrawElements(GL_TRIANGLES, drawMesh->indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glUseProgram(0);

        glfwSwapBuffers(w);
    }

    tri.destroy();
    quad.destroy();
    ngon.destroy();

    return 0;
}
