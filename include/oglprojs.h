#ifndef OGLPROJS_H
#define OGLPROJS_H

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace oglprojs {
// Shader sources
const char *vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 model, view, projection;
uniform mat3 normalMatrix;
out vec3 FragPos, Normal;
void main() {
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos,1.0);
}
)";
const char *fsSrc = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos, Normal;
uniform vec3 lightPos, lightAmbient, lightDiffuse, lightSpecular;
uniform vec3 materialAmbient, materialDiffuse, materialSpecular, materialEmission;
uniform float materialShininess;
uniform vec3 viewPos;
void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    vec3 ambient = lightAmbient * materialAmbient;
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = lightDiffuse * (diff * materialDiffuse);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);
    vec3 specular = lightSpecular * (spec * materialSpecular);
    FragColor = vec4(ambient + diffuse + specular + materialEmission, 1.0);
}
)";
// cube obj fallback
const char *DEFAULT_OBJ = R"(
v -1.0 -1.0 -1.0
v  1.0 -1.0 -1.0
v  1.0  1.0 -1.0
v -1.0  1.0 -1.0
v -1.0 -1.0  1.0
v  1.0 -1.0  1.0
v  1.0  1.0  1.0
v -1.0  1.0  1.0
vn  0.0  0.0 -1.0
vn  0.0  0.0  1.0
vn -1.0  0.0  0.0
vn  1.0  0.0  0.0
vn  0.0 -1.0  0.0
vn  0.0  1.0  0.0
f 1//1 2//1 3//1
f 3//1 4//1 1//1
f 5//2 8//2 7//2
f 7//2 6//2 5//2
f 1//3 4//3 8//3
f 8//3 5//3 1//3
f 2//4 6//4 7//4
f 7//4 3//4 2//4
f 4//5 3//5 7//5
f 7//5 8//5 4//5
f 1//6 5//6 6//6
f 6//6 2//6 1//6
)";

// Shader class
struct Shader {
	unsigned int ID;
	Shader(const char *vs, const char *fs) {
		auto compile = [](const char *src, GLenum type) {
			unsigned int s = glCreateShader(type);
			glShaderSource(s, 1, &src, NULL);
			glCompileShader(s);
			int ok;
			glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[512];
				glGetShaderInfoLog(s, 512, NULL, log);
				std::cerr << log << std::endl;
			}
			return s;
		};
		unsigned v = compile(vs, GL_VERTEX_SHADER), f = compile(fs, GL_FRAGMENT_SHADER);
		ID = glCreateProgram();
		glAttachShader(ID, v);
		glAttachShader(ID, f);
		glLinkProgram(ID);
		int ok;
		glGetProgramiv(ID, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[512];
			glGetProgramInfoLog(ID, 512, NULL, log);
			std::cerr << log << std::endl;
		}
		glDeleteShader(v);
		glDeleteShader(f);
	}
	~Shader() { glDeleteProgram(ID); }
	void use() const { glUseProgram(ID); }
	// void set(const char *n, const glm::mat4 &m) const { glUniformMatrix4fv(glGetUniformLocation(ID, n), 1, GL_FALSE, glm::value_ptr(m));
	// } void set(const char *n, const glm::mat3 &m) const { glUniformMatrix3fv(glGetUniformLocation(ID, n), 1, GL_FALSE,
	// glm::value_ptr(m)); } void set(const char *n, const glm::vec3 &v) const { glUniform3fv(glGetUniformLocation(ID, n), 1,
	// glm::value_ptr(v)); } void set(const char *n, float v) const { glUniform1f(glGetUniformLocation(ID, n), v); }
	void set(GLint &u, const glm::mat4 &m) const { glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(m)); }
	void set(GLint &u, const glm::mat3 &m) const { glUniformMatrix3fv(u, 1, GL_FALSE, glm::value_ptr(m)); }
	void set(GLint &u, const glm::vec3 &v) const { glUniform3fv(u, 1, glm::value_ptr(v)); }
	void set(GLint &u, float v) const { glUniform1f(u, v); }
};

// Mesh class
struct Mesh {
	std::vector<float> verts;
	std::vector<unsigned> idx;
	unsigned VAO = 0, VBO = 0, EBO = 0;
	~Mesh() {
		if (VAO) glDeleteVertexArrays(1, &VAO);
		if (VBO) glDeleteBuffers(1, &VBO);
		if (EBO) glDeleteBuffers(1, &EBO);
	}
	void setup() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindVertexArray(0);
	}
	void draw() const {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, idx.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
};

// Model loader
struct Model {
	Mesh mesh;

	bool loadFromString(const std::string &data) {
		std::istringstream f(data);
		std::vector<glm::vec3> v, n;
		std::vector<unsigned> vi, ni;
		std::string l;
		while (std::getline(f, l)) {
			std::istringstream s(l);
			std::string t;
			s >> t;
			if (t == "v") {
				glm::vec3 p;
				s >> p.x >> p.y >> p.z;
				v.push_back(p);
			} else if (t == "vn") {
				glm::vec3 p;
				s >> p.x >> p.y >> p.z;
				n.push_back(p);
			} else if (t == "f") {
				for (int i = 0; i < 3; ++i) {
					std::string w;
					s >> w;
					size_t p1 = w.find('/'), p2 = w.find('/', p1 + 1);
					unsigned vi_ = std::stoi(w.substr(0, p1)) - 1, ni_ = vi_;
					if (p2 != std::string::npos && p2 + 1 < w.size()) ni_ = std::stoi(w.substr(p2 + 1)) - 1;
					vi.push_back(vi_);
					ni.push_back(ni_);
				}
			}
		}
		if (v.empty()) return false;
		glm::vec3 min = v[0], max = v[0];
		for (auto &p : v) {
			min = glm::min(min, p);
			max = glm::max(max, p);
		}
		glm::vec3 c = (min + max) * 0.5f, sz = max - min;
		float sc = 2.0f / glm::max(sz.x, glm::max(sz.y, sz.z));
		mesh.verts.clear();
		mesh.idx.clear();
		for (size_t i = 0; i < vi.size(); ++i) {
			glm::vec3 p = (v[vi[i]] - c) * sc;
			mesh.verts.insert(mesh.verts.end(), {p.x, p.y, p.z});
			glm::vec3 norm = ni[i] < n.size() ? n[ni[i]] : glm::normalize(p);
			mesh.verts.insert(mesh.verts.end(), {norm.x, norm.y, norm.z});
			mesh.idx.push_back(i);
		}
		mesh.setup();
		return true;
	}

	bool load(const std::string &fn = "") {
		std::ifstream f(fn);
		if (!f.is_open()) {
			loadFromString(DEFAULT_OBJ);
			return false;
		}
		std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		return loadFromString(data);
	}

	void draw() const { mesh.draw(); }
};

class RenderController {
  public:
	struct CachedUniforms {
		GLint uModel, uView, uProj, uNormal;
		GLint uLightPos, uLightAmbient, uLightDiffuse, uLightSpecular;
		GLint uMatAmbient, uMatDiffuse, uMatSpecular, uMatEmission, uMatShininess;
		GLint uViewPos;
	};
	CachedUniforms U;
	std::unique_ptr<oglprojs::Shader> shader;
	int W = 600, H = 600;
	int angle = 0;
	const int FPS = 60;
	std::chrono::steady_clock::time_point lastTime;
	glm::mat4 modelMat = glm::mat4(1.0f);

	void compileShaderAndCacheUniforms(const char *vs = vsSrc, const char *fs = fsSrc) {
		shader = std::make_unique<oglprojs::Shader>(vs, fs);
		Shader &s = *shader;
		U.uModel = glGetUniformLocation(s.ID, "model");
		U.uView = glGetUniformLocation(s.ID, "view");
		U.uProj = glGetUniformLocation(s.ID, "projection");
		U.uNormal = glGetUniformLocation(s.ID, "normalMatrix");
		U.uLightPos = glGetUniformLocation(s.ID, "lightPos");
		U.uLightAmbient = glGetUniformLocation(s.ID, "lightAmbient");
		U.uLightDiffuse = glGetUniformLocation(s.ID, "lightDiffuse");
		U.uLightSpecular = glGetUniformLocation(s.ID, "lightSpecular");
		U.uMatAmbient = glGetUniformLocation(s.ID, "materialAmbient");
		U.uMatDiffuse = glGetUniformLocation(s.ID, "materialDiffuse");
		U.uMatSpecular = glGetUniformLocation(s.ID, "materialSpecular");
		U.uMatEmission = glGetUniformLocation(s.ID, "materialEmission");
		U.uMatShininess = glGetUniformLocation(s.ID, "materialShininess");
		U.uViewPos = glGetUniformLocation(s.ID, "viewPos");
	}
};
} // namespace oglprojs
#endif // OGLPROJS_H