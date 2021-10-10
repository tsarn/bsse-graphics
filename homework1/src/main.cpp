#include <iostream>
#include <stdexcept>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

#include "gl_objects.h"
#include "shaders.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

static GLFWwindow* window;

struct GraphData {
    std::vector<glm::vec2> coords;
    std::vector<float> values;
    std::vector<int> indices;
};

GraphData generateGraph(
    float xmin, float xmax,
    float ymin, float ymax,
    float step
) {
    GraphData result;
    int n = (int)((xmax - xmin) / step);
    int m = (int)((ymax - ymin) / step);

    for (int i = 0; i < n; ++i) {
        float x = i * step + xmin;
        for (int j = 0; j < m; ++j) {
            float y = j * step + ymin;

            result.coords.emplace_back(x, y);
            result.values.emplace_back(.0f);
        }
    }

    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < m - 1; ++j) {
            std::array<int, 4> idx = {
                i * m + j,
                (i + 1) * m + j,
                (i + 1) * m + j + 1,
                i * m + j + 1
            };

            result.indices.push_back(idx[0]);
            result.indices.push_back(idx[1]);
            result.indices.push_back(idx[3]);
            result.indices.push_back(idx[1]);
            result.indices.push_back(idx[2]);
            result.indices.push_back(idx[3]);
        }
    }

    return result;
}

void updateGraph(GraphData& graph) {
    float t = glfwGetTime();
    for (int i = 0; i < graph.coords.size(); ++i) {
        float x = graph.coords[i].x;
        float y = graph.coords[i].y;

        graph.values[i] = sin(x + 3 * t) + cos(y + t);
    }
}

struct IsolinesData {
    std::vector<glm::vec3> coords;
    std::vector<int> indices;
};

void addIsoline(const GraphData& graph, IsolinesData& isolines, float value) {
    float eps = 0.01;

    std::unordered_map<std::uint64_t, int> idxs;

    auto add = [&](glm::vec3 coords, unsigned idx0, unsigned idx1) {
        if (idx0 > idx1) std::swap(idx0, idx1);
        std::uint64_t idx = idx0;
        idx <<= 32;
        idx |= idx1;

        if (!idxs.contains(idx)) {
            idxs[idx] = isolines.coords.size();
            isolines.coords.push_back(coords);
        }

        isolines.indices.push_back(idxs[idx]);
    };

    for (int i = 0; i < graph.indices.size(); i += 3) {
        auto i0 = graph.indices[i];
        auto i1 = graph.indices[i + 1];
        auto i2 = graph.indices[i + 2];

        auto p0 = graph.coords[i0];
        auto p1 = graph.coords[i1];
        auto p2 = graph.coords[i2];

        auto t0 = graph.values[i0] + eps;
        auto t1 = graph.values[i1] + eps;
        auto t2 = graph.values[i2] + eps;

        unsigned mask = (t0 > value) | ((t1 > value) << 1) | ((t2 > value) << 2);

        if (mask == 0 || mask == 7) {
            continue;
        }

        if (mask == 3 || mask == 5 || mask == 6) {
            mask ^= 7;
        }

        if (mask == 1) {
            float a = (value - t1) / (t0 - t1);
            add(glm::mix(glm::vec3(p0, t0), glm::vec3(p1, t1), 1.0 - a), i0, i1);
            float b = (value - t2) / (t0 - t2);
            add(glm::mix(glm::vec3(p0, t0), glm::vec3(p2, t2), 1.0 - b), i0, i2);
        } else if (mask == 2) {
            float a = (value - t0) / (t1 - t0);
            add(glm::mix(glm::vec3(p1, t1), glm::vec3(p0, t0), 1.0 - a), i0, i1);
            float b = (value - t2) / (t1 - t2);
            add(glm::mix(glm::vec3(p1, t1), glm::vec3(p2, t2), 1.0 - b), i1, i2);
        } else {
            float a = (value - t1) / (t2 - t1);
            add(glm::mix(glm::vec3(p2, t2), glm::vec3(p1, t1), 1.0 - a), i1, i2);
            float b = (value - t0) / (t2 - t0);
            add(glm::mix(glm::vec3(p2, t2), glm::vec3(p0, t0), 1.0 - b), i0, i2);
        }
    }
}

void initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 32);

    window = glfwCreateWindow(800, 600, "homework1", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error{"glfwCreateWindow failed"};
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error{"gladLoadGLLoader failed"};
    }    

    glViewport(0, 0, 800, 600);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    });
}

void loop() {
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    VertexArray graphVAO, isolinesVAO;
    Buffer coordsVBO, valuesVBO, graphEBO;
    Buffer isolinesVBO, isolinesEBO;

    Program graphShader = createProgram(createShader(GL_VERTEX_SHADER, graphVS), createShader(GL_FRAGMENT_SHADER, graphFS));
    Program isolinesShader = createProgram(createShader(GL_VERTEX_SHADER, isolinesVS), createShader(GL_FRAGMENT_SHADER, isolinesFS));

    auto Lview1 = glGetUniformLocation(graphShader, "view");
    auto Lprojection1 = glGetUniformLocation(graphShader, "projection");

    auto Lview2 = glGetUniformLocation(isolinesShader, "view");
    auto Lprojection2 = glGetUniformLocation(isolinesShader, "projection");

    float xmin = -10.0f;
    float xmax = 10.0f;
    float ymin = -10.0f;
    float ymax = 10.0f;
    float step = 0.1f;
    float zmin = -3.0f;
    float zmax = 3.0f;
    float zstep = 0.25f;
    float cameraDist = 15.0f;
    float cameraHeight = 10.0f;
    float cameraAngle = 0.0f;
    float fov = glm::radians(70.0f);
    int width, height;

    GraphData graph;

    auto regenerate = [&] {
        graph = generateGraph(
                xmin, xmax,
                ymin, ymax,
                step
        );

        glBindVertexArray(graphVAO);

        glBindBuffer(GL_ARRAY_BUFFER, coordsVBO);
        glBufferData(GL_ARRAY_BUFFER, graph.coords.size() * sizeof(glm::vec2), graph.coords.data(), GL_STATIC_DRAW);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, graph.indices.size() * sizeof(int), graph.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    };

    glBindVertexArray(graphVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, graphEBO);
    glBindBuffer(GL_ARRAY_BUFFER, coordsVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, valuesVBO);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);

    glBindVertexArray(isolinesVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, isolinesEBO);
    glBindBuffer(GL_ARRAY_BUFFER, isolinesVBO);
	glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    float lastTime = .0f;

    regenerate();

    while(!glfwWindowShouldClose(window)) {
        glBindVertexArray(0);
        glfwGetWindowSize(window, &width, &height);

        // Process input

        float dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            cameraAngle += dt * 3.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            cameraAngle -= dt * 3.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            cameraHeight -= dt * 10.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            cameraHeight += dt * 10.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            step += dt;
            regenerate();
        }

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
            step = glm::max(0.1f, step - dt);
            regenerate();
        }

        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
            zstep += dt;
        }

        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
            zstep = glm::max(0.1f, zstep - dt);
        }


        // Update graph data
        updateGraph(graph);
        glBindBuffer(GL_ARRAY_BUFFER, valuesVBO);
        glBufferData(GL_ARRAY_BUFFER, graph.values.size() * sizeof(float), graph.values.data(), GL_STREAM_DRAW);

        // Update isolines data
        IsolinesData isolines;
        for (float value = zmin; value <= zmax; value += zstep) {
            addIsoline(graph, isolines, value);
        }

        glBindBuffer(GL_ARRAY_BUFFER, isolinesVBO);
        glBufferData(GL_ARRAY_BUFFER, isolines.coords.size() * sizeof(glm::vec3), isolines.coords.data(), GL_STREAM_DRAW);

        glBindVertexArray(isolinesVAO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, isolines.indices.size() * sizeof(int), isolines.indices.data(), GL_STREAM_DRAW);
        glBindVertexArray(0);

//        std::cerr << "gv=" << graph.coords.size() << " gi=" << graph.indices.size() << " lv=" << isolines.coords.size() << " li=" << isolines.indices.size() << "\n";

        // Set camera
        glm::vec3 cameraPosition;
        cameraPosition.y = cameraHeight;
        cameraPosition.x = cos(cameraAngle) * cameraDist;
        cameraPosition.z = sin(cameraAngle) * cameraDist;

        glm::mat4 view = glm::lookAt(cameraPosition, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
        glm::mat4 projection = glm::perspective(fov, 1.0f * width / height, 0.1f, 100.0f);

        // Clear
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw graph

        glBindVertexArray(graphVAO);
        glUseProgram(graphShader);
        glUniformMatrix4fv(Lview1, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(Lprojection1, 1, GL_FALSE, glm::value_ptr(projection));
        glDrawElements(GL_TRIANGLES, graph.indices.size(), GL_UNSIGNED_INT, nullptr);

        // Draw isolines

        glBindVertexArray(isolinesVAO);
        glUseProgram(isolinesShader);
        glUniformMatrix4fv(Lview2, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(Lprojection2, 1, GL_FALSE, glm::value_ptr(projection));
        glDrawElements(GL_LINES, isolines.indices.size(), GL_UNSIGNED_INT, nullptr);

        // Swap

        glfwPollEvents();    
        glfwSwapBuffers(window);
    }
}

int main() {
    try {
        initialize();
        loop();
    } catch (...) {
        glfwTerminate();
        throw;
    }

    glfwTerminate();
    return 0;
}
