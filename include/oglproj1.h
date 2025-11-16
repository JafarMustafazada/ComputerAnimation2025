#ifndef OGLPROJ1_H
#define OGLPROJ1_H

#include "oglprojs.h"

namespace oglprojs {

// ============================================================================
// Core Types
// ============================================================================

enum class OrientationType {
	Quaternion,
	Euler
};
enum class InterpType {
	CatmullRom,
	BSpline
};

struct Transform {
	glm::vec3 position{0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
	glm::vec3 scale{1.0f};

	Transform() = default;
	Transform(const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scl = glm::vec3(1.0f))
	    : position(pos), rotation(rot), scale(scl) {}

	glm::mat4 toMatrix() const {
		return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}
};

struct Keyframe {
	float time;
	Transform transform;

	Keyframe(float t, const Transform &trans) : time(t), transform(trans) {}
};

// ============================================================================
// Motion Controller
// ============================================================================

class MotionController {
	std::vector<Keyframe> keyframes;
	float prevTime = 0.0f;

	static glm::vec3 catmullRom(const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3, float t) {
		const float t2 = t * t, t3 = t2 * t;
		return 0.5f *
		       ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	static glm::vec3 bSpline(const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3, float t) {
		const float t2 = t * t, t3 = t2 * t;
		const float b0 = (1.0f - 3.0f * t + 3.0f * t2 - t3) / 6.0f;
		const float b1 = (4.0f - 6.0f * t2 + 3.0f * t3) / 6.0f;
		const float b2 = (1.0f + 3.0f * t + 3.0f * t2 - 3.0f * t3) / 6.0f;
		const float b3 = t3 / 6.0f;
		return b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3;
	}

	glm::vec3 interpolatePosition(int i, float t, InterpType type) const {
		const int n = keyframes.size();
		const glm::vec3 p0 = keyframes[std::max(0, i - 1)].transform.position;
		const glm::vec3 p1 = keyframes[i].transform.position;
		const glm::vec3 p2 = keyframes[i + 1].transform.position;
		const glm::vec3 p3 = keyframes[std::min(n - 1, i + 2)].transform.position;

		return (type == InterpType::CatmullRom) ? catmullRom(p0, p1, p2, p3, t) : bSpline(p0, p1, p2, p3, t);
	}

	glm::quat interpolateRotation(int i, float t, OrientationType type) const {
		if (type == OrientationType::Quaternion) {
			return glm::slerp(keyframes[i].transform.rotation, keyframes[i + 1].transform.rotation, t);
		} else {
			const glm::vec3 e1 = glm::eulerAngles(keyframes[i].transform.rotation);
			const glm::vec3 e2 = glm::eulerAngles(keyframes[i + 1].transform.rotation);
			return glm::quat(glm::mix(e1, e2, t));
		}
	}

  public:
	void addKeyframe(float time, const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scl = glm::vec3(1.0f)) {
		if (time <= prevTime) time = prevTime + 0.001f;
		prevTime = time;
		keyframes.push_back({time, {pos, glm::normalize(rot), scl}});
		std::sort(keyframes.begin(), keyframes.end(), [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
	}

	void addKeyframe(const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scl = glm::vec3(1.0f)) {
		prevTime += 1.0f;
		keyframes.push_back({prevTime, {pos, glm::normalize(rot), scl}});
		std::sort(keyframes.begin(), keyframes.end(), [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
	}

	void addKeyframe(float time, const glm::vec3 &pos, const glm::vec3 &euler) { addKeyframe(time, pos, glm::quat(euler)); }
	void addKeyframe(const glm::vec3 &pos, const glm::vec3 &euler) { addKeyframe(pos, glm::quat(euler)); }

	float totalDuration() const {
		if (keyframes.empty()) return 0.0f;
		return keyframes.back().time - keyframes.front().time;
	}

	Transform evaluate(float t, OrientationType orientType, InterpType interpType) const {
		if (keyframes.empty()) return Transform();
		if (keyframes.size() == 1) return keyframes[0].transform;

		// Find segment
		int i = 0;
		for (; i < keyframes.size() - 1; i++) {
			if (t <= keyframes[i + 1].time) break;
		}

		const float t0 = keyframes[i].time;
		const float t1 = keyframes[i + 1].time;
		const float localT = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;

		Transform result;
		result.position = interpolatePosition(i, localT, interpType);
		result.rotation = interpolateRotation(i, localT, orientType);
		result.scale = glm::mix(keyframes[i].transform.scale, keyframes[i + 1].transform.scale, localT);
		return result;
	}
};
} // namespace oglprojs
#endif // OGLPROJ1_H