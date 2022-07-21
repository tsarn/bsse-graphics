#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include "scene.h"
#include "gl_objects.h"
#include "shaders.h"
#include "camera.h"
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

static GLenum formats[] = {
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA
};

struct TextureManager {
    std::unordered_map<std::string, Texture> textures;

    static TextureManager& instance() {
        static TextureManager singleton;
        return singleton;
    }

    static GLuint get(const std::string& name) {
        if (name.empty()) return 0;
        if (!instance().textures.contains(name)) {
            instance().textures[name];
            GLuint id = instance().textures.at(name).Id;
            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            std::stringstream path;
            path << "./sponza/" << name;
            int width, height, chans;
            auto ptr = stbi_load(path.str().c_str(), &width, &height, &chans, 0);
            if (!ptr) {
                throw std::runtime_error{"failed to load texture"};
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, formats[chans - 1], GL_UNSIGNED_BYTE, ptr);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(ptr);
        }
        return instance().textures.at(name).Id;
    }

private:
    TextureManager() = default;
};

struct DrawableSceneObject {
    VertexArray vao;
    Buffer vbo, ebo;
    int vertexCount;

    GLuint map_Ka;
    GLuint map_Kd;
    GLuint map_Ks;
    GLuint map_d;
    GLuint norm;

    glm::vec3 Ka;
    glm::vec3 Kd;
    glm::vec3 Ks;
    float Ns;

    void init(const SceneObject& obj) {
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(obj.vertices[0]), obj.vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj.indices.size() * sizeof(obj.indices[0]), obj.indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj.vertices[0]), (void*)offsetof(VertexData, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(obj.vertices[0]), (void*)offsetof(VertexData, texcoord));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(obj.vertices[0]), (void*)offsetof(VertexData, normal));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(obj.vertices[0]), (void*)offsetof(VertexData, tangent));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(obj.vertices[0]), (void*)offsetof(VertexData, bitangent));

        glBindVertexArray(0);

        map_Ka = TextureManager::get(obj.material.map_Ka);
        map_Kd = TextureManager::get(obj.material.map_Kd);
        map_Ks = TextureManager::get(obj.material.map_Ks);
        map_d = TextureManager::get(obj.material.map_d);
        norm = TextureManager::get(obj.material.norm);

        Ka = obj.material.Ka;
        Kd = obj.material.Kd;
        Ks = obj.material.Ks;
        Ns = obj.material.Ns;

        vertexCount = obj.indices.size();
    }

    void renderFlat() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void render(const Program& shader) const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, map_Ka);
        shader.setUniform("uniform_Ka", Ka);
        shader.setUniform("has_Ka", map_Ka != 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, map_Kd);
        shader.setUniform("uniform_Kd", Kd);
        shader.setUniform("has_Kd", map_Kd != 0);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, map_d);
        shader.setUniform("has_d", map_d != 0);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, norm);
        shader.setUniform("has_norm", norm != 0);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, map_Ks);
        shader.setUniform("uniform_Ks", Ks);
        shader.setUniform("has_Ks", map_Ks != 0);

        shader.setUniform("Ns", Ns);

        renderFlat();
    }
};

struct Light {
    glm::vec3 position;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 attenuation;
    bool directional;
};

static const unsigned SHADOW_WIDTH = 8192, SHADOW_HEIGHT = 8192;

struct DrawableScene {
    std::vector<DrawableSceneObject> objects;
    Program shader;
    Framebuffer shadowFBO;
    Texture shadowTexture;
    glm::mat4 shadowTransform;

    void init(const Scene& scene) {
        for (const auto& [_, obj] : scene.objects) {
            objects.emplace_back().init(obj);
        }

        std::sort(objects.begin(), objects.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.map_d < rhs.map_d;
        });

        shader = createProgram(
            createShader(GL_VERTEX_SHADER, SCENE_VERTEX_SHADER),
            createShader(GL_FRAGMENT_SHADER, SCENE_FRAGMENT_SHADER)
        );

        glBindTexture(GL_TEXTURE_2D, shadowTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    void calculateShadows(const Light& light) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);

        // draw to shadow space
        auto view = glm::lookAt(light.position * 3000.0f, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
        auto projection = glm::ortho(-3000.0f, 3000.0f, -3000.0f, 3000.0f, 1.0f, 5000.0f);

        glUseProgram(shader);
        shader.setUniform("is_drawing_shadows", true);
        shader.setUniform("view", view);
        shader.setUniform("projection", projection);
        shadowTransform = projection * view;

        for (const auto& obj : objects) {
            obj.renderFlat();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindTexture(GL_TEXTURE_2D, shadowTexture);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void render(const Camera& camera, const std::vector<Light>& lights) const {
        glUseProgram(shader);
        shader.setUniform("is_drawing_shadows", false);
        shader.setUniform("view", camera.view());
        shader.setUniform("projection", camera.projection());
        shader.setUniform("sampler_Ka", 0);
        shader.setUniform("sampler_Kd", 1);
        shader.setUniform("sampler_d", 2);
        shader.setUniform("sampler_norm", 3);
        shader.setUniform("sampler_Ks", 4);
        shader.setUniform("sampler_shadow", 5);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, shadowTexture);

        shader.setUniform("camera_position", camera.position);
        shader.setUniform("shadow_transform", shadowTransform);

        int n = (int)lights.size();
        shader.setUniform("lights_size", n);
        for (int i = 0; i < n; ++i) {
            shader.setUniform("lights[" + std::to_string(i) + "].position", lights[i].position);
            shader.setUniform("lights[" + std::to_string(i) + "].diffuse", lights[i].diffuse);
            shader.setUniform("lights[" + std::to_string(i) + "].specular", lights[i].specular);
            shader.setUniform("lights[" + std::to_string(i) + "].attenuation", lights[i].attenuation);
            shader.setUniform("lights[" + std::to_string(i) + "].directional", lights[i].directional);
        }

        for (const auto& obj : objects) {
            obj.render(shader);
        }
    }
};
