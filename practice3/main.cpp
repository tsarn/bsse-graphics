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
#include <cstddef>
#include <random>
#include <array>

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

uniform mat4 view;
uniform float time;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in float in_dist;

out vec4 color;
out float dist;

void main()
{
	gl_Position = view * vec4(in_position, 0.0, 1.0);
	color = in_color;
    dist = in_dist - time * 100;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec4 color;
in float dist;

uniform int dash;

layout (location = 0) out vec4 out_color;

void main()
{
    if (dash == 1 && mod(dist, 40.0) > 20.0) {
        discard;
    } else {
        out_color = color;
    }
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

struct vec2
{
	float x;
	float y;
};

struct vertex
{
	vec2 position;
    std::array<std::uint8_t, 4> color;
    float dist;
};

vec2 bezier(std::vector<vertex> const & vertices, float t)
{
	std::vector<vec2> points(vertices.size());

	for (std::size_t i = 0; i < vertices.size(); ++i)
		points[i] = vertices[i].position;

	// De Casteljau's algorithm
	for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
		for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
			points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
			points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
		}
	}
	return points[0];
}

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

	SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
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

	SDL_GL_SetSwapInterval(0);

	if (auto result = glewInit(); result != GLEW_NO_ERROR)
		glew_fail("glewInit: ", result);

	if (!GLEW_VERSION_3_3)
		throw std::runtime_error("OpenGL 3.3 is not supported");

	auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
	auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
	auto program = create_program(vertex_shader, fragment_shader);

    std::mt19937 rng{std::random_device{}()};

    std::vector<vertex> vertices;

    GLuint vbo_points;
    glGenBuffers(1, &vbo_points);

    GLuint vbo_bezier;
    glGenBuffers(1, &vbo_bezier);

    int quality = 8;

    size_t curve_size = 0;

    auto update_bezier = [&] {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_points);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);

        curve_size = vertices.empty() ? 0 : (vertices.size() - 1) * quality + 1;
        std::vector<vertex> curve{curve_size};
        for (int i = 0; i < curve_size; ++i) {
            float t = 1.0f * i / (curve_size - 1);
            curve[i].position = bezier(vertices, t);
            curve[i].color = {255, 255, 255, 255};
            if (i == 0) {
                curve[i].dist = 0.0f;
            } else {
                curve[i].dist = curve[i - 1].dist +
                    hypotf(curve[i].position.x - curve[i - 1].position.x, curve[i].position.y - curve[i - 1].position.y);
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo_bezier);
        glBufferData(GL_ARRAY_BUFFER, curve.size() * sizeof(vertex), curve.data(), GL_STATIC_DRAW);
    };

    GLuint vao_points;
    glGenVertexArrays(1, &vao_points);
    glBindVertexArray(vao_points);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_points);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, color));

    GLuint vao_bezier;
    glGenVertexArrays(1, &vao_bezier);
    glBindVertexArray(vao_bezier);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bezier);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, color));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, dist));

    glLineWidth(2);
    glPointSize(10);

	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint dash_location = glGetUniformLocation(program, "dash");
	GLuint time_location = glGetUniformLocation(program, "time");

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

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
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_LEFT)
			{
				int mouse_x = event.button.x;
				int mouse_y = event.button.y;

                std::uniform_int_distribution<std::uint8_t> dist{0, 255};

                auto r = dist(rng);
                auto g = dist(rng);
                auto b = dist(rng);

                vertices.push_back({
                    .position = {1.0f * mouse_x, 1.0f * mouse_y},
                    .color = { r, g, b, 255 }
                });

                update_bezier();
			}
			else if (event.button.button == SDL_BUTTON_RIGHT)
			{
                if (!vertices.empty()) {
                    vertices.pop_back();

                    update_bezier();
                }
			}
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_LEFT)
			{
                if (quality > 1) {
                    --quality;
                    update_bezier();
                }
			}
			else if (event.key.keysym.sym == SDLK_RIGHT)
			{
                ++quality;
                update_bezier();
			}
			break;
		}

		if (!running)
			break;

		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
		last_frame_start = now;
		time += dt;

		glClear(GL_COLOR_BUFFER_BIT);

		float view[16] =
		{
			2.f / width, 0.f, 0.f, -1.f,
			0.f, -2.f / height, 0.f, 1.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f,
		};

		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        glUniform1f(time_location, time);

        glUniform1i(dash_location, 1);
        glBindVertexArray(vao_bezier);
        glDrawArrays(GL_LINE_STRIP, 0, curve_size);

        glUniform1i(dash_location, 0);
        glBindVertexArray(vao_points);
        glDrawArrays(GL_POINTS, 0, vertices.size());

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
