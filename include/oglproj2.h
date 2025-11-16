#ifndef OGLPROJ2_H
#define OGLPROJ2_H

#include "oglprojs.h"

#include "oglproj1.h"

namespace oglprojs {

// ============================================================================
// ArticulatedFigure (Forward Kinematics)
// ============================================================================

class ArticulatedFigure {
	std::shared_ptr<MotionController> baseMotion;

  public:
	// lengths
	float thighLength = 0.9f;
	float shinLength = 0.9f;

	// hip offsets from torso origin (left/right)
	glm::vec3 leftHipOffset = glm::vec3(-0.28f, -0.4f, 0.0f);
	glm::vec3 rightHipOffset = glm::vec3(0.28f, -0.4f, 0.0f);

	// gait params
	float stepFreq = 1.0f;                     // steps per second
	float hipAmplitude = glm::radians(30.0f);  // hip swing amplitude
	float kneeAmplitude = glm::radians(40.0f); // knee bending amplitude

	ArticulatedFigure(std::shared_ptr<MotionController> base) : baseMotion(base) {}

	// returns matrices in order: torso, left_thigh, left_shin, right_thigh, right_shin
	void evaluateBones(float time, OrientationType ot, InterpType it, glm::mat4 *outTransforms) {
		Transform rootT = baseMotion->evaluate(time, ot, it);
		glm::mat4 rootMat = rootT.toMatrix();
		outTransforms[0] = rootMat; // torso

		// Gait phase
		float phase = 2.0f * glm::pi<float>() * stepFreq * time;

		float leftHipAngle = std::sin(phase) * hipAmplitude;
		float rightHipAngle = std::sin(phase + glm::pi<float>()) * hipAmplitude;

		float leftKneeAngle = std::abs(std::sin(phase)) * kneeAmplitude;
		float rightKneeAngle = std::abs(std::sin(phase + glm::pi<float>())) * kneeAmplitude;

		// LEFT thigh (hip joint)
		glm::mat4 leftHipLocal =
		    glm::translate(glm::mat4(1.0f), leftHipOffset) * glm::rotate(glm::mat4(1.0f), leftHipAngle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 leftThighWorld = rootMat * leftHipLocal;
		outTransforms[1] = leftThighWorld * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength * 0.5f, 0.0f));

		// LEFT shin (knee -> shin)
		glm::mat4 leftKneeLocal = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength, 0.0f)) *
		                          glm::rotate(glm::mat4(1.0f), leftKneeAngle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 leftShinWorld = rootMat * leftHipLocal * leftKneeLocal;
		outTransforms[2] = leftShinWorld * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -shinLength * 0.5f, 0.0f));

		// RIGHT thigh
		glm::mat4 rightHipLocal =
		    glm::translate(glm::mat4(1.0f), rightHipOffset) * glm::rotate(glm::mat4(1.0f), rightHipAngle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rightThighWorld = rootMat * rightHipLocal;
		outTransforms[3] = rightThighWorld * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength * 0.5f, 0.0f));

		// RIGHT shin
		glm::mat4 rightKneeLocal = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -thighLength, 0.0f)) *
		                           glm::rotate(glm::mat4(1.0f), rightKneeAngle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rightShinWorld = rootMat * rightHipLocal * rightKneeLocal;
		outTransforms[4] = rightShinWorld * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -shinLength * 0.5f, 0.0f));
	}
};
} // namespace oglprojs
#endif // OGLPROJ2_H