#pragma once

#include <glad/glad.h>
#include <string>
#include <stdexcept>

template <typename New, typename Delete>
struct GLObject {
    GLuint Id = 0;

    GLObject() : Id (New{}()) {}
    explicit GLObject(GLuint id) : Id(id) {}
    GLObject(const GLObject& that) = delete;
    GLObject(GLObject&& that) noexcept : Id (that.Id) { that.Id = 0; }
    GLObject& operator=(const GLObject& that) = delete;
    GLObject& operator=(GLObject&& that) noexcept { Id = that.Id; that.Id = 0; }
    ~GLObject() { if (Id) Delete{}(Id); }

    operator GLuint() const noexcept { return Id; }
};

using VertexArray = GLObject<decltype([] {
    GLuint id;
    glCreateVertexArrays(1, &id);
    return id;
}), decltype([](GLuint id) {
    glDeleteVertexArrays(1, &id);
})>;

using Buffer = GLObject<decltype([] {
    GLuint id;
    glCreateBuffers(1, &id);
    return id;
}), decltype([](GLuint id) {
    glDeleteBuffers(1, &id);
})>;

using Program = GLObject<decltype([] { return 0; }), decltype([](GLuint id) {
    glDeleteProgram(id);
})>;

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
	GLuint result = glCreateProgram();
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

	return Program{result};
}
