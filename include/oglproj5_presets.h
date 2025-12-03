#ifndef OGLPROJ5_presets_H
#define OGLPROJ5_presets_H

#include "oglprojs.h"

#include "oglproj5.h"

namespace oglprojs {
struct EmitterConfigurator {
	EmitterParams params;

	EmitterConfigurator &withEmitRate(float v) {
		params.emitRate = v;
		return *this;
	}
	EmitterConfigurator &withMaxParticles(int v) {
		params.maxParticles = v;
		return *this;
	}
	EmitterConfigurator &withBurst(bool on, int count = 0) {
		params.burst = on;
		params.burstCount = count;
		return *this;
	}

	EmitterConfigurator &withLifetime(float minv, float maxv) {
		params.lifetimeMin = minv;
		params.lifetimeMax = maxv;
		return *this;
	}
	EmitterConfigurator &withSizeRange(float minv, float maxv) {
		params.sizeMin = minv;
		params.sizeMax = maxv;
		return *this;
	}
	EmitterConfigurator &withColorRange(const glm::vec4 &start, const glm::vec4 &end) {
		params.colorStart = start;
		params.colorEnd = end;
		return *this;
	}

	EmitterConfigurator &withVelocityRange(const glm::vec3 &vmin, const glm::vec3 &vmax) {
		params.velocityMin = vmin;
		params.velocityMax = vmax;
		return *this;
	}
	EmitterConfigurator &withSpread(float s) {
		params.spread = s;
		return *this;
	}

	EmitterConfigurator &withGravity(const glm::vec3 &g) {
		params.gravity = g;
		return *this;
	}
	EmitterConfigurator &withDrag(float d) {
		params.drag = d;
		return *this;
	}

	EmitterConfigurator &withNoiseType(int noiseType) {
		params.noiseType = static_cast<EmitterParams::NoiseType>(noiseType);
		return *this;
	}
	EmitterConfigurator &withNoiseParams(float freq, float amp, float tscale, unsigned int seed) {
		params.noiseFrequency = freq;
		params.noiseAmplitude = amp;
		params.noiseTimeScale = tscale;
		params.noiseSeed = seed;
		return *this;
	}

	EmitterConfigurator &withSpawnPoint() {
		params.spawnShape = EmitterParams::POINT;
		return *this;
	}
	EmitterConfigurator &withSpawnSphere(float radius) {
		params.spawnShape = EmitterParams::SPHERE;
		params.sphereRadius = radius;
		return *this;
	}
	EmitterConfigurator &withSpawnBox(const glm::vec3 &size) {
		params.spawnShape = EmitterParams::BOX;
		params.boxSize = size;
		return *this;
	}

	EmitterConfigurator &withLocalSpace(bool local) {
		params.localSpace = local;
		return *this;
	}

	EmitterConfigurator &withCollisions(bool collide, float restitution = 0.4f) {
		params.collideWithPhysics = collide;
		params.restitution = restitution;
		return *this;
	}

	// shortcuts for color convenience (RGB + optional alpha)
	static glm::vec4 rgba(float r, float g, float b, float a = 1.0f) { return glm::vec4(r, g, b, a); }

	// Apply the current params to an existing ParticleEmitter.
	// This overwrites emitter.
	void applyTo(ParticleEmitter &emitter, bool resetParticles = true) const {
		emitter.params = params;
		if (resetParticles) emitter.clear();
		if (params.burst && params.burstCount > 0) emitter.burst(params.burstCount);
	}

	// Presets
	enum Preset {
		Default = 0,
		Fire,
		Fire_Long,
		Smoke,
		Fountain,
		Snow,
		Steam,
		Plasma
	};

	// return an EmitterConfigurator preloaded with chosen preset
	static EmitterConfigurator preset(Preset p) {
		EmitterConfigurator cfg;
		switch (p) {
		case Default: cfg.params = EmitterParams(); break;

		case Fire:
			cfg.withEmitRate(2200.0f)
			    .withMaxParticles(5500)
			    .withLifetime(0.35f, 1.0f)                                                     // short-lived
			    .withSizeRange(0.02f, 0.10f)                                                   // small to medium
			    .withColorRange(EmitterConfigurator::rgba(1.0f, 1.0f, 0.88f, 1.0f),            // start (bright white/yellow)
			                    EmitterConfigurator::rgba(0.35f, 0.03f, 0.02f, 0.0f))          // end (dark red, transparent)
			    .withVelocityRange(glm::vec3(-0.6f, 1.8f, -0.6f), glm::vec3(0.6f, 5.2f, 0.6f)) // strong upward velocity
			    .withSpread(1.0f)
			    .withGravity(glm::vec3(0.0f, 5.5f, 0.0f)) // buoyancy upward
			    .withDrag(0.18f)                          // low drag so particles rise quickly
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(2.4f /*freq*/, 3.6f /*amp*/, 1.1f /*timeScale*/, 424242u /*seed*/)
			    .withSpawnSphere(0.06f)
			    .withLocalSpace(true)
			    .withCollisions(false, 0.0f);
			break;

		case Fire_Long:
			cfg.withEmitRate(140.0f)
			    .withMaxParticles(1200)
			    .withLifetime(1.6f, 4.0f) // longer-lived
			    .withSizeRange(0.03f, 0.16f)
			    .withColorRange(EmitterConfigurator::rgba(1.0f, 0.55f, 0.12f, 1.0f),  // bright ember
			                    EmitterConfigurator::rgba(0.12f, 0.04f, 0.01f, 0.0f)) // dark ash -> transparent
			    .withVelocityRange(glm::vec3(-0.6f, 0.8f, -0.6f), glm::vec3(0.6f, 2.2f, 0.6f))
			    .withGravity(glm::vec3(0.0f, -1.8f, 0.0f)) // embers eventually fall (negative Y)
			    .withDrag(0.65f)
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(0.9f /*freq*/, 1.2f /*amp*/, 0.6f /*timeScale*/, 777777u /*seed*/)
			    .withSpawnSphere(0.03f)
			    .withLocalSpace(false)        // embers can fall independently in world space
			    .withCollisions(true, 0.25f);
			break;

		case Smoke:
			cfg.withEmitRate(350.0f)
			    .withMaxParticles(2000)
			    .withLifetime(2.0f, 5.0f)
			    .withSizeRange(0.12f, 0.6f)
			    .withColorRange(rgba(0.2f, 0.2f, 0.2f, 0.8f), rgba(0.05f, 0.05f, 0.05f, 0.0f))
			    .withVelocityRange(glm::vec3(-0.3f, 0.3f, -0.3f), glm::vec3(0.3f, 1.5f, 0.3f))
			    .withGravity(glm::vec3(0.0f, 1.0f, 0.0f)) // slight upward buoyancy
			    .withDrag(0.6f)
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(0.6f, 1.0f, 0.4f, 2222u)
			    .withSpawnBox(glm::vec3(0.3f, 0.1f, 0.3f))
			    .withLocalSpace(true);
			break;

		case Fountain:
			cfg.withEmitRate(800.0f)
			    .withMaxParticles(2500)
			    .withLifetime(1.2f, 2.2f)
			    .withSizeRange(0.03f, 0.06f)
			    .withColorRange(rgba(0.7f, 0.85f, 1.0f, 1.0f), rgba(0.2f, 0.3f, 0.45f, 0.0f))
			    .withVelocityRange(glm::vec3(-1.0f, 6.0f, -1.0f), glm::vec3(1.0f, 9.0f, 1.0f))
			    .withGravity(glm::vec3(0.0f, -9.81f, 0.0f))
			    .withDrag(0.1f)
			    .withNoiseType(EmitterParams::UNIFORM)
			    .withNoiseParams(1.0f, 0.2f, 1.0f, 0u)
			    .withSpawnSphere(0.05f)
			    .withLocalSpace(true)
			    .withCollisions(true, 0.35f);
			break;

		case Snow:
			cfg.withEmitRate(600.0f)
			    .withMaxParticles(4500)
			    .withLifetime(4.0f, 10.0f)
			    .withSizeRange(0.02f, 0.06f)
			    .withColorRange(rgba(1.0f, 1.0f, 1.0f, 1.0f), rgba(1.0f, 1.0f, 1.0f, 0.0f))
			    .withVelocityRange(glm::vec3(-0.2f, -0.1f, -0.2f), glm::vec3(0.2f, -0.5f, 0.2f))
			    .withGravity(glm::vec3(0.0f, -0.6f, 0.0f))
			    .withDrag(0.9f)
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(0.3f, 0.5f, 0.6f, 2024u)
			    .withSpawnBox(glm::vec3(6.0f, 0.2f, 6.0f))
			    .withLocalSpace(false)
			    .withCollisions(false);
			break;

		case Steam:
			cfg.withEmitRate(420.0f)
			    .withMaxParticles(2000)
			    .withLifetime(1.6f, 3.6f)
			    .withSizeRange(0.08f, 0.25f)
			    .withColorRange(rgba(0.9f, 0.9f, 0.9f, 0.9f), rgba(0.9f, 0.9f, 0.9f, 0.0f))
			    .withVelocityRange(glm::vec3(-0.2f, 0.6f, -0.2f), glm::vec3(0.2f, 2.0f, 0.2f))
			    .withGravity(glm::vec3(0.0f, 0.6f, 0.0f))
			    .withDrag(0.5f)
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(0.5f, 1.0f, 0.8f, 555u)
			    .withSpawnSphere(0.06f)
			    .withLocalSpace(true);
			break;

		case Plasma:
			cfg.withEmitRate(1200.f)
			    .withMaxParticles(4000)
			    .withLifetime(0.5f, 1.4f)
			    .withSizeRange(0.01f, 0.06f)
			    .withColorRange(rgba(0.3f, 0.8f, 1.0f, 1.0f), rgba(0.05f, 0.01f, 0.2f, 0.0f))
			    .withVelocityRange(glm::vec3(-1.6f, -0.2f, -1.6f), glm::vec3(1.6f, 1.5f, 1.6f))
			    .withGravity(glm::vec3(0.0f, -1.2f, 0.0f))
			    .withDrag(0.2f)
			    .withNoiseType(EmitterParams::PERLIN)
			    .withNoiseParams(1.4f, 3.2f, 1.2f, 31337u)
			    .withSpawnSphere(0.08f)
			    .withLocalSpace(false)
			    .withCollisions(true, 0.5f);
			break;
		}
		return cfg;
	}
};
} // namespace oglprojs
#endif // OGLPROJ5_presets_H