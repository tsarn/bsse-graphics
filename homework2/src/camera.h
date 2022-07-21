#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>

struct Camera {
    glm::vec3 position{0.0f};
    glm::vec2 angle{0.0f};
    float fov = glm::radians(70.0f);
    float aspectRatio = 1.0f;

    glm::vec3 direction() const {
        return {
            glm::cos(angle.y) * glm::sin(angle.x),
            glm::sin(angle.y),
            glm::cos(angle.y) * glm::cos(angle.x)
        };
    }

    glm::vec3 right() const {
        return {
            glm::sin(angle.x - glm::half_pi<float>()),
            .0f,
            glm::cos(angle.x - glm::half_pi<float>())
        };
    }

    glm::vec3 up() const {
        return glm::cross(right(), direction());
    }

    glm::mat4 view() const {
        return glm::lookAt(position, position + direction(), up());
    }

    glm::mat4 projection() const {
        return glm::perspective(fov, aspectRatio, 0.1f, 10000.0f);
    }

    void look(glm::vec2 delta) {
        angle.x += delta.x;
        angle.y = glm::clamp(angle.y + delta.y, -glm::half_pi<float>(), +glm::half_pi<float>());
    }

    void move(glm::vec2 delta) {
        position += direction() * delta.y;
        position += right() * delta.x;
    }
};
