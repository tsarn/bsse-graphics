#pragma once

static const char* graphVS = R"(
#version 330 core

uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec2 in_coords;
layout (location = 1) in float in_value;

out vec3 color;

void main() {
    gl_Position = projection * view * vec4(in_coords.x, in_value, in_coords.y, 1.0);
    color = mix(vec3(0.25, 0.5, 0.75), vec3(0.75, 0.5, 0.25), 1.0 + 0.5 * clamp(in_value, -1.0, 1.0));
}
)";

static const char* graphFS = R"(
#version 330 core

in vec3 color;

out vec4 out_color;

void main() {
    out_color = vec4(color, 1.0);
}
)";

static const char* isolinesVS = R"(
#version 330 core

uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_coords;

void main() {
    gl_Position = projection * view * vec4(in_coords.xzy, 1.0);
}
)";

static const char* isolinesFS = R"(
#version 330 core

out vec4 out_color;

void main() {
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
}
)";
