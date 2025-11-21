#ifndef OGLPROJ4_H
#define OGLPROJ4_H

#include "oglprojs.h"

#include "oglproj3.h"

#include <random>

namespace oglprojs {
struct Boid {
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 acceleration = glm::vec3(0.0f);

	float radius = 0.08f;  // visual radius
	float maxSpeed = 3.0f; // units per second
	float maxForce = 6.0f; // steering accel magnitude
};

class Flock {
	// Helpers
	static glm::vec3 limitMagnitude(const glm::vec3 &v, float maxMag) {
		float len2 = glm::dot(v, v);
		if (len2 > maxMag * maxMag) return glm::normalize(v) * maxMag;
		return v;
	}
	static glm::vec3 setMagnitude(const glm::vec3 &v, float mag) {
		float l = glm::length(v);
		if (l < 1e-6f) return glm::vec3(0.0f);
		return v * (mag / l);
	}

	// RNG for wander
	std::mt19937 rng;
	std::uniform_real_distribution<float> uni;

  public:
	std::vector<Boid> boids;

	// neighbor / perception
	float neighborRadius = 1.0f;
	float separationRadius = 0.35f;

	// behavior weights
	float wSeparation = 1.6f;
	float wAlignment = 1.0f;
	float wCohesion = 1.0f;
	float wWander = 0.25f;
	float wAvoid = 2.5f; // obstacle avoidance weight

	// wander parameters
	float wanderJitter = 0.8f; // radians/sec jitter
	float wanderRadius = 0.5f;

	// world confinement
	float worldRadius = 8.0f;
	float centerPull = 1.0f; // steer toward center when far

	Flock(int N = 32, unsigned int seed = 1234) : uni(-1.0f, 1.0f) {
		rng.seed(seed);
		boids.resize(N);
		for (int i = 0; i < N; ++i) {
			boids[i].position = glm::vec3((uni(rng) * worldRadius * 0.5f),
			                              (uni(rng) * 1.0f + 1.0f), // slightly above ground
			                              (uni(rng) * worldRadius * 0.5f));
			boids[i].velocity = glm::normalize(glm::vec3(uni(rng), uni(rng) * 0.2f, uni(rng))) * (boids[i].maxSpeed * 0.5f);
			boids[i].maxSpeed = 2.0f + (uni(rng) + 1.0f) * 1.0f;
			boids[i].maxForce = 4.0f;
		}
	}

	// Update flock: dt in seconds. If physicsEngine != nullptr, boids will avoid physics bodies as obstacles.
	void update(float dt, PhysicsEngine *physicsEngine = nullptr) {
		if (boids.empty()) return;

		// compute behaviors for each boid
		std::vector<glm::vec3> steering(boids.size(), glm::vec3(0.0f));

		for (size_t i = 0; i < boids.size(); ++i) {
			Boid &b = boids[i];

			// accumulate neighbors
			glm::vec3 posSum(0.0f), velSum(0.0f);
			glm::vec3 sepForce(0.0f);
			int countNeighbors = 0;
			int countSeparation = 0;

			for (size_t j = 0; j < boids.size(); ++j) {
				if (i == j) continue;
				Boid &o = boids[j];
				glm::vec3 diff = o.position - b.position;
				float dist2 = glm::dot(diff, diff);
				float neighR2 = neighborRadius * neighborRadius;
				if (dist2 < neighR2) {
					++countNeighbors;
					posSum += o.position;
					velSum += o.velocity;
				}
				float sepR2 = separationRadius * separationRadius;
				if (dist2 < sepR2 && dist2 > 0.00001f) {
					// repulsive vector (away from neighbor), scaled by inverse distance
					glm::vec3 away = b.position - o.position;
					float invDist = 1.0f / sqrt(dist2);
					sepForce += glm::normalize(away) * invDist;
					++countSeparation;
				}
			}

			// Separation
			glm::vec3 separation(0.0f);
			if (countSeparation > 0) {
				separation = sepForce / float(countSeparation);
				separation = setMagnitude(separation, b.maxSpeed);
				separation -= b.velocity;
				separation = limitMagnitude(separation, b.maxForce);
			}

			// Alignment
			glm::vec3 alignment(0.0f);
			if (countNeighbors > 0) {
				glm::vec3 avgV = velSum / float(countNeighbors);
				avgV = setMagnitude(avgV, b.maxSpeed);
				alignment = avgV - b.velocity;
				alignment = limitMagnitude(alignment, b.maxForce);
			}

			// Cohesion
			glm::vec3 cohesion(0.0f);
			if (countNeighbors > 0) {
				glm::vec3 center = posSum / float(countNeighbors);
				glm::vec3 desired = center - b.position;
				desired = setMagnitude(desired, b.maxSpeed);
				cohesion = desired - b.velocity;
				cohesion = limitMagnitude(cohesion, b.maxForce);
			}

			// Wander: small randomized steering to break symmetry
			glm::vec3 wander = glm::vec3(uni(rng), uni(rng), uni(rng));
			wander = glm::normalize(wander) * wanderRadius;
			wander = setMagnitude(wander, b.maxSpeed) - b.velocity;
			wander = limitMagnitude(wander, b.maxForce) * (wanderJitter * dt);

			// Obstacle avoidance using physics spheres (optional)
			glm::vec3 avoid(0.0f);
			if (physicsEngine) {
				for (const auto &ob : physicsEngine->bodies) {
					// treat static or dynamic spheres as obstacles
					float combined = ob.radius + b.radius + 0.2f; // safe margin
					glm::vec3 diff = ob.position - b.position;
					float d2 = glm::dot(diff, diff);
					if (d2 < combined * combined && d2 > 0.0001f) {
						float d = sqrt(d2);
						glm::vec3 away = b.position - ob.position;
						avoid += glm::normalize(away) * ((combined - d) / combined);
					}
				}
				if (glm::dot(avoid, avoid) > 0.0f) {
					avoid = setMagnitude(avoid, b.maxSpeed);
					avoid -= b.velocity;
					avoid = limitMagnitude(avoid, b.maxForce);
				}
			}

			// Keep inside world radius: steer toward center when outside
			glm::vec3 centerSteer(0.0f);
			float distCenter2 = glm::dot(b.position, b.position);
			if (distCenter2 > (worldRadius * worldRadius)) {
				glm::vec3 toCenter = -b.position;
				toCenter = setMagnitude(toCenter, b.maxSpeed);
				centerSteer = toCenter - b.velocity;
				centerSteer = limitMagnitude(centerSteer, b.maxForce) * centerPull;
			}

			// Weighted sum
			glm::vec3 total =
			    wSeparation * separation + wAlignment * alignment + wCohesion * cohesion + wWander * wander + wAvoid * avoid + centerSteer;
			steering[i] = total;
		}

		// Apply steering and integrate
		for (size_t i = 0; i < boids.size(); ++i) {
			Boid &b = boids[i];
			glm::vec3 acc = steering[i];
			// limit acceleration (maxForce is per-boid)
			if (glm::dot(acc, acc) > 0.0f) acc = limitMagnitude(acc, b.maxForce);
			b.velocity += acc * dt;

			// limit speed
			b.velocity = limitMagnitude(b.velocity, b.maxSpeed);
			b.position += b.velocity * dt;

			// simple collision with ground (y = 0)
			if (b.position.y < 0.05f) b.position.y = 0.05f;

			// reset acceleration for next frame
			b.acceleration = glm::vec3(0.0f);
		}
	}
};
} // namespace oglprojs
#endif // OGLPROJ4_H