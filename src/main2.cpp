#include "oglprojs.h"

#include "oglproj1.h"
#include "oglproj2.h"

using namespace oglprojs;

// ============================================================================
// Application - (changed: render(), loadModels(), enableArticulated())
// ============================================================================

class Application {
  public:
	int FPS = 60;
	float motionSpeed = 0.032f;

  private:
	GLFWwindow *window = nullptr;
	int width, height;
	float time;
	bool isFirstRender = true;
	bool isArticulated = false; // for proj2

	std::unique_ptr<Shader> shader;
	std::vector<std::unique_ptr<Mesh>> boneMeshes;  // for proj2
	std::unique_ptr<ArticulatedFigure> articulated; // for proj2
	std::shared_ptr<MotionController> motion;

	OrientationType orientType = OrientationType::Quaternion;
	InterpType interpType = InterpType::CatmullRom;

	void createShader() {
		const std::string vertexSrc = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            
            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;
            uniform mat3 normalMatrix;
            
            out vec3 FragPos;
            out vec3 Normal;
            
            void main() {
                FragPos = vec3(model * vec4(aPos, 1.0));
                Normal = normalMatrix * aNormal;
                gl_Position = projection * view * vec4(FragPos, 1.0);
            }
        )";

		const std::string fragmentSrc = R"(
            #version 330 core
            out vec4 FragColor;
            
            in vec3 FragPos;
            in vec3 Normal;
            
            uniform vec3 lightPos;
            uniform vec3 viewPos;
            uniform vec3 objectColor;
            uniform vec3 lightColor;
            
            void main() {
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(lightPos - FragPos);
                vec3 viewDir = normalize(viewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                
                float ambientStrength = 0.3;
                vec3 ambient = ambientStrength * lightColor;
                
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * lightColor;
                
                float specularStrength = 0.5;
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
                vec3 specular = specularStrength * spec * lightColor;
                
                vec3 result = (ambient + diffuse + specular) * objectColor;
                FragColor = vec4(result, 1.0);
            }
        )";

		shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
	}

	void update() {
		static const float loopTime = motion->totalDuration();
		time += motionSpeed;
		if (time > loopTime) time = 0.0f;
	}

	void render() {
		glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!shader) return;
		Shader &s = *shader;
		s.use();

		if (isFirstRender) {
			s.CacheUniforms();
			isFirstRender = false;
			s.set(s.U.uViewPos, glm::vec3(0.0f, 2.0f, 5.0f));
			s.set(s.U.uLightPos, glm::vec3(5.0f, 5.0f, 5.0f));
			s.set(s.U.uLightAmbient, glm::vec3(0.4f));
			s.set(s.U.uLightDiffuse, glm::vec3(0.3f));
			s.set(s.U.uLightSpecular, glm::vec3(0.4f));
			s.set(s.U.uLightColor, glm::vec3(1.0f, 1.0f, 1.0f));
			s.set(s.U.uMatAmbient, glm::vec3(0.11f, 0.06f, 0.11f));
			s.set(s.U.uMatDiffuse, glm::vec3(0.43f, 0.47f, 0.54f));
			s.set(s.U.uMatSpecular, glm::vec3(0.33f, 0.33f, 0.52f));
			s.set(s.U.uMatEmission, glm::vec3(0.1f, 0, 0.1f));
			s.set(s.U.uMatShininess, 10.0f);
		}

		// Camera
		glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), float(width) / float(height), 0.1f, 100.0f);
		s.set(s.U.uView, view);
		s.set(s.U.uProj, projection);

		// Render if available
		if (boneMeshes.empty()) return;
		if (isArticulated) {
			static std::unique_ptr<ArticulatedFigure> articulated = std::make_unique<ArticulatedFigure>(motion);
			glm::mat4 *boneModels = new glm::mat4[5];
			articulated->evaluateBones(time, orientType, interpType, boneModels);

			// torso: draw original mesh at torso transform (assuming mesh is torso)
			renderMesh(*boneMeshes[0].get(), boneModels[0]);

			// draw limbs (thigh/shin):
			glm::mat4 thighScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, articulated->thighLength * 1.0f, 0.25f));
			glm::mat4 shinScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, articulated->shinLength * 1.0f, 0.25f));

			// left thigh (index 1), left shin (index 2)
			renderMesh(*boneMeshes[1].get(), boneModels[1] * thighScale);
			renderMesh(*boneMeshes[2].get(), boneModels[2] * shinScale);

			// right thigh (index 3), right shin (index 4)
			renderMesh(*boneMeshes[3].get(), boneModels[3] * thighScale);
			renderMesh(*boneMeshes[4].get(), boneModels[4] * shinScale);

		} else {
			glm::mat4 model = glm::mat4(1.0f);
			if (motion) {
				Transform trans = motion->evaluate(time, orientType, interpType);
				model = trans.toMatrix();
			}
			renderMesh(*boneMeshes[0].get(), model);
		}
	}

	void renderMesh(const Mesh &meshPtr, const glm::mat4 &model) {
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
		Shader &s = *shader;
		s.set(s.U.uModel, model);
		s.set(s.U.uNormal, normalMatrix);
		s.set(s.U.uObjectColor, glm::vec3(0.8f, 0.5f, 0.3f));
		meshPtr.draw();
	}

	static void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
		auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
		app->width = width;
		app->height = height;
		glViewport(0, 0, width, height);
	}

	static void keyCallback(GLFWwindow *window, int key, int, int action, int) {
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

  public:
	Application(int width = 800, int height = 600) : width(width), height(height), time(0.0f) {}

	bool initialize() {
		if (!glfwInit()) return false;

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(width, height, "OpenGL Project #2", nullptr, nullptr);
		if (!window) {
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(window);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
		glfwSetKeyCallback(window, keyCallback);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return false; }

		glEnable(GL_DEPTH_TEST);
		glViewport(0, 0, width, height);

		createShader();
		return true;
	}

	void setController(std::shared_ptr<MotionController> controller) { motion = controller; }

	void loadModels(std::vector<std::string> filenames) {
		boneMeshes.clear();
		for (const auto &filename : filenames) boneMeshes.push_back(ObjLoader::load(filename));
	}

	bool enableArticulated(bool enable = true) {
		if (enable && boneMeshes.size() >= 5 && motion) {
			isArticulated = true;
			articulated = std::make_unique<ArticulatedFigure>(motion);
		}
		if (!enable) isArticulated = false;
		return isArticulated;
	}

	void setInterpolation(OrientationType ot, InterpType it) {
		orientType = ot;
		interpType = it;
	}

	void run() {
		int frameTime = 1000 / FPS;
		std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
		while (!glfwWindowShouldClose(window)) {
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count() >= frameTime) {
				update();
				lastTime = now;
			}
			render();
			glfwSwapBuffers(window);
			glfwPollEvents();
		}
	}

	~Application() {
		if (window) glfwDestroyWindow(window);
		glfwTerminate();
	}
};

// Helper to parse keyframe string format: "x,y,z:e1,e2,e3"
void parseKeyframe(const std::string &str, MotionController &motion) {
	size_t pos = str.find(':');
	if (pos == std::string::npos) return;
	std::string posStr = str.substr(0, pos);
	std::string orientStr = str.substr(pos + 1);

	glm::vec3 position(0);
	glm::vec3 euler(0);

	// Parse position
	std::stringstream ss(posStr);
	for (int i = 0; i < 3; ++i) {
		std::string val;
		if (!std::getline(ss, val, ',')) break;
		position[i] = std::stof(val);
	}

	// Parse orientation
	std::stringstream ss2(orientStr);
	for (int i = 0; i < 3; ++i) {
		std::string val;
		if (!std::getline(ss2, val, ',')) break;
		euler[i] = glm::radians(std::stof(val));
	}

	motion.addKeyframe(position, euler);
}

void parseIO(int argc, char **argv, Application &app) {
	std::vector<std::string> fnVec = {"teapot.obj"};
	auto motion = std::make_shared<MotionController>();
	OrientationType orientType = OrientationType::Quaternion;
	InterpType interpType = InterpType::CatmullRom;

	// Processing command line arguments
	for (int i = 1; i < argc; i++) {
		std::string args = argv[i];
		if (args == "-ot" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "quaternion" || t == "0") orientType = OrientationType::Quaternion;
			else if (t == "euler" || t == "1") orientType = OrientationType::Euler;
			else std::cerr << "Unknown orientation type: " << t << "\n";
			continue;
		} else if (args == "-it" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "catmullrom" || t == "0") interpType = InterpType::CatmullRom;
			else if (t == "bspline" || t == "1") interpType = InterpType::BSpline;
			else std::cerr << "Unknown interpolation type: " << t << "\n";
			continue;
		} else if (args == "-kf" && i + 1 < argc) {
			std::string kfs = argv[++i];
			std::stringstream ss(kfs);
			std::string kf;
			while (std::getline(ss, kf, ';')) { parseKeyframe(kf, *motion); }
			continue;
		} else if (args == "-fn" && i + 1 < argc) {
			std::string fn = argv[++i];
			fnVec.push_back(fn);
			continue;
		} else if (args == "-articulated") {
			app.enableArticulated();
			continue;
		} else if (args == "-h" || args == "--help") {
			std::cout << "Usage: " << argv[0] << " [options]\n"
			          << "Options:\n"
			          << "  -ot <type>               Orientation type: quaternion|0 or euler|1 (default: quaternion)\n"
			          << "  -it <type>               Interpolation type: catmullrom|0 or bspline|1 (default: catmullrom)\n"
			          << "  -kf <kf1;kf2;...>        Keyframes in format x,y,z:e1,e2,e3 separated by semicolons\n"
			          << "  -fn <filename>           Additional model filename to load (OBJ format)\n"
			          << "  -articulated             Enable articulated figure rendering (requires 5 meshes)\n"
			          << " example: -fn data/n1.obj, -fn data/n2.obj, -fn data/n3.obj, -fn data/nr.obj, -fn data/n5.obj.\n"
			          << " articulated figure order: torso, left thigh, left shin, right thigh, right shin.\n"
			          << "  -h, --help               Show this help message\n";
			exit(0);
		}
	}

	if (motion->totalDuration() == 0.0f) {
		// Add default keyframes if none provided
		motion->addKeyframe(0.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f));
		motion->addKeyframe(2.0f, glm::vec3(2.0f, 1.0f, 0.0f), glm::vec3(0.0f, glm::radians(90.0f), 0.0f));
		motion->addKeyframe(4.0f, glm::vec3(-2.0f, 1.0f, 0.0f), glm::vec3(0.0f, glm::radians(180.0f), glm::radians(90.0f)));
		motion->addKeyframe(6.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f));
	}

	app.loadModels(fnVec);
	app.setController(motion);
	app.setInterpolation(orientType, interpType);
}

int main(int argc, char **argv) {
	Application app(800, 600);
	if (!app.initialize()) return -1;
	parseIO(argc, argv, app);
	app.loadModels({"teapot.obj", "left_thigh.obj", "left_shin.obj", "right_thigh.obj", "right_shin.obj"});
	app.enableArticulated();
	app.run();
	return 0;
}