#ifndef OGLPROJS_H
#define OGLPROJS_H

#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <corecrt_math_defines.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace oglprojs {

// ============================================================================
// Core Classes
// ============================================================================

class Shader {
	struct CachedUniforms {
		GLint uModel, uView, uProj, uNormal;
		GLint uLightPos, uLightAmbient, uLightDiffuse, uLightSpecular, uLightColor;
		GLint uMatAmbient, uMatDiffuse, uMatSpecular, uMatEmission, uMatShininess;
		GLint uViewPos, uObjectColor;
	};
	GLuint id;

  public:
	CachedUniforms U;

	Shader(const std::string &vertexSrc, const std::string &fragmentSrc) {
		auto compile = [](const std::string &src, GLenum type) -> GLuint {
			GLuint shader = glCreateShader(type);
			const char *source = src.c_str();
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);

			GLint success;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				char log[512];
				glGetShaderInfoLog(shader, 512, nullptr, log);
				std::cerr << "Shader compilation error: " << log << std::endl;
			}
			return shader;
		};

		GLuint vs = compile(vertexSrc, GL_VERTEX_SHADER);
		GLuint fs = compile(fragmentSrc, GL_FRAGMENT_SHADER);

		id = glCreateProgram();
		glAttachShader(id, vs);
		glAttachShader(id, fs);
		glLinkProgram(id);

		GLint success;
		glGetProgramiv(id, GL_LINK_STATUS, &success);
		if (!success) {
			char log[512];
			glGetProgramInfoLog(id, 512, nullptr, log);
			std::cerr << "Shader linking error: " << log << std::endl;
		}

		glDeleteShader(vs);
		glDeleteShader(fs);
	}

	~Shader() { glDeleteProgram(id); }

	void use() const { glUseProgram(id); }

	void CacheUniforms() {
		U.uModel = glGetUniformLocation(id, "model");
		U.uView = glGetUniformLocation(id, "view");
		U.uProj = glGetUniformLocation(id, "projection");
		U.uNormal = glGetUniformLocation(id, "normalMatrix");
		U.uLightPos = glGetUniformLocation(id, "lightPos");
		U.uLightAmbient = glGetUniformLocation(id, "lightAmbient");
		U.uLightDiffuse = glGetUniformLocation(id, "lightDiffuse");
		U.uLightSpecular = glGetUniformLocation(id, "lightSpecular");
		U.uLightColor = glGetUniformLocation(id, "lightColor");
		U.uMatAmbient = glGetUniformLocation(id, "materialAmbient");
		U.uMatDiffuse = glGetUniformLocation(id, "materialDiffuse");
		U.uMatSpecular = glGetUniformLocation(id, "materialSpecular");
		U.uMatEmission = glGetUniformLocation(id, "materialEmission");
		U.uMatShininess = glGetUniformLocation(id, "materialShininess");
		U.uViewPos = glGetUniformLocation(id, "viewPos");
		U.uObjectColor = glGetUniformLocation(id, "objectColor");
	}

	void set(GLint &u, const glm::mat4 &m) const { glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(m)); }
	void set(GLint &u, const glm::mat3 &m) const { glUniformMatrix3fv(u, 1, GL_FALSE, glm::value_ptr(m)); }
	void set(GLint &u, const glm::vec3 &v) const { glUniform3fv(u, 1, glm::value_ptr(v)); }
	void set(GLint &u, float v) const { glUniform1f(u, v); }

	void setMat4(const std::string &name, const glm::mat4 &mat) const {
		glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
	}

	void setMat3(const std::string &name, const glm::mat3 &mat) const {
		glUniformMatrix3fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
	}

	void setVec3(const std::string &name, const glm::vec3 &vec) const {
		glUniform3fv(glGetUniformLocation(id, name.c_str()), 1, glm::value_ptr(vec));
	}

	void setFloat(const std::string &name, float value) const { glUniform1f(glGetUniformLocation(id, name.c_str()), value); }
};

class Mesh {
	GLuint vao, vbo, ebo;
	size_t indexCount;

  public:
	Mesh(const std::vector<float> &vertices, const std::vector<unsigned> &indices) : indexCount(indices.size()) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		glBindVertexArray(vao);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

		// Position attribute (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);

		// Normal attribute (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	~Mesh() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ebo);
	}

	void draw() const {
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
};

class GeometryFactory {
  public:
	static std::unique_ptr<Mesh> createCube(float size = 1.0f) {
		float s = size * 0.5f;
		std::vector<float> vertices = {// Front face
		                               -s, -s, s, 0, 0, 1, s, -s, s, 0, 0, 1, s, s, s, 0, 0, 1, -s, s, s, 0, 0, 1,
		                               // Back face
		                               -s, -s, -s, 0, 0, -1, -s, s, -s, 0, 0, -1, s, s, -s, 0, 0, -1, s, -s, -s, 0, 0, -1,
		                               // Top face
		                               -s, s, -s, 0, 1, 0, -s, s, s, 0, 1, 0, s, s, s, 0, 1, 0, s, s, -s, 0, 1, 0,
		                               // Bottom face
		                               -s, -s, -s, 0, -1, 0, s, -s, -s, 0, -1, 0, s, -s, s, 0, -1, 0, -s, -s, s, 0, -1, 0,
		                               // Right face
		                               s, -s, -s, 1, 0, 0, s, s, -s, 1, 0, 0, s, s, s, 1, 0, 0, s, -s, s, 1, 0, 0,
		                               // Left face
		                               -s, -s, -s, -1, 0, 0, -s, -s, s, -1, 0, 0, -s, s, s, -1, 0, 0, -s, s, -s, -1, 0, 0};
		std::vector<unsigned> indices;
		for (unsigned i = 0; i < 6; i++) {
			unsigned offset = i * 4;
			indices.insert(indices.end(), {offset, offset + 1, offset + 2, offset + 2, offset + 3, offset});
		}
		return std::make_unique<Mesh>(vertices, indices);
	}

	static std::unique_ptr<Mesh> createSphere(float radius = 1.0f, int slices = 16, int stacks = 16) {
		std::vector<float> vertices;
		std::vector<unsigned> indices;

		for (int i = 0; i <= stacks; i++) {
			float phi = M_PI * i / stacks;
			for (int j = 0; j <= slices; ++j) {
				float theta = 2.0f * M_PI * j / slices;

				float x = radius * sin(phi) * cos(theta);
				float y = radius * cos(phi);
				float z = radius * sin(phi) * sin(theta);

				glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
				vertices.insert(vertices.end(), {x, y, z, normal.x, normal.y, normal.z});
			}
		}

		for (int i = 0; i < stacks; i++) {
			for (int j = 0; j < slices; ++j) {
				unsigned first = i * (slices + 1) + j;
				unsigned second = first + slices + 1;
				indices.insert(indices.end(), {first, second, first + 1});
				indices.insert(indices.end(), {second, second + 1, first + 1});
			}
		}

		return std::make_unique<Mesh>(vertices, indices);
	}

	static std::unique_ptr<Mesh> createCylinder(float radius = 0.5f, float height = 1.0f, int segments = 16) {
		std::vector<float> vertices;
		std::vector<unsigned> indices;
		float halfHeight = height * 0.5f;

		// Side vertices
		for (int i = 0; i <= segments; i++) {
			float theta = 2.0f * M_PI * i / segments;
			float x = radius * cos(theta);
			float z = radius * sin(theta);
			glm::vec3 normal = glm::normalize(glm::vec3(x, 0, z));

			vertices.insert(vertices.end(), {x, -halfHeight, z, normal.x, normal.y, normal.z});
			vertices.insert(vertices.end(), {x, halfHeight, z, normal.x, normal.y, normal.z});
		}

		// Side indices
		for (int i = 0; i < segments; i++) {
			unsigned base = i * 2;
			indices.insert(indices.end(), {base, base + 2, base + 1});
			indices.insert(indices.end(), {base + 1, base + 2, base + 3});
		}

		return std::make_unique<Mesh>(vertices, indices);
	}
};

class ObjLoader {
  public:
	static std::unique_ptr<Mesh> load(const std::string &filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to open: " << filename << ", using default cube" << std::endl;
			return GeometryFactory::createCube();
			;
		}

		std::vector<glm::vec3> positions, normals;
		std::vector<float> vertices;
		std::vector<unsigned> indices;

		std::string line;
		while (std::getline(file, line)) {
			std::istringstream ss(line);
			std::string prefix;
			ss >> prefix;

			if (prefix == "v") {
				glm::vec3 v;
				ss >> v.x >> v.y >> v.z;
				positions.push_back(v);
			} else if (prefix == "vn") {
				glm::vec3 n;
				ss >> n.x >> n.y >> n.z;
				normals.push_back(n);
			} else if (prefix == "f") {
				for (int i = 0; i < 3; i++) {
					std::string token;
					ss >> token;
					auto parts = split(token, '/');

					int vIdx = std::stoi(parts[0]) - 1;
					int nIdx = (parts.size() > 2 && !parts[2].empty()) ? std::stoi(parts[2]) - 1 : vIdx;

					const auto &pos = positions[vIdx];
					const auto &norm = (nIdx < normals.size()) ? normals[nIdx] : glm::normalize(pos);

					vertices.insert(vertices.end(), {pos.x, pos.y, pos.z});
					vertices.insert(vertices.end(), {norm.x, norm.y, norm.z});
					indices.push_back(indices.size());
				}
			}
		}

		// Normalize to [-1, 1] cube
		normalize(vertices);

		return std::make_unique<Mesh>(vertices, indices);
	}

  private:
	static std::vector<std::string> split(const std::string &s, char delimiter) {
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream ss(s);
		while (std::getline(ss, token, delimiter)) { tokens.push_back(token); }
		return tokens;
	}

	static void normalize(std::vector<float> &vertices) {
		if (vertices.empty()) return;

		glm::vec3 min(vertices[0], vertices[1], vertices[2]);
		glm::vec3 max = min;

		for (size_t i = 0; i < vertices.size(); i += 6) {
			glm::vec3 pos(vertices[i], vertices[i + 1], vertices[i + 2]);
			min = glm::min(min, pos);
			max = glm::max(max, pos);
		}

		glm::vec3 center = (min + max) * 0.5f;
		float scale = 2.0f / glm::max(glm::max(max.x - min.x, max.y - min.y), max.z - min.z);

		for (size_t i = 0; i < vertices.size(); i += 6) {
			vertices[i] = (vertices[i] - center.x) * scale;
			vertices[i + 1] = (vertices[i + 1] - center.y) * scale;
			vertices[i + 2] = (vertices[i + 2] - center.z) * scale;
		}
	}
};

} // namespace oglprojs
#endif