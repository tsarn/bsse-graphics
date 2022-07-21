#pragma once

static const char* SCENE_VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_tangent;
layout (location = 4) in vec3 in_bitangent;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 shadow_transform;

out vec3 position;
out vec2 texcoord;
out vec3 normal;
out vec4 shadow_position;
out mat3 TBN;

void main() {
    gl_Position = projection * view * vec4(in_position, 1.0);

    vec3 T = normalize(in_tangent);
    vec3 B = normalize(in_bitangent);
    vec3 N = normalize(in_normal);
    TBN = mat3(T, B, N);

    position = in_position;
    texcoord = vec2(in_texcoord.x, 1.0 - in_texcoord.y);
    normal = in_normal;
    shadow_position = shadow_transform * vec4(in_position, 1.0);
}
)";

static const char* SCENE_FRAGMENT_SHADER = R"(
#version 330 core

uniform bool is_drawing_shadows;

in vec3 position;
in vec2 texcoord;
in vec3 normal;
in vec4 shadow_position;
in mat3 TBN;

out vec4 out_color;

uniform sampler2D sampler_shadow;

uniform sampler2D sampler_Ka;
uniform vec3 uniform_Ka;
uniform bool has_Ka;

uniform sampler2D sampler_Kd;
uniform vec3 uniform_Kd;
uniform bool has_Kd;

uniform sampler2D sampler_d;
uniform bool has_d;

uniform sampler2D sampler_norm;
uniform bool has_norm;

uniform sampler2D sampler_Ks;
uniform vec3 uniform_Ks;
uniform bool has_Ks;

uniform float Ns;

uniform vec3 camera_position;

struct Light {
    vec3 position;
    vec3 diffuse;
    vec3 specular;
    vec3 attenuation;
    bool directional;
};

#define MAX_LIGHTS 16
uniform Light lights[MAX_LIGHTS];
uniform int lights_size;

float get_shadow() {
    vec3 coords = shadow_position.xyz / shadow_position.w;
    coords = coords * 0.5 + 0.5;
    bool in_shadow_texture = (coords.x > 0.0) && (coords.x < 1.0) && (coords.y > 0.0) && (coords.y < 1.0) && (coords.z > 0.0) && (coords.z < 1.0);
    if (!in_shadow_texture) return 0.0;

    float closest_depth = texture(sampler_shadow, coords.xy).x;
    float current_depth = coords.z;
    float shadow = current_depth - 0.005 > closest_depth  ? 1.0 : 0.0;
    return shadow;
}

void main() {
    if (is_drawing_shadows && !has_d) return;

    vec4 Ka = has_Ka ? vec4(pow(texture(sampler_Ka, texcoord).rgb, vec3(2.2)), 1.0) : vec4(uniform_Ka, 1.0);

    if (has_d) {
        Ka.a = texture(sampler_d, texcoord).x;
    }

    if (Ka.a < 0.001) discard;

    if (is_drawing_shadows) return;

    vec4 Kd = has_Kd ? vec4(pow(texture(sampler_Kd, texcoord).rgb, vec3(2.2)), 1.0) : vec4(uniform_Kd, 1.0);
    vec4 Ks = has_Ks ? texture(sampler_Ks, texcoord) : vec4(uniform_Ks, 1.0);

    vec3 norm = normal;

    if (has_norm) {
        norm = texture(sampler_norm, texcoord).xyz * 2.0 - vec3(1.0, 1.0, 1.0);
        norm = TBN * norm;
    }

    norm = normalize(norm);

    out_color = Ka * 0.1;
    for (int i = 0; i < lights_size; ++i) {
        float shadow = lights[i].directional ? get_shadow() : 0.0;
        vec3 direction = lights[i].directional ? lights[i].position : normalize(lights[i].position - position);
        float distance = lights[i].directional ? 0.0 : length(position - lights[i].position);
        float factor = max(dot(direction, norm), 0.0);
        float intensity = 1.0 / dot(vec3(1.0, distance, distance * distance), lights[i].attenuation);

        out_color.rgb += Kd.rgb * lights[i].diffuse * factor * intensity * (1.0 - shadow);

        vec3 camera_direction = normalize(camera_position - position);
        vec3 reflect_direction = reflect(-direction, norm);
        float spec = pow(max(dot(camera_direction, reflect_direction), 0.0), Ns);

        out_color.rgb += Ks.rrr * lights[i].specular * spec * intensity * (1.0 - shadow);
    }

    out_color.a = Ka.a;
}
)";
