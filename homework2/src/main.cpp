#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <fstream>

#include "gl_objects.h"
#include "shaders.h"
#include "camera.h"
#include "scene.h"
#include "render.h"

#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

static GLFWwindow *window;

void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

void initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 32);

    window = glfwCreateWindow(800, 600, "homework2", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error{"glfwCreateWindow failed"};
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error{"gladLoadGLLoader failed"};
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
}

void loop() {
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Camera camera;
    auto scene = [] {
        std::ifstream mtlIn{"./sponza/sponza.mtl"};
        auto materials = loadMTL(mtlIn);
        std::ifstream objIn{"./sponza/sponza.obj"};
        auto scene = loadOBJ(objIn, materials);
        DrawableScene result;
        result.init(scene);
        return result;
    }();
    std::cerr << "Loaded scene\n";

    int width = 800, height = 600;
    double xpos = 0.0, ypos = 0.0;
    float lastTime = 0.0;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glm::vec3 attenuation{1.0, 0.002, 0.00002};

    std::vector<Light> lights;
    lights.push_back(Light {
        .position = glm::normalize(glm::vec3(0.2, 1.0, 0.2)),
        .diffuse = glm::vec3(0.6, 0.6, 0.6),
        .specular = glm::vec3(0.0, 0.0, 0.0),
        .attenuation = glm::vec3(1.0, 0.0, 0.00),
        .directional = true
    });

    lights.push_back(Light {
        .position = glm::vec3(-1200, 200, -400),
        .diffuse = glm::vec3(1.0, 0.0, 0.0),
        .specular = glm::vec3(1.0, 0.0, 0.0),
        .attenuation = attenuation,
        .directional = false
    });

    lights.push_back(Light {
        .position = glm::vec3(1100, 200, -450),
        .diffuse = glm::vec3(0.0, 1.0, 0.0),
        .specular = glm::vec3(0.0, 1.0, 0.0),
        .attenuation = attenuation,
        .directional = false
    });

    lights.push_back(Light {
        .position = glm::vec3(1130, 200, 410),
        .diffuse = glm::vec3(0.0, 0.0, 1.0),
        .specular = glm::vec3(0.0, 0.0, 1.0),
        .attenuation = attenuation,
        .directional = false
    });

    lights.push_back(Light {
        .position = glm::vec3(-1200, 200, 400),
        .diffuse = glm::vec3(1.0, 1.0, 0.0),
        .specular = glm::vec3(1.0, 1.0, 0.0),
        .attenuation = attenuation,
        .directional = false
    });

    scene.calculateShadows(lights[0]);

    while (!glfwWindowShouldClose(window)) {
        glBindVertexArray(0);
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glfwGetCursorPos(window, &xpos, &ypos);
        glfwSetCursorPos(window, width / 2.0, height / 2.0);
        xpos = xpos / (width * 0.5) - 1.0;
        ypos = ypos / (height * 0.5) - 1.0;

        camera.aspectRatio = (float)width / (float)height;
        camera.look({-xpos, -ypos});

        float dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();
        float speed = dt * 1000.0;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.move({0, speed}); }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.move({0, -speed}); }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.move({-speed, 0}); }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.move({speed, 0}); }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        scene.render(camera, lights);

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    TextureManager::instance().textures.clear();
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
