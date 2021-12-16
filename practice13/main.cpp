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
#include <sstream>
#include <fstream>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_set>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "aabb.hpp"
#include "frustum.hpp"
#include "mesh_utils.hpp"
#include "intersect.hpp"

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
uniform mat4 projection;
uniform vec3 offset;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_offset;

out vec3 normal;

void main()
{
	normal = in_normal;
	gl_Position = projection * view * vec4(in_position + in_offset + offset, 1.0);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec3 light_dir;

in vec3 normal;

layout (location = 0) out vec4 out_color;

void main()
{
	float lightness = 0.5 + 0.5 * dot(normal, light_dir);
	out_color = vec4(vec3(lightness), 1.0);
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

template<typename ... Shaders>
GLuint create_program(Shaders ... shaders) {
	GLuint result = glCreateProgram();
	(glAttachShader(result, shaders), ...);
	glLinkProgram(result);

	GLint status;
	glGetProgramiv(result, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Program linkage failed: " + info_log);
	}

	return result;
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
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_Window * window = SDL_CreateWindow("Graphics course practice 12",
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

	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint projection_location = glGetUniformLocation(program, "projection");
	GLuint offset_location = glGetUniformLocation(program, "offset");
	GLuint light_dir_location = glGetUniformLocation(program, "light_dir");

	std::vector<vertex> vertices;
	std::vector<std::uint32_t> indices;
    std::vector<std::size_t> lod_sizes, lod_offsets;
    int lod_count = 6;
	{
        for (int i = 0; i < lod_count; ++i) {
            std::stringstream path;
            path << PRACTICE_SOURCE_DIRECTORY;
            path << "/bunny" << i << ".obj";
            std::ifstream in{path.str()};
            auto [obj_vertices, obj_indices] = load_obj(in, 4.f);
            for (auto idx : obj_indices) {
                indices.push_back(idx + vertices.size());
            }
            vertices.insert(vertices.end(), obj_vertices.begin(), obj_vertices.end());
            if (i == 0) {
                lod_offsets.push_back(0);
            } else {
                lod_offsets.push_back(lod_offsets.back() + lod_sizes.back());
            }
            lod_sizes.push_back(obj_indices.size());
        }
	}
    auto model_bbox = bbox(vertices);
	fill_normals(vertices, indices);

    std::vector<glm::vec3> offsets;
    for (int x = -16; x < 16; ++x) {
        for (int z = -16; z < 16; ++z) {
            offsets.emplace_back(x, 0, z);
        }
    }

	GLuint vao, vbo, ebo, offsets_vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(12));

	glGenBuffers(1, &offsets_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, offsets_vbo);
	glBufferData(GL_ARRAY_BUFFER, offsets.size() * sizeof(offsets[0]), offsets.data(), GL_STATIC_DRAW);

	//glEnableVertexAttribArray(2);
	//glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    //glVertexAttribDivisor(2, 1);

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	glm::vec3 camera_position{0.f, 0.5f, 3.f};

	float camera_rotation = 0.f;

	std::map<SDL_Keycode, bool> button_down;
    std::vector<GLuint> usedQueries, freeQueries;
    std::vector<float> frameTimes;

    auto getQuery = [&] {
        GLuint id;
        if (freeQueries.empty()) {
            glGenQueries(1, &id);
        } else {
            id = freeQueries.back();
            freeQueries.pop_back();
        }
        usedQueries.push_back(id);
        return id;
    };

    auto measureTimes = [&] {
        std::unordered_set<GLuint> toRemove;
        for (size_t i = 0; i < usedQueries.size(); ++i) {
            auto query = usedQueries[i];
            GLint available = 0;
            glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available) {
                GLuint64 us;
                glGetQueryObjectui64v(query, GL_QUERY_RESULT, &us);
                float seconds = us * 1e-9f;

                frameTimes.push_back(seconds);
                freeQueries.push_back(query);
                toRemove.insert(query);
            }
        }
        usedQueries.erase(
            std::remove_if(
                usedQueries.begin(), usedQueries.end(), [&](auto query) {
                    return toRemove.contains(query);
                }
            ),
            usedQueries.end()
        );
    };

	bool running = true;
	bool paused = false;
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
			if (event.key.keysym.sym == SDLK_SPACE)
				paused = !paused;
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

		if (!paused)
			time += dt;

		float camera_move_forward = 0.f;
		float camera_move_sideways = 0.f;

		if (button_down[SDLK_w])
			camera_move_forward -= 3.f * dt;
		if (button_down[SDLK_s])
			camera_move_forward += 3.f * dt;
		if (button_down[SDLK_a])
			camera_move_sideways -= 3.f * dt;
		if (button_down[SDLK_d])
			camera_move_sideways += 3.f * dt;

		camera_position += camera_move_forward * glm::vec3(-std::sin(camera_rotation), 0.f, std::cos(camera_rotation));
		camera_position += camera_move_sideways * glm::vec3(std::cos(camera_rotation), 0.f, std::sin(camera_rotation));

		if (button_down[SDLK_LEFT])
			camera_rotation -= 3.f * dt;
		if (button_down[SDLK_RIGHT])
			camera_rotation += 3.f * dt;

		if (button_down[SDLK_DOWN])
			camera_position.y -= 3.f * dt;
		if (button_down[SDLK_UP])
			camera_position.y += 3.f * dt;

        glBeginQuery(GL_TIME_ELAPSED, getQuery());

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		float near = 0.1f;
		float far = 100.f;

		glm::mat4 view(1.f);
		view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
		view = glm::translate(view, -camera_position);

		glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

		glm::vec3 light_dir = glm::normalize(glm::vec3(1.f, 1.f, 1.f));

		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
		glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
		glUniform3fv(light_dir_location, 1, reinterpret_cast<float *>(&light_dir));

        frustum fr{projection * view};

		glBindVertexArray(vao);

//        glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, offsets.size());
        int drawnCount = 0;
        for (const auto& offset : offsets) {
            if (intersect(fr, aabb{model_bbox.first + offset, model_bbox.second + offset})) {
                int lod = (int)(glm::length(offset - camera_position) / 3);
                if (lod < 0) lod = 0;
                if (lod >= lod_count) lod = lod_count - 1;
                ++drawnCount;
                glUniform3fv(offset_location, 1, glm::value_ptr(offset));
                glDrawElements(GL_TRIANGLES, lod_sizes[lod], GL_UNSIGNED_INT, (void*)(lod_offsets[lod] * sizeof(std::uint32_t)));
            }
        }
//        std::cerr << "drawing " << drawnCount << " objects\n";

        glEndQuery(GL_TIME_ELAPSED);
        measureTimes();

		SDL_GL_SwapWindow(window);
	}

    std::sort(frameTimes.begin(), frameTimes.end());
    auto frameTimeQuant = [&](float q) {
        if (frameTimes.empty()) {
            return 0.f;
        }
        return frameTimes[(size_t)(q * frameTimes.size())];
    };
    std::cerr << "allocated " << freeQueries.size() + usedQueries.size() << " query objects\n";
    std::cerr << "collected " << frameTimes.size() << " frame times\n";
    std::cerr << "  p50: " << frameTimeQuant(0.50f) << " seconds \n";
    std::cerr << "  p90: " << frameTimeQuant(0.90f) << " seconds \n";
    std::cerr << "  p99: " << frameTimeQuant(0.99f) << " seconds \n";

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
