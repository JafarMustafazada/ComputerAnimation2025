#include "oglproj2.h" // adds Figure class;
// Shader, Model and RenderController class from oglprojs.h, includes glm;
#include "oglproj1.h" // adds MotionController

using namespace oglprojs;

// Globals
RenderController renderer;
std::unique_ptr<ArticulatedFigure> figure;
MotionController motion;
float motionTime = 0;

// Helper parsers
// keyframe string format: "x,y,z:e1,e2,e3" (dont forget, only Euler angles in degrees!)
void parseKeyframe(const std::string &str, MotionController &motion, OrientationType orientType) {
	size_t pos = str.find(':');
	if (pos == std::string::npos) return;
	std::string posStr = str.substr(0, pos);
	std::string orientStr = str.substr(pos + 1);

	glm::vec3 position(0);
	glm::vec3 euler(0);

	// Parse position
	std::stringstream ss(posStr);
	for (int i = 0; i < 3; i++) {
		std::string val;
		if (!std::getline(ss, val, ',')) break;
		position[i] = std::stof(val);
	}

	// Parse orientation
	std::stringstream ss2(orientStr);
	for (int i = 0; i < 3; i++) {
		std::string val;
		if (!std::getline(ss2, val, ',')) break;
		euler[i] = glm::radians(std::stof(val));
	}

	if (orientType == OrientationType::Quaternion) motion.addKey(position, glm::quat(euler));
	else motion.addKey(position, euler);
}
std::vector<std::string> parsePartsArg(const std::string &s) {
	std::vector<std::string> out;
	std::stringstream ss(s);
	std::string token;
	while (std::getline(ss, token, ',')) {
		if (!token.empty()) out.push_back(token);
	}
	return out;
}

void init(int argc, char **argv) {
	renderer.compileShaderAndCacheUniforms();
	glClearDepth(1.0f);

	// defaults
	std::vector<std::string> partsList = {"torso.obj", "upperL.obj", "lowerL.obj", "upperR.obj", "lowerR.obj"};
	std::vector<std::string> keyframeInputs;
	motion.orientType = OrientationType::Quaternion;
	motion.interpType = InterpType::CatmullRom;

	// parse args
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "-parts" && i + 1 < argc) {
			partsList = parsePartsArg(argv[++i]);
		} else if (a == "-kf" && i + 1 < argc) {
			std::string kfs = argv[++i];
			std::stringstream ss(kfs);
			std::string kf;
			while (std::getline(ss, kf, ';')) {
				if (!kf.empty()) keyframeInputs.push_back(kf);
			}
		} else if (a == "-ot" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "quat" || t == "quaternion" || t == "0") motion.orientType = OrientationType::Quaternion;
			else motion.orientType = OrientationType::Euler;
		} else if (a == "-it" && i + 1 < argc) {
			std::string t = argv[++i];
			if (t == "bspline" || t == "1") motion.interpType = InterpType::BSpline;
			else motion.interpType = InterpType::CatmullRom;
		} else {
			std::cout << "Usage: oglproj1 [-parts torso.obj,upperL.obj,lowerL.obj,upperR.obj,lowerR.obj] [-kf keylist] [-ot "
			             "orientationType] [-it interpType]\n";
			exit(0);
		}
	}

	// Load keyframes
	if (!keyframeInputs.empty()) {
		for (auto &kf : keyframeInputs) parseKeyframe(kf, motion, motion.orientType);
	} else {
		// default loop path (square)
		motion.addKey(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(0.0f), 0.0f)));
		motion.addKey(glm::vec3(4.0f, 0.0f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(0.0f), 0.0f)));
		motion.addKey(glm::vec3(4.0f, 0.0f, 4.0f), glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)));
		motion.addKey(glm::vec3(0.0f, 0.0f, 4.0f), glm::quat(glm::vec3(0.0f, glm::radians(180.0f), 0.0f)));
	}

	// instantiate figure and load parts (if partsList empty, fallback model is used)
	figure = std::make_unique<ArticulatedFigure>();
	figure->loadParts(partsList);
}

// each frame update
void update() {
	motionTime += 0.01f; // speed
	if (motionTime > 1.0f) motionTime = 0.0f;
	renderer.modelMat = motion.getTransform(motionTime);
}

void render() {
	Shader &shader = *renderer.shader;
	glClearColor(0.2f, 0.25f, 0.28f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -8));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), float(renderer.W) / renderer.H, 0.1f, 200.0f);
	shader.use();

	struct RunOnce {
		RunOnce(Shader &shader) {
			shader.set(renderer.U.uLightPos, glm::vec3(5, 5, 5));
			shader.set(renderer.U.uLightAmbient, glm::vec3(0.4f));
			shader.set(renderer.U.uLightDiffuse, glm::vec3(0.3f));
			shader.set(renderer.U.uLightSpecular, glm::vec3(0.4f));
			shader.set(renderer.U.uMatAmbient, glm::vec3(0.11f, 0.06f, 0.11f));
			shader.set(renderer.U.uMatDiffuse, glm::vec3(0.43f, 0.47f, 0.54f));
			shader.set(renderer.U.uMatSpecular, glm::vec3(0.33f, 0.33f, 0.52f));
			shader.set(renderer.U.uMatEmission, glm::vec3(0.1f, 0, 0.1f));
			shader.set(renderer.U.uMatShininess, 10.0f);
			shader.set(renderer.U.uViewPos, glm::vec3(0, 0, 5));
		}
	} static runOnce(shader);

	// computing a stable root transform for the figure from the motion controller
	glm::mat4 root = motion.getTransform(motionTime);

	// scaling root a bit for visual clarity
	glm::mat4 rootScaled = root * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

	figure->draw(rootScaled, renderer, motionTime);
	shader.set(renderer.U.uView, view);
	shader.set(renderer.U.uProj, proj);
}

// Input/resize
void key(GLFWwindow *w, int k, int, int, int) {
	if (k == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
	// simple controls to change gait speed
	if (k == GLFW_KEY_UP) figure->stepFrequency += 0.1f;
	if (k == GLFW_KEY_DOWN) figure->stepFrequency = std::max(0.1f, figure->stepFrequency - 0.1f);
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
	GLFWwindow *win = glfwCreateWindow(renderer.W, renderer.H, "OpenGLProject02", NULL, NULL);
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