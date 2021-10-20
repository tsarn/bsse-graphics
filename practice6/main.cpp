#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <sstream>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "textures.hpp"

std::string to_string(std::string_view str)
{
	return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
	throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
	throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 2) in vec2 in_texcoord;

out vec2 texcoord;
out vec3 world_pos;

void main()
{
	gl_Position = projection * view * model * vec4(in_position, 1.0);
	texcoord = in_texcoord;
    world_pos = (model * vec4(in_position, 1.0)).xyz;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo_texture;
uniform sampler2D normal_texture;
uniform sampler2D ao_texture;
uniform sampler2D roughness_texture;

uniform vec3 ambient;
uniform vec3 light_position[3];
uniform vec3 light_color[3];
uniform vec3 light_attenuation[3];
uniform mat4 model;
uniform vec3 camera_pos;

in vec2 texcoord;
in vec3 world_pos;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 albedo = texture(albedo_texture, texcoord).xyz;
    vec3 norm = texture(normal_texture, texcoord).xyz * 2.0 - vec3(1.0);
    norm = normalize((model * vec4(norm, 0.0)).xyz);

    float roughness = texture(roughness_texture, texcoord).x;

    vec3 result_color = ambient * pow(texture(ao_texture, texcoord).xyz, vec3(4.0));
    vec3 camera_dir = normalize(camera_pos - world_pos);

    for (int i = 0; i < 3; ++i) {
        vec3 light_dir = normalize(light_position[i] - world_pos);
        float light_dist = length(light_position[i] - world_pos);
        float light_factor = max(dot(norm, light_dir), 0.0);
        float light_intensity = 1.0 / dot(light_attenuation[i], vec3(1.0, light_dist, light_dist * light_dist));
        vec3 reflected_dir = reflect(-light_dir, norm);
        float specular_intensity = pow(max(0.0, dot(camera_dir, reflected_dir)), 4.0) * (1.0 - roughness);
        result_color += light_color[i] * (light_intensity * light_factor + specular_intensity);
    }

    result_color *= albedo;
    result_color /= vec3(1.0) + result_color;
	out_color = vec4(result_color, 1.0);
}
)";

GLuint create_shader(GLenum type, const char * source)
{
	GLuint result = glCreateShader(type);
	glShaderSource(result, 1, &source, nullptr);
	glCompileShader(result);
	GLint status;
	glGetShaderiv(result, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE)
	{
		GLint info_log_length;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Shader compilation failed: " + info_log);
	}
	return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
	GLuint result = glCreateProgram();
	glAttachShader(result, vertex_shader);
	glAttachShader(result, fragment_shader);
	glLinkProgram(result);

	GLint status;
	glGetProgramiv(result, GL_LINK_STATUS, &status);
	if (status != GL_TRUE)
	{
		GLint info_log_length;
		glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Program linkage failed: " + info_log);
	}

	return result;
}

struct vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

static vertex plane_vertices[]
{
	{{-10.f, -10.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
	{{-10.f,  10.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
	{{ 10.f, -10.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
	{{ 10.f,  10.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f}},
};

static std::uint32_t plane_indices[]
{
	0, 1, 2, 2, 1, 3,
};

int main() try
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		sdl2_fail("SDL_Init: ");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_Window * window = SDL_CreateWindow("Graphics course practice 5",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		800, 600,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

	if (!window)
		sdl2_fail("SDL_CreateWindow: ");

	int width, height;
	SDL_GetWindowSize(window, &width, &height);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context)
		sdl2_fail("SDL_GL_CreateContext: ");

	if (auto result = glewInit(); result != GLEW_NO_ERROR)
		glew_fail("glewInit: ", result);

	if (!GLEW_VERSION_3_3)
		throw std::runtime_error("OpenGL 3.3 is not supported");

	glClearColor(0.8f, 0.8f, 1.f, 0.f);

	auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
	auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
	auto program = create_program(vertex_shader, fragment_shader);

	GLuint model_location = glGetUniformLocation(program, "model");
	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint projection_location = glGetUniformLocation(program, "projection");
	GLuint albedo_location = glGetUniformLocation(program, "albedo_texture");
	GLuint normal_location = glGetUniformLocation(program, "normal_texture");
	GLuint ao_location = glGetUniformLocation(program, "ao_texture");
	GLuint roughness_location = glGetUniformLocation(program, "roughness_texture");
	GLuint ambient_location = glGetUniformLocation(program, "ambient");
	GLuint camera_pos_location = glGetUniformLocation(program, "camera_pos");

    constexpr int n = 3;
    auto getUniformLocation = [&](const std::string& name) {
        std::array<GLuint, n> locations;
        for (int i = 0; i < n; ++i) {
            std::stringstream ss;
            ss << name << "[" << i << "]";
            locations[i] = glGetUniformLocation(program, ss.str().c_str());
        }
        return locations;
    };

	auto light_position_location = getUniformLocation("light_position");
	auto light_color_location = getUniformLocation("light_color");
	auto light_attenuation_location = getUniformLocation("light_attenuation");

	glUseProgram(program);
	glUniform1i(albedo_location, 0);
	glUniform1i(normal_location, 1);
	glUniform1i(ao_location, 2);
	glUniform1i(roughness_location, 3);

	GLuint plane_vao, plane_vbo, plane_ebo;
	glGenVertexArrays(1, &plane_vao);
	glBindVertexArray(plane_vao);

	glGenBuffers(1, &plane_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, plane_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &plane_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_indices), plane_indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));
//	glEnableVertexAttribArray(1);
//	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(12));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(24));

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLuint brick_albedo;
	glGenTextures(1, &brick_albedo);
	glBindTexture(GL_TEXTURE_2D, brick_albedo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, brick_albedo_width, brick_albedo_height, 0, GL_RGB, GL_UNSIGNED_BYTE, brick_albedo_data);
	glGenerateMipmap(GL_TEXTURE_2D);

	GLuint brick_normal;
	glGenTextures(1, &brick_normal);
	glBindTexture(GL_TEXTURE_2D, brick_normal);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, brick_normal_width, brick_normal_height, 0, GL_RGB, GL_UNSIGNED_BYTE, brick_normal_data);
	glGenerateMipmap(GL_TEXTURE_2D);

	GLuint brick_ao;
	glGenTextures(1, &brick_ao);
	glBindTexture(GL_TEXTURE_2D, brick_ao);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, brick_ao_width, brick_ao_height, 0, GL_RGB, GL_UNSIGNED_BYTE, brick_ao_data);
	glGenerateMipmap(GL_TEXTURE_2D);

	GLuint brick_roughness;
	glGenTextures(1, &brick_roughness);
	glBindTexture(GL_TEXTURE_2D, brick_roughness);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, brick_roughness_width, brick_roughness_height, 0, GL_RGB, GL_UNSIGNED_BYTE, brick_roughness_data);
	glGenerateMipmap(GL_TEXTURE_2D);

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	std::map<SDL_Keycode, bool> button_down;

	float view_angle = glm::pi<float>() / 6.f;
	float camera_distance = 15.f;

	bool running = true;
	while (running)
	{
		for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
		{
		case SDL_QUIT:
			running = false;
			break;
		case SDL_WINDOWEVENT: switch (event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED:
				width = event.window.data1;
				height = event.window.data2;
				glViewport(0, 0, width, height);
				break;
			}
			break;
		case SDL_KEYDOWN:
			button_down[event.key.keysym.sym] = true;
			break;
		case SDL_KEYUP:
			button_down[event.key.keysym.sym] = false;
			break;
		}

		if (!running)
			break;

		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
		last_frame_start = now;
		time += dt;

		if (button_down[SDLK_UP])
			camera_distance -= 5.f * dt;
		if (button_down[SDLK_DOWN])
			camera_distance += 5.f * dt;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		float near = 0.1f;
		float far = 100.f;

		glm::mat4 view(1.f);
		view = glm::translate(view, {0.f, 0.f, -camera_distance});
		view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});

		glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 ambient{0.15f};

		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
		glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(ambient_location, 1, glm::value_ptr(ambient));

        for (int i = 0; i < n; ++i) {
            glm::vec3 light_position{7 * cos(time + i * glm::radians(120.0f)), 2.0f, 7 * sin(time + i * glm::radians(120.0f))};
            glm::vec3 light_attenuation{1.0f, 0.0f, 0.1f};
            glm::vec3 light_color{i == 0, i == 1, i == 2};
            glUniform3fv(light_position_location[i], 1, glm::value_ptr(light_position));
            glUniform3fv(light_attenuation_location[i], 1, glm::value_ptr(light_attenuation));
            glUniform3fv(light_color_location[i], 1, glm::value_ptr(light_color));
        }

        glm::vec3 cameraPosition = -(view * glm::vec4{1.0f}).xyz();
        glUniform3fv(camera_pos_location, 1, glm::value_ptr(cameraPosition));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, brick_albedo);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, brick_normal);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, brick_ao);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, brick_roughness);

        {
            glm::mat4 model(1.f);
            model = glm::rotate(model, -glm::pi<float>() / 2.f, {1.f, 0.f, 0.f});
            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
            glDrawElements(GL_TRIANGLES, std::size(plane_indices), GL_UNSIGNED_INT, nullptr);
        }

        {
            glm::mat4 model(1.f);
            model = glm::translate(model, glm::vec3{0.0f, 10.0f, -10.0f});
            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
            glDrawElements(GL_TRIANGLES, std::size(plane_indices), GL_UNSIGNED_INT, nullptr);
        }

        {
            glm::mat4 model(1.f);
            model = glm::translate(model, glm::vec3{0.0f, 10.0f, -10.0f});
            model = glm::rotate(model, glm::pi<float>() / 2.f, {0.f, 1.f, 0.f});
            model = glm::translate(model, glm::vec3{-10.0f, 0.0f, -10.0f});
            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
            glDrawElements(GL_TRIANGLES, std::size(plane_indices), GL_UNSIGNED_INT, nullptr);
        }

        {
            glm::mat4 model(1.f);
            model = glm::translate(model, glm::vec3{0.0f, 10.0f, -10.0f});
            model = glm::rotate(model, glm::pi<float>() / -2.f, {0.f, 1.f, 0.f});
            model = glm::translate(model, glm::vec3{10.0f, 0.0f, -10.0f});
            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
            glDrawElements(GL_TRIANGLES, std::size(plane_indices), GL_UNSIGNED_INT, nullptr);
        }

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
