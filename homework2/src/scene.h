#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

#define PARSE_VEC3_2(name, target) do { if (cmd == name) { auto& _T = target; ss >> _T.x >> _T.y >> _T.z; } } while (0)
#define PARSE_VEC3(name) PARSE_VEC3_2(#name, M.name)
#define PARSE_SCALAR(name) do { if (cmd == #name) { ss >> M.name; } } while (0)

struct Material {
    glm::vec3 Ka; // ambient color
    glm::vec3 Kd; // diffuse color
    glm::vec3 Ks; // specular color
    float Ns; // specular exponent

    std::string map_Ka; // ambient texture map
    std::string map_Kd; // diffuse texture map
    std::string map_Ks; // specular texture map
    std::string map_d;  // alpha texture map
    std::string norm;   // normal map
};

struct MaterialMap {
    std::unordered_map<std::string, Material> materials;
};

MaterialMap loadMTL(auto& stream) {
    MaterialMap result;
    std::string cur;

    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with("#")) continue;
        std::stringstream ss{line};
        std::string cmd;
        ss >> cmd;

        if (cmd == "newmtl") {
            ss >> cur;
            continue;
        }

        auto& M = result.materials[cur];

        PARSE_VEC3(Ka);
        PARSE_VEC3(Kd);
        PARSE_VEC3(Ks);
        PARSE_SCALAR(Ns);
        PARSE_SCALAR(map_Ka);
        PARSE_SCALAR(map_Kd);
        PARSE_SCALAR(map_Ks);
        PARSE_SCALAR(map_d);
        PARSE_SCALAR(norm);
    }

    return result;
}

struct VertexData {
    glm::vec3 position;
    glm::vec2 texcoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct SceneObject {
    std::vector<VertexData> vertices;
    std::vector<unsigned> indices;
    Material material;
};

struct Scene {
    std::unordered_map<std::string, SceneObject> objects;
};

Scene loadOBJ(auto& stream, const MaterialMap& materials) {
    Scene result;
    std::vector<glm::vec3> v, vt, vn;
    std::string material;

    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with("#")) continue;
        std::stringstream ss{line};
        std::string cmd;
        ss >> cmd;

        if (cmd == "usemtl") {
            ss >> material;
            result.objects[material].material = materials.materials.at(material);
            continue;
        }

        auto& obj = result.objects[material];

        PARSE_VEC3_2("v", v.emplace_back());
        PARSE_VEC3_2("vt", vt.emplace_back());
        PARSE_VEC3_2("vn", vn.emplace_back());

        if (cmd == "f") {
            std::string face;
            unsigned first = obj.vertices.size();
            unsigned count = 0;
            while (ss >> face) {
                int vi, vti, vni;
                char dummy;
                std::stringstream fs{face};
                fs >> vi >> dummy >> vti >> dummy >> vni;
                --vi, --vti, --vni;

                auto& vertex = obj.vertices.emplace_back();
                vertex.position = v[vi];
                vertex.texcoord = { vt[vti].x, vt[vti].y };
                vertex.normal = vn[vni];

                ++count;
            }

            for (int i = 2; i < count; ++i) {
                obj.indices.push_back(first);
                obj.indices.push_back(first + i - 1);
                obj.indices.push_back(first + i);

                // calculate tangents and bitangents
                auto& v1 = obj.vertices[first];
                auto& v2 = obj.vertices[first + i - 1];
                auto& v3 = obj.vertices[first + i];

                auto edge1 = v2.position - v1.position;
                auto edge2 = v3.position - v1.position;
                auto duv1 = v2.texcoord - v1.texcoord;
                auto duv2 = v3.texcoord - v1.texcoord;

                float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);
                v1.tangent = v2.tangent = v3.tangent = f * (duv2.y * edge1 - duv1.y * edge2);
                v1.bitangent = v2.bitangent = v3.bitangent = f * (-duv2.x * edge1 - duv1.x * edge2);
            }
        }
    }

    return result;
}
