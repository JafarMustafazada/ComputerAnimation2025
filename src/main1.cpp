#include "oglproj1.h" // adds Motion Controller
#include "oglprojs.h" // adds helping Shader, Model and RenderController class; includes glm, iostream, fstream, sstream

using namespace oglprojs;


// Globals
RenderController renderer;
std::unique_ptr<oglprojs::Model> model;
MotionController motion;
float motionTime = 0;

// Helper to parse keyframe string format: "x,y,z:e1,e2,e3" (dont forget, only Euler angles in degrees!)
void parseKeyframe(const std::string &str, MotionController &motion, OrientationType orientType) {
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

	if (orientType == OrientationType::Quaternion) motion.addKey(position, glm::quat(euler));
	else motion.addKey(position, euler);
}

void init(int argc, char **argv) {
	glClearDepth(1.0f);
	model = std::make_unique<oglprojs::Model>();
	renderer.compileShaderAndCacheUniforms();
	std::string fn = "teapot.obj";
	std::vector<std::string> keyframeInputs;

	// Processing command line arguments
	for (int i = 1; i < argc; i++) {
		std::string args = argv[i];
		if (args == "-ot" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "quat" || t == "quaternion" || t == "0") motion.orientType = OrientationType::Quaternion;
			else if (t == "euler" || t == "1") motion.orientType = OrientationType::Euler;
			else std::cerr << "Unknown orientation type: " << t << "\n";
			continue;
		} else if (args == "-it" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "crspline" || t == "catmullrom" || t == "0") motion.interpType = InterpType::CatmullRom;
			else if (t == "bspline" || t == "1") motion.interpType = InterpType::BSpline;
			else std::cerr << "Unknown interpolation type: " << t << "\n";
			continue;
		} else if (args == "-m" && i + 1 < argc) {
			fn = argv[++i];
			continue;
		} else if (args == "-kf" && i + 1 < argc) {
			std::string kfs = argv[++i];
			std::stringstream ss(kfs);
			std::string kf;
			while (std::getline(ss, kf, ';')) {
				if (!kf.empty()) keyframeInputs.push_back(kf);
			}
			continue;
		} else if (args == "-h" || args == "--help") {
			std::cout << "Usage: oglproj1 [options]\n";
			std::cout << "Options:\n";
			std::cout << "  -ot <type> Orientation type: quat/quaternion/0 (default), euler/1\n";
			std::cout << "  -it <type> Interpolation type: crspline/catmullrom/0 (default), bspline/b-spline/1\n";
			std::cout << "  -m <file>  File path, loads models with `.obj` extension (default: cube or `teapot.obj` file)\n";
			std::cout << "  -kf <list> Keyframes, format: \"x,y,z:e1,e2,e3;...\" (Euler angles in degrees)\n";
			std::cout << "  -h, --help Show this help message\n";
			exit(0);
		} else {
			std::cerr << "Unknown argument: " << args << "\n";
			std::cerr << "Use -h or --help for usage.\n";
			exit(1);
		}
	}

	model->load(fn);

	if (!keyframeInputs.empty()) {
		for (const auto &kf : keyframeInputs) parseKeyframe(kf, motion, motion.orientType);
	} else {
		// Default keyframes
		motion.addKey(glm::vec3(0, 0, 0), glm::quat(glm::vec3(0, 0, 0)));
		motion.addKey(glm::vec3(2, 0, 0), glm::quat(glm::vec3(0, glm::radians(90.0f), 0)));
		motion.addKey(glm::vec3(2, 2, 0), glm::quat(glm::vec3(glm::radians(90.0f), glm::radians(90.0f), 0)));
		motion.addKey(glm::vec3(0, 2, 0), glm::quat(glm::vec3(glm::radians(180.0f), 0, 0)));
	}
}

// each frame update
void update() {
	motionTime += 0.01f; // speed
	if (motionTime > 1.0f) motionTime = 0.0f;
	renderer.modelMat = motion.getTransform(motionTime);
}

void render() {
	static Shader &shader = *renderer.shader;
	static Model &model1 = *model;

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glm::mat4 view = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), float(renderer.W) / renderer.H, 1.0f, 2000.0f);
	shader.use();

	struct RunOnce {
		RunOnce(Shader &shader) {
			shader.set(renderer.U.uLightPos, glm::vec3(5, 5, 5));
			shader.set(renderer.U.uLightAmbient, glm::vec3(0.4f));
			shader.set(renderer.U.uLightDiffuse, glm::vec3(0.3f));
			shader.set(renderer.U.uLightSpecular, glm::vec3(0.4f));
			shader.set(renderer.U.uMatAmbient, glm::vec3(0.11f, 0.06f, 0.11f));
			shader.set(renderer.U.uMatDiffuse, glm::vec3(0.43f, 0.47f, 0.54f));
			shader.set(renderer.U.uMatDiffuse, glm::vec3(0.33f, 0.33f, 0.52f));
			shader.set(renderer.U.uMatEmission, glm::vec3(0.1f, 0, 0.1f));
			shader.set(renderer.U.uMatShininess, 10.0f);
			shader.set(renderer.U.uViewPos, glm::vec3(0, 0, 5));
		}
	} static runOnce(shader);

	shader.set(renderer.U.uModel, renderer.modelMat);
	shader.set(renderer.U.uView, view);
	shader.set(renderer.U.uProj, proj);
	shader.set(renderer.U.uNormal, glm::transpose(glm::inverse(glm::mat3(renderer.modelMat))));
	model1.draw();
}

// Input/resize
void key(GLFWwindow *w, int k, int, int, int) {
	if (k == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
}
void resize(GLFWwindow *, int w, int h) {
	renderer.W = w;
	renderer.H = h;
	glViewport(0, 0, w, h);
}

int main(int argc, char **argv) {
	if (!glfwInit()) return -1;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow *win = glfwCreateWindow(renderer.W, renderer.H, "OpenGLProject01", NULL, NULL);
	if (!win) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(win);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
	init(argc, argv);
	glfwSetKeyCallback(win, key);
	glfwSetFramebufferSizeCallback(win, resize);
	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(win)) {
		auto now = std::chrono::steady_clock::now();
		static const int frameTime = 1000 / renderer.FPS;
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count() >= frameTime) {
			// from exercise 0 //
			// angle = (angle + 5) % 360;
			// modelMat = glm::rotate(glm::mat4(1), glm::radians(float(angle)), glm::vec3(0, 1, 0));
			update();
			lastTime = now;
		}
		render();
		glfwSwapBuffers(win);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}