#include "oglprojs.h"

#include "oglproj1.h"
#include "oglproj2.h"
#include "oglproj3.h"
#include "oglproj4.h"
#include "oglproj5.h"

#include "oglproj5_presets.h"

using namespace oglprojs;

// ============================================================================
// Application - (changed: update(), render(), initialize(), keyCallback())
// ============================================================================

class Application {
  public:
	int FPS = 60;
	float motionSpeed = 0.032f;
	unsigned int seed = 12345;

  private:
	GLFWwindow *window = nullptr;
	int width, height;
	float time;
	bool isFirstRender = true;
	bool isArticulated = false;

	OrientationType orientType = OrientationType::Quaternion;
	InterpType interpType = InterpType::CatmullRom;
	std::unique_ptr<Shader> shader;
	std::shared_ptr<MotionController> motion;

	std::vector<std::unique_ptr<Mesh>> boneMeshes;
	std::unique_ptr<ArticulatedFigure> articulated;

	PhysicsEngine physics;
	std::unique_ptr<Mesh> sphereMesh;

	std::unique_ptr<Flock> flock;
	std::unique_ptr<Mesh> boidMesh;

	std::unique_ptr<ParticleEmitter> particleEmitter;
	std::unique_ptr<Mesh> particleMesh; // reuse sphere mesh

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
            layout(location = 0) out vec4 FragColor;
            
            in vec3 FragPos;
            in vec3 Normal;
            
            uniform vec3 lightPos;
            uniform vec3 viewPos;
            uniform vec3 lightColor;
            uniform vec4 objectColor;
            
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
                
                vec3 result = (ambient + diffuse + specular) * vec3(objectColor);
                FragColor = vec4(result, objectColor.a);
            }
        )";

		shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
	}

	void update() {
		static const float loopTime = motion->totalDuration();
		time += motionSpeed;
		if (time > loopTime) time = 0.0f;
		float dt = 1.0f / float(FPS);
		physics.step(dt);

		if (flock) flock->update(dt, &physics);

		if (particleEmitter) {
			// setting emitter transform from the root motion if motion controller exists
			if (motion && particleEmitter->params.localSpace) {
				Transform tr = motion->evaluate(time, orientType, interpType);
				particleEmitter->setTransform(tr.toMatrix());
			}
			particleEmitter->update(dt, particleEmitter->params.collideWithPhysics ? &physics : nullptr, time);
			particleEmitter->applyMorphs();
		}
	}

	void renderMesh(const Mesh &meshPtr, const glm::mat4 &model, glm::vec4 color = glm::vec4(0.8f, 0.5f, 0.3f, 1.0f)) {
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
		Shader &s = *shader;
		s.set(s.U.uModel, model);
		s.set(s.U.uNormal, normalMatrix);
		s.set(s.U.uObjectColor, color);
		meshPtr.draw();
	}

	void render() {
		glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

		} else if (!boneMeshes.empty()) {
			glm::mat4 model = glm::mat4(1.0f);
			if (motion) {
				Transform trans = motion->evaluate(time, orientType, interpType);
				model = trans.toMatrix();
			}
			renderMesh(*boneMeshes[0].get(), model);
		}

		// Render physics spheres
		if (sphereMesh && shader) {
			for (const auto &b : physics.bodies) {
				glm::mat4 model = glm::translate(glm::mat4(1.0f), b.position) * glm::mat4_cast(b.orientation) *
				                  glm::scale(glm::mat4(1.0f), glm::vec3(b.radius));
				renderMesh(*sphereMesh.get(), model);
			}
		}

		if (flock && boidMesh && shader) {
			for (const auto &b : flock->boids) {
				// orient boid to face velocity direction if velocity significant
				glm::vec3 dir = b.velocity;
				glm::mat4 orient = glm::mat4(1.0f);
				if (glm::dot(dir, dir) > 1e-6f) {
					glm::vec3 forward = glm::normalize(dir);
					// build a tiny rotation that maps +Z to forward
					glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
					glm::vec3 right = glm::cross(up, forward);
					if (glm::dot(right, right) < 1e-5f) {
						// forward parallel to up; create fallback basis
						right = glm::vec3(1.0f, 0.0f, 0.0f);
					} else {
						right = glm::normalize(right);
					}
					up = glm::normalize(glm::cross(forward, right));
					glm::mat4 rot;
					rot[0] = glm::vec4(right, 0.0f);
					rot[1] = glm::vec4(up, 0.0f);
					rot[2] = glm::vec4(forward, 0.0f);
					rot[3] = glm::vec4(0, 0, 0, 1);
					orient = rot;
				}
				glm::mat4 model = glm::translate(glm::mat4(1.0f), b.position) * orient * glm::scale(glm::mat4(1.0f), glm::vec3(b.radius));
				renderMesh(*boidMesh.get(), model);
			}
		}

		if (particleEmitter && particleMesh && shader) {
			Shader &s = *shader;
			s.use();
			particleEmitter->renderAll([&](const glm::mat4 &model, const glm::vec4 &color, float size) {
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				renderMesh(*particleMesh.get(), model, color);
			});
		}
	}

	static void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
		auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
		app->width = width;
		app->height = height;
		glViewport(0, 0, width, height);
	}

	static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
		auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
		static float value = 10;
		static int ivalue = 0;
		static EmitterConfigurator cfg = EmitterConfigurator::preset(EmitterConfigurator::Fire);
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
		//
		// presets:
		if (key == GLFW_KEY_0 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			app->boneMeshes.clear();
			app->isArticulated = false;
			app->flock = nullptr;
			app->particleEmitter = nullptr;
			std::cout << "\n[CTRL+0] change preset to: null" << std::endl;
		}
		if (key == GLFW_KEY_1 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg = EmitterConfigurator::preset(EmitterConfigurator::Fountain);
			app->createParticleEmitter(cfg.params);
			std::cout << "\n[CTRL+1] change preset to: Fountain" << std::endl;
		}
		if (key == GLFW_KEY_2 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg = EmitterConfigurator::preset(EmitterConfigurator::Plasma);
			app->createParticleEmitter(cfg.params);
			std::cout << "\n[CTRL+2] change preset to: Plasma" << std::endl;
		}
		if (key == GLFW_KEY_3 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg = EmitterConfigurator::preset(EmitterConfigurator::Smoke);
			app->createParticleEmitter(cfg.params);
			std::cout << "\n[CTRL+3] change preset to: Smoke" << std::endl;
		}
		if (key == GLFW_KEY_4 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg = EmitterConfigurator::preset(EmitterConfigurator::Snow);
			app->createParticleEmitter(cfg.params);
			std::cout << "\n[CTRL+4] change preset to: Snow" << std::endl;
		}
		if (key == GLFW_KEY_5 && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg = EmitterConfigurator::preset(EmitterConfigurator::Fire_Long);
			app->createParticleEmitter(cfg.params);
			std::cout << "\n[CTRL+5] change preset to: Fire_Long" << std::endl;
		}
		//
		// scaler
		if (key == GLFW_KEY_Q && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			value *= 10;
			std::cout << "\n[CTRL+Q] change scale: " << value << std::endl;
		}
		if (key == GLFW_KEY_Q && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			value /= 10;
			std::cout << "\n[SHIFT+Q] change scale: " << value << std::endl;
		}
		//
		// usual settings
		if (key == GLFW_KEY_W && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.maxParticles += static_cast<int>(value);
			std::cout << "\n[CTRL+W] change maxParticles: " << cfg.params.maxParticles << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_W && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.maxParticles -= static_cast<int>(value);
			if (cfg.params.maxParticles < 0) cfg.params.maxParticles = 0;
			std::cout << "\n[SHIFT+W] change maxParticles: " << cfg.params.maxParticles << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_E && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.lifetimeMax += value;
			std::cout << "\n[CTRL+E] change lifetimeMax: " << cfg.params.lifetimeMax << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_E && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.lifetimeMax -= value;
			if (cfg.params.lifetimeMax < 0) cfg.params.lifetimeMax = 0;
			std::cout << "\n[SHIFT+E] change lifetimeMax: " << cfg.params.lifetimeMax << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.spread += value;
			std::cout << "\n[CTRL+R] change spread: " << cfg.params.spread << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_R && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.spread -= value;
			if (cfg.params.spread < 0) cfg.params.spread = 0;
			std::cout << "\n[SHIFT+R] change spread: " << cfg.params.spread << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_T && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.sizeMin += value;
			std::cout << "\n[CTRL+T] change sizeMin: " << cfg.params.sizeMin << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_T && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.sizeMin -= value;
			if (cfg.params.sizeMin < 0) cfg.params.sizeMin = 0;
			std::cout << "\n[SHIFT+T] change sizeMin: " << cfg.params.sizeMin << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_Y && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.sizeMax += value;
			std::cout << "\n[CTRL+Y] change sizeMax: " << cfg.params.sizeMax << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_Y && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.sizeMax -= value;
			if (cfg.params.sizeMax < 0) cfg.params.sizeMax = 0;
			std::cout << "\n[SHIFT+Y] change sizeMax: " << cfg.params.sizeMax << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		//
		// color start settings
		if (key == GLFW_KEY_A && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.r + value > 1.0f) cfg.params.colorStart.r = 1.0f;
			else cfg.params.colorStart.r += value;
			std::cout << "\n[CTRL+A] change colorStart-red: " << cfg.params.colorStart.r << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_A && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.r - value < 0) cfg.params.colorStart.r = 0;
			else cfg.params.colorStart.r -= value;
			std::cout << "\n[SHIFT+A] change colorStart-red: " << cfg.params.colorStart.r << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.g + value > 1.0f) cfg.params.colorStart.g = 1.0f;
			else cfg.params.colorStart.g += value;
			std::cout << "\n[CTRL+S] change colorStart-green: " << cfg.params.colorStart.g << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_S && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.g - value < 0) cfg.params.colorStart.g = 0;
			else cfg.params.colorStart.g -= value;
			std::cout << "\n[SHIFT+S] change colorStart-green: " << cfg.params.colorStart.g << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_D && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.b + value > 1.0f) cfg.params.colorStart.b = 1.0f;
			else cfg.params.colorStart.b += value;
			std::cout << "\n[CTRL+D] change colorStart-blue: " << cfg.params.colorStart.b << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_D && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.b - value < 0) cfg.params.colorStart.b = 0;
			else cfg.params.colorStart.b -= value;
			std::cout << "\n[SHIFT+D] change colorStart-blue: " << cfg.params.colorStart.b << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_J && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.a + value > 1.0f) cfg.params.colorStart.a = 1.0f;
			else cfg.params.colorStart.a += value;
			std::cout << "\n[CTRL+J] change colorStart-transparecy: " << cfg.params.colorStart.a << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_J && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorStart.a - value < 0) cfg.params.colorStart.a = 0;
			else cfg.params.colorStart.a -= value;
			std::cout << "\n[SHIFT+J] change colorStart-transparecy: " << cfg.params.colorStart.a << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		//
		// color end settings
		if (key == GLFW_KEY_F && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.r + value > 1.0f) cfg.params.colorEnd.r = 1.0f;
			else cfg.params.colorEnd.r += value;
			std::cout << "\n[CTRL+A] change colorEnd-red: " << cfg.params.colorEnd.r << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_F && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.r - value < 0) cfg.params.colorEnd.r = 0;
			else cfg.params.colorEnd.r -= value;
			std::cout << "\n[SHIFT+A] change colorEnd-red: " << cfg.params.colorEnd.r << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_G && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.g + value > 1.0f) cfg.params.colorEnd.g = 1.0f;
			else cfg.params.colorEnd.g += value;
			std::cout << "\n[CTRL+S] change colorEnd-green: " << cfg.params.colorEnd.g << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_G && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.g - value < 0) cfg.params.colorEnd.g = 0;
			else cfg.params.colorEnd.g -= value;
			std::cout << "\n[SHIFT+S] change colorEnd-green: " << cfg.params.colorEnd.g << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_H && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.b + value > 1.0f) cfg.params.colorEnd.b = 1.0f;
			else cfg.params.colorEnd.b += value;
			std::cout << "\n[CTRL+D] change colorEnd-blue: " << cfg.params.colorEnd.b << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_H && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.b - value < 0) cfg.params.colorEnd.b = 0;
			else cfg.params.colorEnd.b -= value;
			std::cout << "\n[SHIFT+D] change colorEnd-blue: " << cfg.params.colorEnd.b << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_K && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.a + value > 1.0f) cfg.params.colorEnd.a = 1.0f;
			else cfg.params.colorEnd.a += value;
			std::cout << "\n[CTRL+K] change colorEnd-transparecy: " << cfg.params.colorEnd.a << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_K && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			if (cfg.params.colorEnd.a - value < 0) cfg.params.colorEnd.a = 0;
			else cfg.params.colorEnd.a -= value;
			std::cout << "\n[SHIFT+K] change colorEnd-transparecy: " << cfg.params.colorEnd.a << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		//
		// noise settings
		if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.noiseAmplitude += value;
			std::cout << "\n[CTRL+U] change noiseAmplitude: " << cfg.params.noiseAmplitude << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_Z && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.noiseAmplitude -= value;
			if (cfg.params.noiseAmplitude < 0) cfg.params.noiseAmplitude = 0;
			std::cout << "\n[SHIFT+Z] change noiseAmplitude: " << cfg.params.noiseAmplitude << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_X && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.noiseFrequency += value;
			std::cout << "\n[CTRL+X] change noiseFrequency: " << cfg.params.noiseFrequency << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_X && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.noiseFrequency -= value;
			if (cfg.params.noiseFrequency < 0) cfg.params.noiseFrequency = 0;
			std::cout << "\n[SHIFT+X] change noiseFrequency: " << cfg.params.noiseFrequency << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			cfg.params.noiseTimeScale += value;
			std::cout << "\n[CTRL+C] change noiseTimeScale: " << cfg.params.noiseTimeScale << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			cfg.params.noiseTimeScale -= value;
			if (cfg.params.noiseTimeScale < 0) cfg.params.noiseTimeScale = 0;
			std::cout << "\n[SHIFT+C] change noiseTimeScale: " << cfg.params.noiseTimeScale << std::endl;
			app->createParticleEmitter(cfg.params);
		}
		//
		// mesh settings
		if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
			ivalue++;
			if (ivalue == 4) ivalue = 0;
			if (ivalue == 0) app->particleMesh = GeometryFactory::createSphere(1.0f, 10, 8);
			if (ivalue == 1) app->particleMesh = GeometryFactory::createCube();
			if (ivalue == 2) app->particleMesh = GeometryFactory::createCylinder(1.0f, 10, 8);
			if (ivalue == 3) app->particleMesh = ObjLoader::load("teapot.obj");
			std::cout << "\n[CTRL+V] changed mesh" << std::endl;
		}
		if (key == GLFW_KEY_V && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
			ivalue--;
			if (ivalue < 0) ivalue = 3;
			if (ivalue == 0) app->particleMesh = GeometryFactory::createSphere(1.0f, 10, 8);
			if (ivalue == 1) app->particleMesh = GeometryFactory::createCube();
			if (ivalue == 2) app->particleMesh = GeometryFactory::createCylinder(1.0f, 10, 8);
			if (ivalue == 3) app->particleMesh = ObjLoader::load("teapot.obj");
			std::cout << "\n[SHIFT+V] changed mesh" << std::endl;
		}
		// if (key == GLFW_KEY_E && action == GLFW_PRESS) {
		// 	std::cout << "\n[E] Exporting animation..." << std::endl;
		// 	// app->exportAnimationToGLB("exported_animation.glb");
		// 	// as if i have time to implement it with so many finals.
		// }
	}

  public:
	Application(int width = 800, int height = 600) : width(width), height(height), time(0.0f) {}

	bool initialize() {
		if (!glfwInit()) return false;

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(width, height, "OpenGL Project #5", nullptr, nullptr);
		if (!window) {
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(window);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
		glfwSetKeyCallback(window, keyCallback);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return false; }

		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); // full additive

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		glViewport(0, 0, width, height);

		createShader();
		return true;
	}

	void setController(std::shared_ptr<MotionController> controller) { motion = controller; }

	void loadModels(std::vector<std::string> filenames) {
		boneMeshes.clear();
		for (const auto &filename : filenames) boneMeshes.push_back(ObjLoader::load(filename));
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

	bool enableArticulated(bool enable = true) {
		if (enable && boneMeshes.size() >= 5 && motion) {
			isArticulated = true;
			articulated = std::make_unique<ArticulatedFigure>(motion);
		}
		if (!enable) isArticulated = false;
		return isArticulated;
	}

	void createPhysicsScene(int N = 6) {
		// create sphere mesh once
		if (!sphereMesh) sphereMesh = GeometryFactory::createSphere(1.0f, 20, 12);

		// adding a few spheres with varying radius and initial velocities
		physics.bodies.clear();
		srand(seed);
		for (int i = 0; i < N; ++i) {
			RigidBody b;
			b.radius = 0.25f + 0.15f * (i % 3);
			b.mass = glm::max(0.5f, b.radius * b.radius); // scale mass
			b.position = glm::vec3((i - N / 2) * 0.6f, 2.0f + i * 0.3f, (i % 2 == 0) ? -0.5f : 0.5f);
			b.velocity = glm::vec3((i % 2 ? 1.0f : -1.0f) * 0.5f, 0.0f, (i % 3 - 1) * 0.2f);
			b.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			b.angularVelocity = glm::vec3(0.0f, (i % 2 ? 1.0f : -1.0f) * 0.5f, 0.0f);
			b.restitution = 0.5f + 0.1f * (i % 3);
			b.finalizeParams();
			physics.addBody(b);
		}

		// adding a heavier static object (mass = 0 => static)
		RigidBody staticB;
		staticB.radius = 1.2f;
		staticB.mass = 0.0f;
		staticB.position = glm::vec3(3.0f, 0.9f, 0.0f);
		staticB.invMass = 0.0f;
		staticB.invInertia = 0.0f;
		staticB.restitution = 0.2f;
		physics.addBody(staticB);
	}

	void createFlock(int N = 48) {
		flock = std::make_unique<Flock>(N, seed);
		if (!boidMesh) boidMesh = GeometryFactory::createSphere(1.0f, 8, 6);
		flock->neighborRadius = 0.9f;
		flock->separationRadius = 0.28f;
		flock->wSeparation = 1.8f;
		flock->wAlignment = 1.0f;
		flock->wCohesion = 0.9f;
		flock->wWander = 0.15f;
		flock->wAvoid = 2.5f;
		flock->worldRadius = 10.0f;
	}

	void createParticleEmitter(const EmitterParams &params = EmitterParams()) {
		particleEmitter = std::make_unique<ParticleEmitter>(params);
		if (!particleMesh) particleMesh = GeometryFactory::createSphere(1.0f, 10, 8);
	}

	// convenience to create a tuned interesting emitter / failed to be interesting
	void createSignedPerlinEmitter() {
		EmitterParams p;
		p.emitRate = 600.0f;
		p.maxParticles = 3000;
		p.lifetimeMin = 1.0f;
		p.lifetimeMax = 3.2f;
		p.sizeMin = 0.03f;
		p.sizeMax = 0.12f;
		p.colorStart = glm::vec4(0.9f, 0.4f, 0.1f, 1.0f);
		p.colorEnd = glm::vec4(0.2f, 0.05f, 0.5f, 0.0f);
		p.velocityMin = glm::vec3(-0.6f, 2.0f, -0.6f);
		p.velocityMax = glm::vec3(0.6f, 3.5f, 0.6f);
		p.gravity = glm::vec3(0.0f, -3.2f, 0.0f);
		p.drag = 0.6f;
		p.noiseType = EmitterParams::PERLIN;
		p.noiseFrequency = 0.9f;
		p.noiseAmplitude = 2.5f;
		p.noiseTimeScale = 0.9f;
		p.spawnShape = EmitterParams::BOX;
		p.sphereRadius = 0.20f;
		p.localSpace = true;
		p.collideWithPhysics = true;
		p.restitution = 0.35f;
		createParticleEmitter(p);
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
	std::vector<std::string> fnVec; //= {"teapot.obj"};
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
		} else if (args == "-seed" && i + 1 < argc) {
			app.seed = static_cast<unsigned int>(std::stoi(argv[++i]));
			continue;
		} else if (args == "-physicscene" && i + 1 < argc) {
			int N = std::stoi(argv[++i]);
			app.createPhysicsScene(N);
			continue;
		} else if (args == "-flock" && i + 1 < argc) {
			int N = std::stoi(argv[++i]);
			app.createFlock(N);
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
			          << "  -seed <number>           Seed for random number generator in physics scene (default: 12345)\n"
			          << "  -physicscene <N>         Create physics scene with N spheres (default: 6)\n"
			          << "  -flock <N>               Create flocks with N boids (default: 48)\n"
			          << "  -h, --help               Show this help message\n"
			          << "\nParticle Emitter Keyboard Controls:\n"
			          << "  CTRL+0                   Reset: disable articulated figure, flock, and particles\n"
			          << "  CTRL+1                   Load particle preset: Fountain\n"
			          << "  CTRL+2                   Load particle preset: Plasma\n"
			          << "  CTRL+3                   Load particle preset: Smoke\n"
			          << "  CTRL+4                   Load particle preset: Snow\n"
			          << "  CTRL+5                   Load particle preset: Fire_Long\n"
			          << "\n"
			          << "  CTRL+Q / SHIFT+Q         Increase / decrease scale factor (for adjustments)\n"
			          << "  CTRL+W / SHIFT+W         Increase / decrease maxParticles\n"
			          << "  CTRL+E / SHIFT+E         Increase / decrease lifetimeMax\n"
			          << "  CTRL+R / SHIFT+R         Increase / decrease spread\n"
			          << "  CTRL+T / SHIFT+T         Increase / decrease sizeMin\n"
			          << "  CTRL+Y / SHIFT+Y         Increase / decrease sizeMax\n"
			          << "\n"
			          << "  Color Start (RGBA):\n"
			          << "  CTRL+A / SHIFT+A         Increase / decrease colorStart.r (Red)\n"
			          << "  CTRL+S / SHIFT+S         Increase / decrease colorStart.g (Green)\n"
			          << "  CTRL+D / SHIFT+D         Increase / decrease colorStart.b (Blue)\n"
			          << "  CTRL+J / SHIFT+J         Increase / decrease colorStart.a (Alpha/Transparency)\n"
			          << "\n"
			          << "  Color End (RGBA):\n"
			          << "  CTRL+F / SHIFT+F         Increase / decrease colorEnd.r (Red)\n"
			          << "  CTRL+G / SHIFT+G         Increase / decrease colorEnd.g (Green)\n"
			          << "  CTRL+H / SHIFT+H         Increase / decrease colorEnd.b (Blue)\n"
			          << "  CTRL+K / SHIFT+K         Increase / decrease colorEnd.a (Alpha/Transparency)\n"
			          << "\n"
			          << "  Noise Settings:\n"
			          << "  CTRL+Z / SHIFT+Z         Increase / decrease noiseAmplitude\n"
			          << "  CTRL+X / SHIFT+X         Increase / decrease noiseFrequency\n"
			          << "  CTRL+C / SHIFT+C         Increase / decrease noiseTimeScale\n"
			          << "\n"
			          << "  Particle Mesh:\n"
			          << "  CTRL+V / SHIFT+V         Cycle through particle mesh types (Sphere/Cube/Cylinder)\n"
			          << "\n"
			          << "Notes:\n"
			          << "  - All CTRL combinations increase values.\n"
			          << "  - All SHIFT combinations decrease values.\n"
			          << "  - Color (RGBA) values are clamped between 0.0 and 1.0.\n"
			          << "  - Values cannot go below zero.\n";
			exit(0);
		}
	}

	if (motion->totalDuration() == 0.0f) {
		motion->addKeyframe(0.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f));
		motion->addKeyframe(2.0f, glm::vec3(2.0f, 1.0f, 0.0f), glm::vec3(0.0f, glm::radians(90.0f), 0.0f));
		motion->addKeyframe(4.0f, glm::vec3(-2.0f, 1.0f, 0.0f), glm::vec3(0.0f, glm::radians(180.0f), glm::radians(90.0f)));
		motion->addKeyframe(6.0f, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f)); //
	}

	app.loadModels(fnVec);
	app.setController(motion);
	app.setInterpolation(orientType, interpType);
}

int main(int argc, char **argv) {
	Application app(800, 600);
	if (!app.initialize()) return -1;
	parseIO(argc, argv, app);
	app.run();
	return 0;
}