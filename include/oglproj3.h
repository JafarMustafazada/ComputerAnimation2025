#ifndef OGLPROJ3_H
#define OGLPROJ3_H

#include "oglprojs.h"

namespace oglprojs {
struct RigidBody {
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 angularVelocity = glm::vec3(0.0f);

	float radius = 0.5f;
	float mass = 1.0f;
	float invMass = 1.0f;
	float restitution = 0.5f; // bounciness
	float invInertia = 1.0f;  // inverse scalar inertia (approx for sphere)

	// convenience
	glm::mat4 modelMatrix() const {
		return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(orientation) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
	}

	void finalizeParams() {
		invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
		// Solid sphere inertia: I = 2/5 m r^2 -> inv = 1 / I
		float I = (2.0f / 5.0f) * mass * radius * radius;
		invInertia = (I > 0.0f) ? 1.0f / I : 0.0f;
	}
};

class PhysicsEngine {
  public:
	glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
	std::vector<RigidBody> bodies;

	// basic world plane (y = 0)
	float groundY = 0.0f;
	float defaultRestitution = 0.45f;
	float positionalCorrectionPercent = 0.8f; // for penetration correction
	float positionalCorrectionSlop = 0.01f;

	void addBody(const RigidBody &b) {
		bodies.push_back(b);
		bodies.back().finalizeParams();
	}

	void step(float dt) {
		if (dt <= 0.0f) return;
		// Integrate forces (semi-implicit Euler)
		for (auto &b : bodies) {
			if (b.invMass == 0.0f) continue; // static
			// linear
			b.velocity += gravity * dt;
			// angular: no external torques
			b.position += b.velocity * dt;

			// integrate orientation from angular velocity (small-angle)
			if (glm::dot(b.angularVelocity, b.angularVelocity) > 0.0f) {
				glm::vec3 w = b.angularVelocity;
				// dq/dt = 0.5 * q * (0, w)
				glm::quat wq(0.0f, w.x, w.y, w.z);
				glm::quat dq = 0.5f * (b.orientation * wq);
				b.orientation = glm::normalize(b.orientation + dq * dt);
			}
		}

		// Collision detection & resolution (pairwise sphere-sphere)
		for (size_t i = 0; i < bodies.size(); ++i) {
			for (size_t j = i + 1; j < bodies.size(); ++j) { resolveSphereSphere(bodies[i], bodies[j], dt); }
		}

		// Ground collisions
		for (auto &b : bodies) { resolveGround(b, dt); }
	}

  private:
	void resolveSphereSphere(RigidBody &A, RigidBody &B, float dt) {
		glm::vec3 n = B.position - A.position;
		float dist2 = glm::dot(n, n);
		float rSum = A.radius + B.radius;

		if (dist2 <= 0.0f) return; // coincident centers (ignore)
		float dist = sqrt(dist2);

		// penetration check
		float penetration = rSum - dist;
		if (penetration <= 0.0f) return;

		// contact normal
		n = n / dist;

		// contact point approximate: along normal from A
		glm::vec3 contact = A.position + n * A.radius;
		glm::vec3 ra = contact - A.position;
		glm::vec3 rb = contact - B.position;

		// relative velocity at contact
		glm::vec3 va = A.velocity + glm::cross(A.angularVelocity, ra);
		glm::vec3 vb = B.velocity + glm::cross(B.angularVelocity, rb);
		glm::vec3 rv = vb - va;

		float velAlongNormal = glm::dot(rv, n);
		if (velAlongNormal > 0.0f) {
			// moving apart; still correct penetration if any
			positionalCorrection(A, B, n, penetration);
			return;
		}

		// restitution (use min or product)
		float e = std::min(A.restitution, B.restitution);

		// scalar inverse inertial contribution for spheres (approx)
		float invMassSum = A.invMass + B.invMass;

		// rotational term: use scalar inv inertia approximation
		// term = n · ( ( (invI_A * (ra x n)) x ra ) + ( (invI_B * (rb x n)) x rb ) )
		glm::vec3 ra_x_n = glm::cross(ra, n);
		glm::vec3 rb_x_n = glm::cross(rb, n);

		glm::vec3 rotA = glm::cross(A.invInertia * ra_x_n, ra);
		glm::vec3 rotB = glm::cross(B.invInertia * rb_x_n, rb);
		float rotTerm = glm::dot(n, rotA + rotB);

		float jDen = invMassSum + rotTerm;
		if (jDen == 0.0f) return;

		float j = -(1.0f + e) * velAlongNormal;
		j /= jDen;

		// apply impulse
		glm::vec3 impulse = j * n;
		A.velocity -= impulse * A.invMass;
		B.velocity += impulse * B.invMass;

		// angular velocity change
		A.angularVelocity -= A.invInertia * glm::cross(ra, impulse);
		B.angularVelocity += B.invInertia * glm::cross(rb, impulse);

		// positional correction to avoid sinking
		positionalCorrection(A, B, n, penetration);
	}

	void positionalCorrection(RigidBody &A, RigidBody &B, const glm::vec3 &normal, float penetration) {
		float invMassSum = A.invMass + B.invMass;
		if (invMassSum == 0.0f) return;
		float correctionMagnitude = std::max(penetration - positionalCorrectionSlop, 0.0f) / invMassSum * positionalCorrectionPercent;
		glm::vec3 correction = correctionMagnitude * normal;
		A.position -= correction * A.invMass;
		B.position += correction * B.invMass;
	}

	void resolveGround(RigidBody &b, float dt) {
		float bottom = b.position.y - b.radius;
		if (bottom < groundY) {
			// penetration
			float penetration = groundY - bottom;
			// move object up
			b.position.y += penetration + positionalCorrectionSlop;

			// compute contact point and relative velocity
			glm::vec3 n = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 contact = b.position - glm::vec3(0.0f, b.radius, 0.0f);
			glm::vec3 ra = contact - b.position;

			glm::vec3 rv = b.velocity + glm::cross(b.angularVelocity, ra);
			float velAlongNormal = glm::dot(rv, n);

			if (velAlongNormal < 0.0f) {
				float e = b.restitution * defaultRestitution;
				float j = -(1.0f + e) * velAlongNormal;
				float jDen = b.invMass + glm::dot(n, glm::cross(b.invInertia * glm::cross(ra, n), ra));
				if (jDen == 0.0f) return;
				j /= jDen;
				glm::vec3 impulse = j * n;

				b.velocity += impulse * b.invMass;
				b.angularVelocity += b.invInertia * glm::cross(ra, impulse);
			}

			// simple damping to simulate friction when resting
			b.velocity.x *= 0.98f;
			b.velocity.z *= 0.98f;
		}
	}
};
} // namespace oglprojs
#endif // OGLPROJ3_H