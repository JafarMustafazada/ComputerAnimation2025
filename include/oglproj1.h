#ifndef OGLPROJ1_H
#define OGLPROJ1_H

#include "oglprojs.h"

#include <glad/glad.h>

#include <GLFW/glfw3.h>

namespace oglprojs {
// Motion controller
enum OrientationType {
	Quaternion,
	Euler
};
enum InterpType {
	CatmullRom,
	BSpline
};

struct Keyframe {
	glm::vec3 position;
	glm::vec3 euler; // Euler angles in radians
	glm::quat quat;  // Quaternion
};

class MotionController {
  public:
	std::vector<Keyframe> keys;
	OrientationType orientType = OrientationType::Quaternion;
	InterpType interpType = InterpType::CatmullRom;

	// Add a keyframe (Euler or Quaternion)
	void addKey(const glm::vec3 &pos, const glm::vec3 &euler) {
		Keyframe k;
		k.position = pos;
		k.euler = euler;
		k.quat = glm::quat(euler);
		keys.push_back(k);
	}
	void addKey(const glm::vec3 &pos, const glm::quat &quat) {
		Keyframe k;
		k.position = pos;
		k.quat = quat;
		k.euler = glm::eulerAngles(quat);
		keys.push_back(k);
	}

	glm::vec3 getPosition(int i, int &maxVal) const {
		int clampedIndex = glm::clamp(i, 0, maxVal);
		return keys[clampedIndex].position;
	};

	glm::vec3 interpolateCatmullRom(glm::vec3 &p0, glm::vec3 &p1, glm::vec3 &p2, glm::vec3 &p3, float t) const {
		glm::vec3 a = 2.0f * p1;
		glm::vec3 b = -p0 + p2;
		glm::vec3 c = 2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3;
		glm::vec3 d = -p0 + 3.0f * p1 - 3.0f * p2 + p3;

		// Applied Horner's method
		return 0.5f * (a + t * (b + t * (c + t * d)));
	}

	glm::vec3 interpolateBSpline(glm::vec3 &p0, glm::vec3 &p1, glm::vec3 &p2, glm::vec3 &p3, float t) const {
		float t2 = t * t;
		float t3 = t2 * t;
		float oneMinusT = 1.0f - t;

		float basis0 = oneMinusT * oneMinusT * oneMinusT / 6.0f;
		float basis1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
		float basis2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
		float basis3 = t3 / 6.0f;

		return basis0 * p0 + basis1 * p1 + basis2 * p2 + basis3 * p3;
	}

	glm::vec3 interpPos(float t) const {
		int numKeys = keys.size();
		if (numKeys == 0) return glm::vec3(0);
		if (numKeys == 1) return keys[0].position;

		// Map t from [0,1] to segment range
		int maxVal = numKeys - 1;
		float segmentFloat = t * maxVal;
		int segmentIndex = glm::clamp(int(segmentFloat), 0, numKeys - 2);
		float localT = segmentFloat - segmentIndex;

		// Get control points for spline
		glm::vec3 p0 = getPosition(segmentIndex - 1, maxVal);
		glm::vec3 p1 = getPosition(segmentIndex, maxVal);
		glm::vec3 p2 = getPosition(segmentIndex + 1, maxVal);
		glm::vec3 p3 = getPosition(segmentIndex + 2, maxVal);

		if (interpType == InterpType::CatmullRom) return interpolateCatmullRom(p0, p1, p2, p3, localT);
		else return interpolateBSpline(p0, p1, p2, p3, localT);
	}

	// Interpolate orientation
	glm::quat interpQuat(float t) const {
		int numKeys = keys.size();
		if (numKeys == 0) return glm::quat();
		if (numKeys == 1) return keys[0].quat;

		float segmentFloat = t * (numKeys - 1);
		int segmentIndex = glm::clamp(int(segmentFloat), 0, numKeys - 2);
		float localT = segmentFloat - segmentIndex;

		if (orientType == OrientationType::Quaternion) {
			const glm::quat &startQuat = keys[segmentIndex].quat;
			const glm::quat &endQuat = keys[segmentIndex + 1].quat;
			return glm::slerp(startQuat, endQuat, localT);
		} else {
			const glm::vec3 &startEuler = keys[segmentIndex].euler;
			const glm::vec3 &endEuler = keys[segmentIndex + 1].euler;
			glm::vec3 interpolatedEuler = glm::mix(startEuler, endEuler, localT);
			return glm::quat(interpolatedEuler);
		}
	}

	// Get transform matrix at t (0..1)
	glm::mat4 getTransform(float t) const {
		glm::vec3 pos = interpPos(t);
		glm::quat rot = interpQuat(t);
		glm::mat4 T = glm::translate(glm::mat4(1), pos);
		glm::mat4 R = glm::mat4_cast(rot);
		return T * R;
	}
};
} // namespace oglprojs
#endif // OGLPROJ1_H