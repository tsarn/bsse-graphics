#pragma once

#include <string>
#include <stdexcept>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

template <typename Impl>
struct GLObject {
    GLuint Id = 0;

    GLObject() : Id(Impl::New()) {}
    explicit GLObject(GLuint id) : Id(id) {}
    GLObject(const GLObject& that) = delete;
    GLObject(GLObject&& that) noexcept : Id (that.Id) { that.Id = 0; }
    GLObject& operator=(const GLObject& that) = delete;
    GLObject& operator=(GLObject&& that) noexcept { Id = that.Id; that.Id = 0; return *this; }
    ~GLObject() { if (Id) Impl::Delete(Id); }

    operator GLuint() const noexcept { return Id; }
};

struct VertexArray : GLObject<VertexArray> {
    static GLuint New() {
        GLuint id;
        glGenVertexArrays(1, &id);
        return id;
    }

    static void Delete(GLuint id) {
        glDeleteVertexArrays(1, &id);
    }
};

struct Buffer : GLObject<Buffer> {
    static GLuint New() {
        GLuint id;
        glGenBuffers(1, &id);
        return id;
    }

    static void Delete(GLuint id) {
        glDeleteBuffers(1, &id);
    }
};

struct Framebuffer : GLObject<Framebuffer> {
    static GLuint New() {
        GLuint id;
        glGenFramebuffers(1, &id);
        return id;
    }

    static void Delete(GLuint id) {
        glDeleteFramebuffers(1, &id);
    }
};

struct Texture : GLObject<Texture> {
    static GLuint New() {
        GLuint id;
        glGenTextures(1, &id);
        return id;
    }

    static void Delete(GLuint id) {
        glDeleteTextures(1, &id);
    }
};

struct Program : GLObject<Program> {
    mutable std::unordered_map<std::string, GLint> locations;

    static GLuint New() {
        return glCreateProgram();
    }

    static void Delete(GLuint id) {
        glDeleteProgram(id);
    }

    void setUniform(const std::string& name, auto value) const {
        GLint location;
        auto it = locations.find(name);
        if (it == locations.end()) {
            location = glGetUniformLocation(Id, name.c_str());
            locations.emplace(name, location);
        } else {
            location = it->second;
        }

        using type = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<type, glm::vec3>) {
            glUniform3fv(location, 1, glm::value_ptr(value));
        } else if constexpr (std::is_same_v<type, glm::mat4>) {
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        } else if constexpr (std::is_same_v<type, int>) {
            glUniform1i(location, value);
        } else if constexpr (std::is_same_v<type, bool>) {
            glUniform1i(location, value);
        } else if constexpr (std::is_same_v<type, float>) {
            glUniform1f(location, value);
        } else {
            static_assert(std::is_same_v<type, void>, "setUniform: Not implemented");
        }
    }
};

static GLuint createShader(GLenum type, const char * source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint infoLogLen;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &infoLogLen);
        std::string infoLog(infoLogLen, '\0');
        glGetShaderInfoLog(result, infoLog.size(), nullptr, infoLog.data());
        throw std::runtime_error("Shader compilation failed: " + infoLog);
    }
    return result;
}

static Program createProgram(GLuint vs, GLuint fs) {
    Program result;
    glAttachShader(result, vs);
    glAttachShader(result, fs);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint infoLogLen;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &infoLogLen);
        std::string infoLog(infoLogLen, '\0');
        glGetProgramInfoLog(result, infoLog.size(), nullptr, infoLog.data());
        throw std::runtime_error("Program linkage failed: " + infoLog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return result;
}
