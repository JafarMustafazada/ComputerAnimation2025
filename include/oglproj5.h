#ifndef OGLPROJ5_H
#define OGLPROJ5_H

#include "oglprojs.h"

#include "oglproj4.h"

#include <numeric>

namespace oglprojs {
class PerlinNoise {
  public:
	explicit PerlinNoise(unsigned int seed = 2019) {
		p.resize(256);
		std::iota(p.begin(), p.end(), 0);
		std::mt19937 gen(seed);
		std::shuffle(p.begin(), p.end(), gen);
		// duplicate
		p.insert(p.end(), p.begin(), p.end());
	}

	// 3D Perlin noise in [-1,1]
	double noise(double x, double y, double z) const {
		// find unit cube that contains point
		int X = (int)floor(x) & 255;
		int Y = (int)floor(y) & 255;
		int Z = (int)floor(z) & 255;

		x -= floor(x);
		y -= floor(y);
		z -= floor(z);

		double u = fade(x);
		double v = fade(y);
		double w = fade(z);

		int A = p[X] + Y;
		int AA = p[A] + Z;
		int AB = p[A + 1] + Z;
		int B = p[X + 1] + Y;
		int BA = p[B] + Z;
		int BB = p[B + 1] + Z;

		double res = lerp(w,
		                  lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
		                       lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))),
		                  lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
		                       lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
		// result in [-1,1]
		return res;
	}

	double normalizedNoise(double x, double y, double z, float nF = 0.6f, float nA = 1.0f, float nTS = 0.8f, double max = 1,
	                       double min = 0) {
		double nx = x * nF;
		double ny = y * nF;
		double nz = (z * nTS) * nF;
		double noiseValue = nA * noise(nx, ny, nz);
		double normalized = (noiseValue + 1.0) / 2.0;
		return min + normalized * (max - min);
	}

  private:
	std::vector<int> p;
	static inline double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
	static inline double lerp(double t, double a, double b) { return a + t * (b - a); }
	static double grad(int hash, double x, double y, double z) {
		int h = hash & 15;
		double u = h < 8 ? x : y;
		double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
		return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
	}
};

// Particle data
struct Particle {
	glm::vec3 position{0.0f};
	glm::vec3 velocity{0.0f};
	glm::vec4 color{1.0f};
	float size = 0.1f;
	float life = 0.0f;     // current age
	float lifetime = 1.0f; // max age
	float rotation = 0.0f;
	float rotationSpeed = 0.0f;
	bool alive() const { return life < lifetime; }
};

// Emitter configuration
struct EmitterParams {
	// emission
	float emitRate = 200.0f; // particles per second
	int maxParticles = 2000;
	bool burst = false;
	int burstCount = 200;

	// lifetime/size/color
	float lifetimeMin = 1.0f;
	float lifetimeMax = 3.0f;
	float sizeMin = 0.05f;
	float sizeMax = 0.18f;
	glm::vec4 colorStart = glm::vec4(1.0f, 0.6f, 0.2f, 1.0f);
	glm::vec4 colorEnd = glm::vec4(0.2f, 0.1f, 0.6f, 0.0f);

	// initial velocity
	glm::vec3 velocityMin = glm::vec3(-1.0f, 2.0f, -1.0f);
	glm::vec3 velocityMax = glm::vec3(1.0f, 4.0f, 1.0f);
	float spread = 1.0f; // multiplier for randomness

	// physics
	glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
	float drag = 0.3f; // simple linear drag (0..1, larger more damping)

	// noise / turbulence
	enum NoiseType {
		NONE = 0,
		UNIFORM = 1,
		PERLIN = 2
	} noiseType = PERLIN;
	float noiseFrequency = 0.6f;
	float noiseAmplitude = 1.0f;
	float noiseTimeScale = 0.8f; // animate noise with time
	unsigned int noiseSeed = 1337u;

	// spawn volume
	enum SpawnShape {
		POINT = 0,
		SPHERE = 1,
		BOX = 2
	} spawnShape = SPHERE;
	glm::vec3 boxSize = glm::vec3(0.5f);
	float sphereRadius = 0.2f;

	// world vs local
	bool localSpace = true;

	// collision with physics spheres (turned off)
	bool collideWithPhysics = false;
	float restitution = 0.4f; // bounce when colliding with physics spheres
};

// Particle Emitter
class ParticleEmitter {
  public:
	ParticleEmitter(const EmitterParams &p = EmitterParams()) : params(p), perlin(p.noiseSeed), rng(123456) {
		particles.reserve(params.maxParticles);
		emitAccumulator = 0.0f;
		// pre-allocate pool
		for (int i = 0; i < std::min(64, params.maxParticles); i++) particles.push_back(Particle());
	}

	void setTransform(const glm::mat4 &t) { worldTransform = t; }

	// spawn N immediately (for burst)
	void burst(int N) {
		for (int i = 0; i < N; i++) spawnParticle();
	}

	// step simulation: dt seconds. Optionally can pass pointer to physics engine for collisions,
	void update(float dt, PhysicsEngine *physics = nullptr, float timeNow = 0.0f) {
		if (dt <= 0.0f) return;

		// emit particles (continuous)
		if (!params.burst) {
			float toEmit = params.emitRate * dt + emitAccumulator;
			int count = (int)floor(toEmit);
			emitAccumulator = toEmit - float(count);
			for (int i = 0; i < count; i++) spawnParticle();
		}

		// burst handled externally or by calling burst(), you may call me lazy

		// update particles
		for (auto &pt : particles) {
			if (!pt.alive()) continue;
			pt.life += dt;
			if (!pt.alive()) continue;

			glm::vec3 noiseVec(0.0f);
			if (params.noiseType == EmitterParams::PERLIN) {
				// sample Perlin at particle position + time
				double s = perlin.noise(pt.position.x * params.noiseFrequency, pt.position.y * params.noiseFrequency,
				                        timeNow * params.noiseTimeScale);
				double s2 = perlin.noise((pt.position.x + 37.1) * params.noiseFrequency, (pt.position.y + 17.3) * params.noiseFrequency,
				                         (timeNow + 5.1) * params.noiseTimeScale);
				double s3 = perlin.noise((pt.position.x - 12.7) * params.noiseFrequency, (pt.position.y + 93.4) * params.noiseFrequency,
				                         (timeNow + 11.2) * params.noiseTimeScale);
				noiseVec = glm::vec3((float)s, (float)s2, (float)s3) * params.noiseAmplitude;
			} else if (params.noiseType == EmitterParams::UNIFORM) {
				noiseVec = glm::vec3(randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f)) * params.noiseAmplitude;
			}

			// integrate velocity with gravity + noise
			pt.velocity += (params.gravity + noiseVec) * dt;

			// drag
			pt.velocity *= (1.0f / (1.0f + params.drag * dt));

			// integrate position
			pt.position += pt.velocity * dt;

			// simple collision with physics spheres (bounce)
			if (params.collideWithPhysics && physics) {
				for (auto &b : physics->bodies) {
					glm::vec3 diff = pt.position - b.position;
					float d2 = glm::dot(diff, diff);
					float r2 = (b.radius + pt.size) * (b.radius + pt.size);
					if (d2 < r2 && d2 > 1e-8f) {
						float d = sqrt(d2);
						glm::vec3 n = diff / d;
						// reflect velocity
						float vAlong = glm::dot(pt.velocity, n);
						if (vAlong < 0.0f) { pt.velocity -= (1.0f + params.restitution) * vAlong * n; }
						// push out
						pt.position = b.position + n * (b.radius + pt.size + 1e-3f);
					}
				}
			}
		}

		// remove dead particles lazily (compact)
		if (particles.size() > 0) {
			size_t write = 0;
			for (size_t read = 0; read < particles.size(); read++) {
				if (particles[read].alive()) {
					if (write != read) particles[write] = particles[read];
					write++;
				}
			}
			particles.resize(write);
		}
	}

	// draw particles using a provided render callback. The render callback receives (modelMat, color, size)
	// call Application::renderMesh by passing the model matrix computed for each particle.
	template <typename RenderCallback> void renderAll(RenderCallback renderCb) {
		for (const auto &pt : particles) {
			if (!pt.alive()) continue;
			glm::mat4 model = glm::translate(glm::mat4(1.0f), pt.position) * glm::scale(glm::mat4(1.0f), glm::vec3(pt.size));
			renderCb(model, pt.color, pt.size);
		}
	}

	// Create one particle and push to pool if under max
	void spawnParticle() {
		if ((int)particles.size() >= params.maxParticles) return;

		Particle p;
		p.life = 0.0f;
		// if (params.noiseType == EmitterParams::PERLIN) {
		// 	p.lifetime = perlin.normalizedNoise();
		// 	p.size = perlin.normalizedNoise();
		// } else {
		p.lifetime = randFloat(params.lifetimeMin, params.lifetimeMax);
		p.size = randFloat(params.sizeMin, params.sizeMax);
		// }
		p.color = params.colorStart;

		// spawn position depends on shape
		glm::vec3 localPos(0.0f);
		if (params.spawnShape == EmitterParams::POINT) {
			localPos = glm::vec3(0.0f);
		} else if (params.spawnShape == EmitterParams::SPHERE) {
			// random point in sphere
			glm::vec3 u(randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f));
			float r = pow(randFloat(0.0f, 1.0f), 1.0f / 3.0f) * params.sphereRadius;
			localPos = glm::normalize(u) * r;
		} else { // BOX
			localPos = glm::vec3(randFloat(-0.5f, 0.5f) * params.boxSize.x, randFloat(-0.5f, 0.5f) * params.boxSize.y,
			                     randFloat(-0.5f, 0.5f) * params.boxSize.z);
		}

		// transform into world if needed
		glm::vec3 worldPos = localPos;
		if (params.localSpace) {
			glm::vec4 tp = worldTransform * glm::vec4(localPos, 1.0f);
			worldPos = glm::vec3(tp);
		}

		p.position = worldPos;

		// initial velocity
		glm::vec3 v(randFloat(params.velocityMin.x, params.velocityMax.x), randFloat(params.velocityMin.y, params.velocityMax.y),
		            randFloat(params.velocityMin.z, params.velocityMax.z));
		v *= params.spread;
		p.velocity = v;
		p.rotation = 0.0f;
		p.rotationSpeed = randFloat(-2.0f, 2.0f);
		particles.push_back(p);
	}

	// helper to set color/size over lifetime (call before render)
	void applyMorphs() {
		for (auto &pt : particles) {
			if (!pt.alive()) continue;
			float t = glm::clamp(pt.life / pt.lifetime, 0.0f, 1.0f);
			pt.color = glm::mix(params.colorStart, params.colorEnd, t);
			// pt.size = glm::mix(pt.size, params.sizeMin * 0.1f, t);
			// curves or texture-based ramps can be made, but again you may call me lazy
		}
	}

	EmitterParams params;
	void clear() { particles.clear(); }
	size_t aliveCount() const { return particles.size(); }

  private:
	std::vector<Particle> particles;
	glm::mat4 worldTransform = glm::mat4(1.0f);
	PerlinNoise perlin;
	std::mt19937 rng;
	float emitAccumulator = 0.0f;

	inline float randFloat(float a, float b) {
		std::uniform_real_distribution<float> dist(a, b);
		return dist(rng);
	}
};
} // namespace oglprojs
#endif // OGLPROJ5_H