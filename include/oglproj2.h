#ifndef OGLPROJ2_H
#define OGLPROJ2_H

#include "oglprojs.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <glad/glad.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace oglprojs {

// A small, generic FK-based articulated figure that works with any limb topology.
// - joints[] describes a tree (parentIndex < 0 => root).
// - each joint can reference a Model to be drawn at that joint.
// - rotations are per-joint Euler angles (radians).
struct ArticulatedFigure {
	struct Joint {
		std::string name;
		int parentIndex = -1;
		glm::vec3 localPos = glm::vec3(0.0f); // translation from parent to this joint (local)
		glm::vec3 rotation = glm::vec3(0.0f); // Euler (x, y, z) in radians
		glm::vec3 scale = glm::vec3(1.0f);
		std::shared_ptr<Model> model; // optional model attached to this joint
		glm::mat4 localTransform = glm::mat4(1.0f);
		glm::mat4 globalTransform = glm::mat4(1.0f);
		// Per-joint model-space offset so a generic model can be positioned/scaled
		// relative to the joint (e.g. translate half-length then scale for a limb).
		glm::mat4 modelLocalTransform = glm::mat4(1.0f);
	};

	std::vector<Joint> joints;
	std::unordered_map<std::string, int> jointIndex;

	// simple gait params
	float stepFrequency = 2.0f;
	float hipAmplitude = glm::radians(25.0f);
	float kneeAmplitude = glm::radians(40.0f);
	float phaseOffset = glm::pi<float>();

	// Load a set of models and attach them to joints later
	std::shared_ptr<Model> loadModelOrFallback(const std::string &fn) {
		auto m = std::make_shared<Model>();
		if (!fn.empty() && !m->load(fn)) { std::cout << "Warning: failed to load " << fn << " - using fallback\n"; }
		return m;
	}

	// Small helper: add a joint and return its index.
	int addJoint(const std::string &name, int parentIndex, const glm::vec3 &localPos, const glm::vec3 &initialRotation = glm::vec3(0.0f),
	             const glm::vec3 &scale = glm::vec3(1.0f), std::shared_ptr<Model> model = nullptr) {
		Joint j;
		j.name = name;
		j.parentIndex = parentIndex;
		j.localPos = localPos;
		j.rotation = initialRotation;
		j.scale = scale;
		j.model = model;
		int idx = (int)joints.size();
		joints.push_back(j);
		jointIndex[name] = idx;
		return idx;
	}

	// builds a simple biped skeleton (hip->knee->ankle per leg) using provided models.
	// This is a convenience; users can construct arbitrary skeletons by calling addJoint themselves.
	void buildDefaultBiped(const std::shared_ptr<Model> &torsoModel, const std::shared_ptr<Model> &upperModel,
	                       const std::shared_ptr<Model> &lowerModel, float hipOffset = 0.25f, float thighLen = 0.9f, float shinLen = 0.9f) {
		joints.clear();
		jointIndex.clear();

		// root joint at pelvis
		int root = addJoint("pelvis", -1, glm::vec3(0.0f, thighLen + shinLen, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f), torsoModel);

		// left hip -> left knee -> left ankle
		int hipL = addJoint("hip.L", root, glm::vec3(hipOffset, 0.0f, 0.0f));
		int kneeL = addJoint("knee.L", hipL, glm::vec3(0.0f, -thighLen, 0.0f));
		int ankleL = addJoint("ankle.L", kneeL, glm::vec3(0.0f, -shinLen, 0.0f));

		// right hip -> right knee -> right ankle
		int hipR = addJoint("hip.R", root, glm::vec3(-hipOffset, 0.0f, 0.0f));
		int kneeR = addJoint("knee.R", hipR, glm::vec3(0.0f, -thighLen, 0.0f));
		int ankleR = addJoint("ankle.R", kneeR, glm::vec3(0.0f, -shinLen, 0.0f));

		// attach models for limbs (optional)
		if (upperModel) {
			joints[hipL].model = upperModel;
			joints[hipR].model = upperModel;
		}
		if (lowerModel) {
			joints[kneeL].model = lowerModel;
			joints[kneeR].model = lowerModel;
		}

		// set per-joint modelLocalTransform so generic models align like human legs
		auto makeLimbModelTransform = [](float length, float radiusX, float radiusZ) -> glm::mat4 {
			// Move the model origin to mid-limb then scale so the cylinder/capsule model
			// extends from the joint downwards (local -Y)
			glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -length * 0.5f, 0.0f));
			M = M * glm::scale(glm::mat4(1.0f), glm::vec3(radiusX, length, radiusZ));
			return M;
		};

		// Torso: small upward offset and scaling to match human proportions
		if (torsoModel) { joints[root].modelLocalTransform = glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 1.0f, 0.3f)); }

		// Use slimmer radii than the naive defaults to avoid "wide" legs.
		const float upperRadX = 0.14f;
		const float upperRadZ = 0.14f;
		const float lowerRadX = 0.12f;
		const float lowerRadZ = 0.12f;

		if (upperModel) {
			joints[hipL].modelLocalTransform = makeLimbModelTransform(thighLen, upperRadX, upperRadZ);
			joints[hipR].modelLocalTransform = makeLimbModelTransform(thighLen, upperRadX, upperRadZ);
		}
		if (lowerModel) {
			joints[kneeL].modelLocalTransform = makeLimbModelTransform(shinLen, lowerRadX, lowerRadZ);
			joints[kneeR].modelLocalTransform = makeLimbModelTransform(shinLen, lowerRadX, lowerRadZ);
		}
	}

	// Compute FK: fill localTransform and globalTransform for each joint.
	// rootTransform defines world-space transform of the skeleton root.
	void computeForwardKinematics(const glm::mat4 &rootTransform) {
		for (size_t i = 0; i < joints.size(); ++i) {
			// local translation
			glm::mat4 T = glm::translate(glm::mat4(1.0f), joints[i].localPos);
			// rotation order: Y * X * Z (can be changed if needed)
			glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), joints[i].rotation.x, glm::vec3(1, 0, 0));
			glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), joints[i].rotation.y, glm::vec3(0, 1, 0));
			glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), joints[i].rotation.z, glm::vec3(0, 0, 1));
			glm::mat4 S = glm::scale(glm::mat4(1.0f), joints[i].scale);

			// local transform: translate then rotate then scale
			joints[i].localTransform = T * Ry * Rx * Rz * S;

			// global transform
			if (joints[i].parentIndex < 0) {
				joints[i].globalTransform = rootTransform * joints[i].localTransform;
			} else {
				joints[i].globalTransform = joints[joints[i].parentIndex].globalTransform * joints[i].localTransform;
			}
		}
	}

  private:
	// small helper to set shader uniforms and draw a model
	void setAndDraw(Model &model, Shader &sh, RenderController &renderer, const glm::mat4 &M) {
		sh.use();
		sh.set(renderer.U.uModel, M);
		sh.set(renderer.U.uNormal, glm::transpose(glm::inverse(glm::mat3(M))));
		model.draw();
	}

	// update per-frame simple gait by writing rotations to leg joints
	void updateGait(float t) {
		if (jointIndex.empty()) return;
		auto gaitAngles = [&](float time, float phase) -> std::pair<float, float> {
			float w = 2.0f * glm::pi<float>() * stepFrequency;
			float hip = hipAmplitude * std::sin(w * time + phase);
			float swing = std::sin(w * time + phase);
			float knee = (swing > 0.0f) ? (kneeAmplitude * swing) : (kneeAmplitude * 0.15f * std::abs(swing));
			return {hip, knee};
		};

		// left leg (phase = 0)
		if (jointIndex.count("hip.L")) {
			auto [hipA, kneeA] = gaitAngles(t, 0.0f);
			joints[jointIndex["hip.L"]].rotation.x = hipA;
			if (jointIndex.count("knee.L")) joints[jointIndex["knee.L"]].rotation.x = kneeA;
		}
		// right leg (anti-phase)
		if (jointIndex.count("hip.R")) {
			auto [hipA, kneeA] = gaitAngles(t, phaseOffset);
			joints[jointIndex["hip.R"]].rotation.x = hipA;
			if (jointIndex.count("knee.R")) joints[jointIndex["knee.R"]].rotation.x = kneeA;
		}
	}

  public:
	// High-level draw: run gait update, FK, then draw all joint-attached models.
	// rootTransform: world transform for root joint.
	void draw(const glm::mat4 &rootTransform, RenderController &renderer, float t) {
		// update gait-driven joint rotations (if relevant joints exist)
		updateGait(t);

		// compute forward kinematics using provided root transform
		computeForwardKinematics(rootTransform);

		// draw every joint that has a model attached
		Shader &sh = *renderer.shader;
		for (auto &j : joints) {
			if (!j.model) continue;
			// Apply per-joint model-local transform so generic models line up with joint axes
			glm::mat4 M = j.globalTransform * j.modelLocalTransform;
			setAndDraw(*j.model, sh, renderer, M);
		}
	}
};

} // namespace oglprojs
#endif // OGLPROJ2_H