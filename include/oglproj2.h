#ifndef OGLPROJ2_H
#define OGLPROJ2_H

#include "oglprojs.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace oglprojs {
struct ArticulatedFigure {
	std::unique_ptr<Model> torso;
	std::unique_ptr<Model> upperL; // left upper leg (thigh)
	std::unique_ptr<Model> lowerL; // left lower leg (shin)
	std::unique_ptr<Model> upperR;
	std::unique_ptr<Model> lowerR;

	// Simple geometry parameters (in world units)
	float thighLength = 0.9f;
	float shinLength = 0.9f;
	float hipOffset = 0.25f; // lateral offset of hips from root center
	glm::vec3 torsoScale = glm::vec3(0.6f, 1.0f, 0.3f);

	// gait parameters (tweak these)
	float stepFrequency = 1.0f;                // steps per second (one step = half gait cycle per leg)
	float hipAmplitude = glm::radians(25.0f);  // how far hip swings forward/back
	float kneeAmplitude = glm::radians(40.0f); // knee flex amplitude
	float phaseOffset = glm::pi<float>();      // right leg phase offset (anti-phase)

	// If parts not provided, we fallback to this one model
	void loadParts(const std::vector<std::string> &partFiles) {
		// allocate models
		torso = std::make_unique<Model>();
		upperL = std::make_unique<Model>();
		lowerL = std::make_unique<Model>();
		upperR = std::make_unique<Model>();
		lowerR = std::make_unique<Model>();

		auto tryLoadOrFallback = [&](std::unique_ptr<Model> &m, const std::string &fn) {
			if (!fn.empty() && !m->load(fn)) std::cout << "Warning: failed to load " << fn << " - using fallback\n";
		};

		// Expect order: torso, upperL, lowerL, upperR, lowerR (partial lists allowed)
		std::vector<std::string> pf = partFiles;
		pf.resize(5); // empty strings for missing parts
		tryLoadOrFallback(torso, pf[0]);
		tryLoadOrFallback(upperL, pf[1]);
		tryLoadOrFallback(lowerL, pf[2]);
		tryLoadOrFallback(upperR, pf[3]);
		tryLoadOrFallback(lowerR, pf[4]);
	}

	// Evaluate the gait angles for a leg at time t (seconds), with phase shift
	// returns pair (hipAngle, kneeAngle) in radians
	std::pair<float, float> gaitAngles(float t, float phase) const {
		// angular frequency
		float w = 2.0f * glm::pi<float>() * stepFrequency;
		// hip: sinusoidal forward/back swing
		float hip = hipAmplitude * std::sin(w * t + phase);
		// knee: flex more when the leg is in swing (using half-wave rectified sin)
		float swing = std::sin(w * t + phase);
		float knee = 0.0f;
		if (swing > 0.0f) {
			// swing phase: flex knee
			knee = kneeAmplitude * swing;
		} else {
			// stance phase: small bend to support
			knee = kneeAmplitude * 0.15f * std::abs(swing);
		}
		return {hip, knee};
	}

	// Given the root transform (world) and current time, compute & draw parts
	void draw(const glm::mat4 &rootTransform, RenderController &renderer, float t) {
		Shader &sh = *renderer.shader;

		// Root / torso
		{
			glm::mat4 M = rootTransform;
			// apply torso scaling and a vertical offset so feet are under root
			// We assume the torso model is centered at origin and height ~1
			M = M * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, thighLength + shinLength, 0.0f));
			M = M * glm::scale(glm::mat4(1.0f), torsoScale);
			sh.use();
			sh.set(renderer.U.uModel, M);
			sh.set(renderer.U.uNormal, glm::transpose(glm::inverse(glm::mat3(M))));
			torso->draw();
		}

		// Helper to draw a single leg (sideMultiplier = +1 for left, -1 for right)
		auto drawLeg = [&](float sideMultiplier, std::unique_ptr<Model> &upper, std::unique_ptr<Model> &lower, float phaseShift) {
			// compute gait angles
			auto [hipAngle, kneeAngle] = gaitAngles(t, phaseShift);

			// hip location in root frame
			glm::vec3 hipLocal = glm::vec3(sideMultiplier * hipOffset, thighLength + shinLength, 0.0f);

			// hip transform: root * translate(hipLocal) * rotate( hipAngle about X ) *
			//                 translate thigh half to position the upper leg mesh
			glm::mat4 Troot = rootTransform;
			glm::mat4 Hip = Troot * glm::translate(glm::mat4(1.0f), hipLocal);
			// rotate around local X (pitch)
			Hip = Hip * glm::rotate(glm::mat4(1.0f), hipAngle, glm::vec3(1, 0, 0));

			// upper leg model matrix (place upper leg so it extends along -Y in local model)
			glm::mat4 UpperModel = Hip;
			// move model's origin to mid-thigh (so scaling looks like a thigh)
			UpperModel = UpperModel * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength * 0.5f, 0.0f));
			// scale to approximate a thigh cylinder
			UpperModel = UpperModel * glm::scale(glm::mat4(1.0f), glm::vec3(0.18f, thighLength, 0.18f));

			sh.use();
			sh.set(renderer.U.uModel, UpperModel);
			sh.set(renderer.U.uNormal, glm::transpose(glm::inverse(glm::mat3(UpperModel))));
			upper->draw();

			// knee position in world: Hip * translate(0, -thighLength, 0)
			glm::mat4 Knee = Hip * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength, 0.0f));
			// knee rotation (about X for flexion)
			Knee = Knee * glm::rotate(glm::mat4(1.0f), kneeAngle, glm::vec3(1, 0, 0));

			// lower leg model matrix (mid shin)
			glm::mat4 LowerModel = Knee;
			LowerModel = LowerModel * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -shinLength * 0.5f, 0.0f));
			LowerModel = LowerModel * glm::scale(glm::mat4(1.0f), glm::vec3(0.16f, shinLength, 0.16f));

			sh.use();
			sh.set(renderer.U.uModel, LowerModel);
			sh.set(renderer.U.uNormal, glm::transpose(glm::inverse(glm::mat3(LowerModel))));
			lower->draw();
		};

		// drawing left leg (phase = 0) and right leg (phase = pi)
		drawLeg(+1.0f, upperL, lowerL, 0.0f);
		drawLeg(-1.0f, upperR, lowerR, phaseOffset);
	}
};
} // namespace oglprojs
#endif // OGLPROJ2_H